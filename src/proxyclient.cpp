#include "proxyclient.h"
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslError>
#include <QFile>
#include <QUrl>
#include <QDebug>
#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDateTime>

ProxyClient::ProxyClient(QObject *parent)
    : QObject(parent)
    , sslSocket(new QSslSocket(this))
    , connectionTimer(new QTimer(this))
    , proxyPort(8080)
    , targetPort(443)
    , connecting(false)
    , stage_(Stage::ProxyTlsHandshake)
{
    // 连接SSL Socket信号
    connect(sslSocket, &QSslSocket::connected,
            this, &ProxyClient::handleSslSocketConnected);
    connect(sslSocket, &QSslSocket::encrypted,
            this, &ProxyClient::handleSslSocketEncrypted);
    connect(sslSocket, &QSslSocket::disconnected, 
            this, &ProxyClient::handleSslSocketDisconnected);
    connect(sslSocket, &QSslSocket::errorOccurred,
            this, &ProxyClient::handleSslSocketError);
    connect(sslSocket, &QSslSocket::sslErrors, 
            this, &ProxyClient::handleSslErrors);
    connect(sslSocket, &QSslSocket::readyRead, 
            this, &ProxyClient::handleSslSocketReadyRead);
    
    // 连接超时定时器
    connect(connectionTimer, &QTimer::timeout, 
            this, &ProxyClient::handleConnectionTimeout);
    connectionTimer->setSingleShot(true);
}

ProxyClient::~ProxyClient()
{
    if (sslSocket) {
        sslSocket->disconnectFromHost();
    }
    if (connectionTimer) {
        connectionTimer->stop();
    }
}

void ProxyClient::setProxySettings(const QString &host, int port,
                                  const QString &username, const QString &password)
{
    proxyHost = host;
    proxyPort = port;
    proxyUsername = username;
    proxyPassword = password;
}

void ProxyClient::setSslCertificate(const QString &certPath)
{
    certificatePath = certPath;
}

void ProxyClient::addDebugMessage(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString fullMessage = QString("[%1] %2").arg(timestamp).arg(message);
    debugMessages.append(fullMessage);
    qDebug() << message;
    
    // 实时发送调试信息到GUI
    emit debugMessage(fullMessage);
}

void ProxyClient::connectToUrl(const QString &url)
{
    // 检查是否已经在连接
    if (connecting) {
        emit networkError("正在连接中，请等待当前请求完成");
        return;
    }
    
    // 验证输入
    if (url.isEmpty()) {
        emit networkError("请输入目标网址");
        return;
    }
    
    if (proxyHost.isEmpty()) {
        emit networkError("请输入代理主机地址");
        return;
    }
    
    if (proxyPort <= 0) {
        emit networkError("请输入有效的代理端口");
        return;
    }
    
    // 解析目标URL
    QUrl targetUrlObj(url);
    if (!targetUrlObj.isValid()) {
        emit networkError("无效的目标URL");
        return;
    }
    
    targetUrl = url;
    targetHost = targetUrlObj.host();
    targetPort = targetUrlObj.port(443); // 默认443端口
    
    // 重置状态
    connecting = true;
    stage_ = Stage::ProxyTlsHandshake;
    buffer_.clear();
    debugMessages.clear();
    
    addDebugMessage("开始连接流程");
    addDebugMessage(QString("目标URL: %1").arg(url));
    addDebugMessage(QString("代理服务器: %1:%2").arg(proxyHost).arg(proxyPort));
    
    emit connectionStarted();
    
    // 设置连接超时
    connectionTimer->start(30000); // 30秒超时
    
    // 配置SSL
    QSslConfiguration sslConfig = createSslConfiguration();
    sslSocket->setSslConfiguration(sslConfig);
    
    // 连接到代理服务器
    addDebugMessage(QString("正在连接到HTTPS代理: %1:%2").arg(proxyHost).arg(proxyPort));
    sslSocket->connectToHostEncrypted(proxyHost, proxyPort);
}

void ProxyClient::cancelRequest()
{
    if (connecting) {
        if (sslSocket) {
            sslSocket->disconnectFromHost();
        }
        if (connectionTimer) {
            connectionTimer->stop();
        }
        connecting = false;
        emit networkError("请求已取消");
    }
}

bool ProxyClient::isConnecting() const
{
    return connecting;
}

