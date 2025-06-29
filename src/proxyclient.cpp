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

ProxyClient::ProxyClient(QObject *parent)
    : QObject(parent)
    , sslSocket(new QSslSocket(this))
    , connectionTimer(new QTimer(this))
    , proxyPort(8080)
    , targetPort(443)
    , connecting(false)
    , tlsConnected(false)
    , connectRequestSent(false)
    , connectResponseReceived(false)
{
    // 连接SSL Socket信号
    connect(sslSocket, &QSslSocket::connected, 
            this, &ProxyClient::handleSslSocketConnected);
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
    
    emit connectionStarted();
    
    // 设置连接超时
    connectionTimer->start(30000); // 30秒超时
    
    // 配置SSL
    QSslConfiguration sslConfig = createSslConfiguration();
    sslSocket->setSslConfiguration(sslConfig);
    
    // 连接到代理服务器
    QString msg = QString("正在连接到HTTPS代理: %1:%2").arg(proxyHost).arg(proxyPort);
    qDebug() << msg;
    emit networkError(msg);
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
                qDebug() << "CA证书已加载:" << certificatePath;
            } else {
                qDebug() << "警告: 无法加载CA证书:" << certificatePath;
            }
            certFile.close();
        } else {
            qDebug() << "警告: 无法打开证书文件:" << certificatePath;
        }
    }
    
    // 设置SSL选项 - 不验证对等证书，让Qt自动协商协议
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    
    return sslConfig;
}

void ProxyClient::handleSslSocketConnected()
{
    QString msg = "SSL Socket已连接 - TLS握手成功";
    qDebug() << msg;
    emit networkError(msg); // 临时使用networkError信号显示调试信息
    
    tlsConnected = true;
    
    // TLS连接建立后，发送CONNECT请求
    sendConnectRequest();
}

void ProxyClient::handleSslSocketDisconnected()
{
    QString msg = QString("SSL Socket已断开连接 - 连接状态:%1, TLS连接:%2, CONNECT请求已发送:%3, CONNECT响应已接收:%4")
        .arg(connecting)
        .arg(tlsConnected)
        .arg(connectRequestSent)
        .arg(connectResponseReceived);
    
    qDebug() << msg;
    emit networkError(msg); // 临时使用networkError信号显示调试信息
    
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
    qDebug() << errorMsg;
    emit networkError(errorMsg);
    
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
    
    qDebug() << errorMsg;
    emit sslErrors(errorMsg);
    emit networkError(errorMsg); // 同时发送到GUI
    
    // 忽略SSL错误继续连接
    sslSocket->ignoreSslErrors();
}

void ProxyClient::handleSslSocketReadyRead()
{
    QByteArray data = sslSocket->readAll();
    responseBuffer.append(data);
    
    if (!connectResponseReceived) {
        parseConnectResponse();
    } else {
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
    if (!tlsConnected) {
        emit networkError("TLS连接未建立");
        return;
    }
    
    // 构建CONNECT请求
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
    
    connectRequest += "Connection: keep-alive\r\n\r\n";
    
    QString msg = "发送CONNECT请求: " + connectRequest;
    qDebug() << msg;
    emit networkError(msg);
    
    // 发送CONNECT请求
    sslSocket->write(connectRequest.toUtf8());
    connectRequestSent = true;
}

void ProxyClient::parseConnectResponse()
{
    // 查找HTTP响应结束标记
    int endIndex = responseBuffer.indexOf("\r\n\r\n");
    if (endIndex == -1) {
        return; // 响应不完整，等待更多数据
    }
    
    // 提取响应头
    QByteArray responseHeaders = responseBuffer.left(endIndex);
    QString responseStr = QString::fromUtf8(responseHeaders);
    
    QString msg = "收到CONNECT响应: " + responseStr;
    qDebug() << msg;
    emit networkError(msg);
    
    // 检查响应状态
    if (responseStr.contains("200 Connection Established")) {
        connectResponseReceived = true;
        responseBuffer.remove(0, endIndex + 4); // 移除响应头
        
        // CONNECT成功，现在发送HTTP请求
        sendHttpRequest();
    } else {
        // CONNECT失败
        connecting = false;
        connectionTimer->stop();
        emit connectionFinished(false, "CONNECT请求失败: " + responseStr);
    }
}

void ProxyClient::sendHttpRequest()
{
    // 构建HTTP请求
    QUrl url(targetUrl);
    QString request = QString("GET %1 HTTP/1.1\r\n")
        .arg(url.path().isEmpty() ? "/" : url.path());
    
    request += QString("Host: %1\r\n").arg(targetHost);
    request += "User-Agent: EasyProxyClient/1.0.0\r\n";
    request += "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
    request += "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n";
    request += "Accept-Encoding: gzip, deflate\r\n";
    request += "Connection: close\r\n\r\n";
    
    QString msg = "发送HTTP请求: " + request;
    qDebug() << msg;
    emit networkError(msg);
    
    // 发送HTTP请求
    sslSocket->write(request.toUtf8());
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
    
    QString msg = "收到HTTP响应头: " + responseStr;
    qDebug() << msg;
    emit networkError(msg);
    
    // 检查响应状态
    if (responseStr.contains("200 OK")) {
        // 提取响应体
        QByteArray responseBody = responseBuffer.mid(endIndex + 4);
        
        // 构建完整响应信息
        QString result = QString("状态码: 200 OK\n");
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
        
        if (contentType.contains("text") || contentType.contains("json") || contentType.contains("xml")) {
            result += QString::fromUtf8(responseBody);
        } else {
            result += "[二进制内容，无法显示]";
        }
        
        connecting = false;
        connectionTimer->stop();
        emit connectionFinished(true, result);
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