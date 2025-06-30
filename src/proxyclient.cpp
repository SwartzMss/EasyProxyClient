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
    , proxySocket(new QSslSocket(this))
    , tunnelSocket(nullptr)
    , currentSocket(proxySocket)
    , connectionTimer(new QTimer(this))
    , proxyPort(8080)
    , targetPort(443)
    , connecting(false)
    , tlsConnected(false)
    , connectRequestSent(false)
    , connectResponseReceived(false)
{
    // 连接到代理使用的socket信号
    connect(proxySocket, &QSslSocket::connected,
            this, &ProxyClient::handleSslSocketConnected);
    connect(proxySocket, &QSslSocket::encrypted,
            this, &ProxyClient::handleSslSocketEncrypted);
    connect(proxySocket, &QSslSocket::disconnected,
            this, &ProxyClient::handleSslSocketDisconnected);
    connect(proxySocket, &QSslSocket::errorOccurred,
            this, &ProxyClient::handleSslSocketError);
    connect(proxySocket, &QSslSocket::sslErrors,
            this, &ProxyClient::handleSslErrors);
    connect(proxySocket, &QSslSocket::readyRead,
            this, &ProxyClient::handleSslSocketReadyRead);
    
    // 连接超时定时器
    connect(connectionTimer, &QTimer::timeout, 
            this, &ProxyClient::handleConnectionTimeout);
    connectionTimer->setSingleShot(true);
}