QSslConfiguration ProxyClient::createSslConfiguration()
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    
    // 加载CA证书
    if (!certificatePath.isEmpty()) {
        QFile certFile(certificatePath);
        if (certFile.open(QIODevice::ReadOnly)) {
            QSslCertificate cert(&certFile);
            if (!cert.isNull()) {
                QList<QSslCertificate> caCerts = sslConfig.caCertificates();
                caCerts.append(cert);
                sslConfig.setCaCertificates(caCerts);
                addDebugMessage("CA证书已加载:" + certificatePath);
            } else {
                addDebugMessage("警告: 无法加载CA证书:" + certificatePath);
            }
            certFile.close();
        } else {
            addDebugMessage("警告: 无法打开证书文件:" + certificatePath);
        }
    }
    
    // 设置SSL选项 - 不验证对等证书，让Qt自动协商协议
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    
    return sslConfig;
}

void ProxyClient::handleSslSocketConnected()
{
    addDebugMessage("已连接到代理服务器");


}

void ProxyClient::handleSslSocketDisconnected()
{
    QString msg = QString("SSL Socket已断开连接 - 连接状态:%1")
        .arg(connecting);
    
    addDebugMessage(msg);
    
    if (connecting) {
        connecting = false;
        connectionTimer->stop();
        emit connectionFinished(false, "连接已断开");
    }
}

void ProxyClient::handleSslSocketError(QAbstractSocket::SocketError error)
{
    QString errorMsg = QString("Socket错误: %1 - %2")
        .arg(error)
        .arg(sslSocket->errorString());
    addDebugMessage(errorMsg);
    
    if (connecting) {
        connecting = false;
        connectionTimer->stop();
        emit connectionFinished(false, errorMsg);
    }
}

void ProxyClient::handleSslErrors(const QList<QSslError> &errors)
{
    QString errorMsg = "SSL错误:\n";
    for (const QSslError &error : errors) {
        errorMsg += QString("- %1\n").arg(error.errorString());
    }
    
    addDebugMessage(errorMsg);
    emit sslErrors(errorMsg);
    
    // 忽略SSL错误继续连接
    sslSocket->ignoreSslErrors();
}

void ProxyClient::handleSslSocketEncrypted()
{
    if (stage_ == Stage::ProxyTlsHandshake) {
        addDebugMessage("已与代理建立TLS连接，准备发送CONNECT请求...");
        sendConnectRequest();
        stage_ = Stage::WaitProxyResponse;
        return;
    }

    if (stage_ == Stage::TargetTlsHandshake) {
        addDebugMessage("TLS握手完成，开始发送HTTP请求...");
        stage_ = Stage::Ready;
        sendHttpRequest();
    }
}

void ProxyClient::handleSslSocketReadyRead()
{
    const QByteArray data = sslSocket->readAll();
    if (data.isEmpty())
        return;

    buffer_.append(data);
    addDebugMessage(QString("收到数据，长度: %1 字节").arg(data.size()));

    if (stage_ == Stage::WaitProxyResponse) {
        const int headerEnd = buffer_.indexOf("\r\n\r\n");
        if (headerEnd == -1)
            return;

        const QByteArray header = buffer_.left(headerEnd + 4);
        if (!header.startsWith("HTTP/1.1 200") && !header.startsWith("HTTP/1.0 200")) {
            addDebugMessage("CONNECT请求失败: " + QString::fromUtf8(header));
            connecting = false;
            connectionTimer->stop();
            emit connectionFinished(false, "CONNECT请求失败: " + QString::fromUtf8(header));
            sslSocket->disconnectFromHost();
            return;
        }

        buffer_.remove(0, headerEnd + 4);
        stage_ = Stage::TargetTlsHandshake;
        sslSocket->setPeerVerifyName(targetHost);
        sslSocket->startClientEncryption();
        return;
    }

    if (stage_ == Stage::Ready) {
        parseHttpResponse();
    }
}

void ProxyClient::handleConnectionTimeout()
{
    if (connecting) {
        connecting = false;
        if (sslSocket) {
            sslSocket->disconnectFromHost();
        }
        emit connectionFinished(false, "连接超时");
    }
}

void ProxyClient::sendConnectRequest()
{
    if (sslSocket->state() != QAbstractSocket::ConnectedState) {
        emit networkError("未连接到代理服务器");
        return;
    }
    
    // 构建CONNECT请求 - 使用标准格式
    QString connectRequest = QString("CONNECT %1:%2 HTTP/1.1\r\n")
        .arg(targetHost)
        .arg(targetPort);
    
    connectRequest += QString("Host: %1:%2\r\n")
        .arg(targetHost)
        .arg(targetPort);
    
    // 添加代理认证
    if (!proxyUsername.isEmpty()) {
        QString auth = QString("%1:%2").arg(proxyUsername).arg(proxyPassword);
        QByteArray authBase64 = auth.toUtf8().toBase64();
        connectRequest += QString("Proxy-Authorization: Basic %1\r\n")
            .arg(QString::fromUtf8(authBase64));
    }
    
    // 添加更多标准头部
    connectRequest += "User-Agent: EasyProxyClient/1.0.0\r\n";
    connectRequest += "Connection: keep-alive\r\n";
    connectRequest += "\r\n";
    
    addDebugMessage("发送CONNECT请求: " + connectRequest);
    
    // 发送CONNECT请求
    int bytesWritten = sslSocket->write(connectRequest.toUtf8());
    addDebugMessage(QString("CONNECT请求已发送，字节数: %1").arg(bytesWritten));
    
}

void ProxyClient::sendHttpRequest()
{
    addDebugMessage("开始构建HTTP请求...");
    
    // 构建HTTP请求
    QUrl url(targetUrl);
    QString request = QString("GET %1 HTTP/1.1\r\n")
        .arg(url.path().isEmpty() ? "/" : url.path());
    
    request += QString("Host: %1\r\n").arg(targetHost);
    request += "User-Agent: EasyProxyClient/1.0.0\r\n";
    request += "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
    request += "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n";
    request += "Accept-Encoding: gzip, deflate\r\n";
    request += "Connection: keep-alive\r\n\r\n";
    
    addDebugMessage("发送HTTP请求: " + request);
    
    // 发送HTTP请求
    int bytesWritten = sslSocket->write(request.toUtf8());
    addDebugMessage(QString("HTTP请求已发送，字节数: %1").arg(bytesWritten));
}

void ProxyClient::parseHttpResponse()
{
    // 查找HTTP响应结束标记
    int endIndex = buffer_.indexOf("\r\n\r\n");
    if (endIndex == -1) {
        return; // 响应不完整，等待更多数据
    }
    
    // 提取响应头
    QByteArray responseHeaders = buffer_.left(endIndex);
    QString responseStr = QString::fromUtf8(responseHeaders);
    
    addDebugMessage("收到HTTP响应头: " + responseStr);
    
    // 检查响应状态
    if (responseStr.contains("200 OK")) {
        // 提取响应体
        QByteArray responseBody = buffer_.mid(endIndex + 4);
        
        // 检查是否有Content-Length头
        int contentLength = -1;
        if (responseStr.contains("Content-Length:")) {
            int clIndex = responseStr.indexOf("Content-Length:");
            int clEndIndex = responseStr.indexOf("\r\n", clIndex);
            if (clEndIndex == -1) clEndIndex = responseStr.indexOf("\n", clIndex);
            if (clEndIndex != -1) {
                QString clStr = responseStr.mid(clIndex + 15, clEndIndex - clIndex - 15).trimmed();
                contentLength = clStr.toInt();
            }
        }
        
        // 如果指定了Content-Length，等待完整响应体
        if (contentLength > 0 && responseBody.size() < contentLength) {
            return; // 等待更多数据
        }
        
        // 构建完整响应信息
        QString result = QString("=== 连接状态 ===\n");
        result += QString("TLS连接: 成功\n");
        result += QString("CONNECT请求: 成功\n");
        result += QString("HTTP请求: 成功\n\n");
        result += QString("=== 响应信息 ===\n");
        result += QString("状态码: 200 OK\n");
        result += QString("内容长度: %1 字节\n\n").arg(responseBody.size());
        
        // 如果是文本内容，直接显示
        QString contentType = "text/html"; // 默认
        if (responseStr.contains("Content-Type:")) {
            // 提取Content-Type
            int ctIndex = responseStr.indexOf("Content-Type:");
            int ctEndIndex = responseStr.indexOf("\r\n", ctIndex);
            if (ctEndIndex == -1) ctEndIndex = responseStr.indexOf("\n", ctIndex);
            if (ctEndIndex != -1) {
                contentType = responseStr.mid(ctIndex + 13, ctEndIndex - ctIndex - 13).trimmed();
            }
        }
        
        result += QString("内容类型: %1\n\n").arg(contentType);
        result += QString("=== 网页内容 ===\n");
        
        if (contentType.contains("text") || contentType.contains("json") || contentType.contains("xml")) {
            result += QString::fromUtf8(responseBody);
        } else {
            result += "[二进制内容，无法显示]";
        }
        
        connecting = false;
        connectionTimer->stop();
        emit connectionFinished(true, result);
        
        // 关闭连接
        sslSocket->disconnectFromHost();
    } else {
        // HTTP请求失败
        connecting = false;
        connectionTimer->stop();
        emit connectionFinished(false, "HTTP请求失败: " + responseStr);
    }
}

void ProxyClient::showError(const QString &message)
{
    qDebug() << "错误:" << message;
    emit networkError(message);
} 