ProxyClient::~ProxyClient()
{
    if (proxySocket) {
        proxySocket->disconnectFromHost();
    }
    if (tunnelSocket && tunnelSocket != proxySocket) {
        tunnelSocket->disconnectFromHost();
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
    tlsConnected = false;
    connectRequestSent = false;
    connectResponseReceived = false;
    responseBuffer.clear();
    debugMessages.clear();
    
    addDebugMessage("开始连接流程");
    addDebugMessage(QString("目标URL: %1").arg(url));
    addDebugMessage(QString("代理服务器: %1:%2").arg(proxyHost).arg(proxyPort));
    
    emit connectionStarted();
    
    // 设置连接超时
    connectionTimer->start(30000); // 30秒超时
    
    // 配置SSL
    QSslConfiguration sslConfig = createSslConfiguration();
    proxySocket->setSslConfiguration(sslConfig);
    
    // 连接到代理服务器
    addDebugMessage(QString("正在连接到HTTPS代理: %1:%2").arg(proxyHost).arg(proxyPort));
    currentSocket = proxySocket;
    proxySocket->connectToHostEncrypted(proxyHost, proxyPort);
}

void ProxyClient::cancelRequest()
{
    if (connecting) {
        if (currentSocket) {
            currentSocket->disconnectFromHost();
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
    QString msg = QString("SSL Socket已断开连接 - 连接状态:%1, TLS连接:%2, CONNECT请求已发送:%3, CONNECT响应已接收:%4")
        .arg(connecting)
        .arg(tlsConnected)
        .arg(connectRequestSent)
        .arg(connectResponseReceived);
    
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
        .arg(currentSocket->errorString());
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
    currentSocket->ignoreSslErrors();
}

void ProxyClient::handleSslSocketEncrypted()
{
    // 如果CONNECT请求尚未发送，说明当前TLS握手与代理服务器完成
    if (!connectRequestSent) {
        addDebugMessage("已与代理建立TLS连接，准备发送CONNECT请求...");
        sendConnectRequest();
        return;
    }

    tlsConnected = true;
    addDebugMessage("TLS握手完成，开始发送HTTP请求...");
    sendHttpRequest();
}

void ProxyClient::handleSslSocketReadyRead()
{
    QByteArray data = currentSocket->readAll();
    addDebugMessage(QString("收到数据，长度: %1 字节").arg(data.size()));
    responseBuffer.append(data);
    
    if (!connectResponseReceived) {
        addDebugMessage("解析CONNECT响应...");
        parseConnectResponse();
    } else {
        addDebugMessage("解析HTTP响应...");
        parseHttpResponse();
    }
}

void ProxyClient::handleConnectionTimeout()
{
    if (connecting) {
        connecting = false;
        if (currentSocket) {
            currentSocket->disconnectFromHost();
        }
        emit connectionFinished(false, "连接超时");
    }
}

void ProxyClient::sendConnectRequest()
{
    if (currentSocket->state() != QAbstractSocket::ConnectedState) {
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
    int bytesWritten = currentSocket->write(connectRequest.toUtf8());
    addDebugMessage(QString("CONNECT请求已发送，字节数: %1").arg(bytesWritten));
    
    connectRequestSent = true;
}

void ProxyClient::parseConnectResponse()
{
    // 查找HTTP响应结束标记
    int endIndex = responseBuffer.indexOf("\r\n\r\n");
    if (endIndex == -1) {
        addDebugMessage("CONNECT响应不完整，等待更多数据...");
        return; // 响应不完整，等待更多数据
    }
    
    // 提取响应头
    QByteArray responseHeaders = responseBuffer.left(endIndex);
    QString responseStr = QString::fromUtf8(responseHeaders);
    
    addDebugMessage("收到CONNECT响应: " + responseStr);
    
    // 检查响应状态
    if (responseStr.contains("200 Connection Established")) {
        addDebugMessage("CONNECT请求成功，开始TLS握手...");
        connectResponseReceived = true;
        responseBuffer.remove(0, endIndex + 4); // 移除响应头

        // 创建新的socket用于与目标服务器TLS握手
        tunnelSocket = new QSslSocket(this);
        QSslConfiguration cfg = createSslConfiguration();
        tunnelSocket->setSslConfiguration(cfg);

        // 继承已有连接的描述符
        qintptr desc = proxySocket->socketDescriptor();
        tunnelSocket->setSocketDescriptor(desc);
        proxySocket->setSocketDescriptor(-1); // 防止旧socket关闭连接

        // 重新连接信号到新socket
        connect(tunnelSocket, &QSslSocket::encrypted,
                this, &ProxyClient::handleSslSocketEncrypted);
        connect(tunnelSocket, &QSslSocket::readyRead,
                this, &ProxyClient::handleSslSocketReadyRead);
        connect(tunnelSocket, &QSslSocket::disconnected,
                this, &ProxyClient::handleSslSocketDisconnected);
        connect(tunnelSocket, &QSslSocket::errorOccurred,
                this, &ProxyClient::handleSslSocketError);
        connect(tunnelSocket, &QSslSocket::sslErrors,
                this, &ProxyClient::handleSslErrors);

        currentSocket = tunnelSocket;
        currentSocket->setPeerVerifyName(targetHost);
        currentSocket->startClientEncryption();
    } else {
        // CONNECT失败
        addDebugMessage("CONNECT请求失败: " + responseStr);
        connecting = false;
        connectionTimer->stop();
        emit connectionFinished(false, "CONNECT请求失败: " + responseStr);
    }
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
    int bytesWritten = currentSocket->write(request.toUtf8());
    addDebugMessage(QString("HTTP请求已发送，字节数: %1").arg(bytesWritten));
}

void ProxyClient::parseHttpResponse()
{
    // 查找HTTP响应结束标记
    int endIndex = responseBuffer.indexOf("\r\n\r\n");
    if (endIndex == -1) {
        return; // 响应不完整，等待更多数据
    }
    
    // 提取响应头
    QByteArray responseHeaders = responseBuffer.left(endIndex);
    QString responseStr = QString::fromUtf8(responseHeaders);
    
    addDebugMessage("收到HTTP响应头: " + responseStr);
    
    // 检查响应状态
    if (responseStr.contains("200 OK")) {
        // 提取响应体
        QByteArray responseBody = responseBuffer.mid(endIndex + 4);
        
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
        if (currentSocket)
            currentSocket->disconnectFromHost();
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