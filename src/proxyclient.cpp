#include "proxyclient.h"
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslError>
#include <QFile>
#include <QDebug>

ProxyClient::ProxyClient(QObject *parent)
    : QObject(parent)
    , networkManager(new QNetworkAccessManager(this))
    , currentReply(nullptr)
    , proxyPort(8080)
    , connecting(false)
{
    connect(networkManager, &QNetworkAccessManager::finished, 
            this, &ProxyClient::handleNetworkReply);
}

ProxyClient::~ProxyClient()
{
    if (currentReply) {
        currentReply->abort();
        currentReply->deleteLater();
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
    
    // 设置连接状态
    connecting = true;
    emit connectionStarted();
    
    // 配置代理
    QNetworkProxy proxy;
    proxy.setType(QNetworkProxy::HttpProxy);
    proxy.setHostName(proxyHost);
    proxy.setPort(proxyPort);
    
    if (!proxyUsername.isEmpty()) {
        proxy.setUser(proxyUsername);
        proxy.setPassword(proxyPassword);
    }
    
    networkManager->setProxy(proxy);
    
    // 创建请求 - 使用更明确的语法
    QUrl targetUrl(url);
    QNetworkRequest request;
    request.setUrl(targetUrl);
    
    // 配置SSL
    if (!certificatePath.isEmpty()) {
        QSslConfiguration sslConfig = createSslConfiguration();
        request.setSslConfiguration(sslConfig);
    }
    
    // 发送请求
    currentReply = networkManager->get(request);
    
    // 连接SSL错误信号
    connect(currentReply, &QNetworkReply::sslErrors, 
            this, &ProxyClient::handleSslErrors);
}

void ProxyClient::cancelRequest()
{
    if (currentReply && connecting) {
        currentReply->abort();
        connecting = false;
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
    QFile certFile(certificatePath);
    if (certFile.open(QIODevice::ReadOnly)) {
        QSslCertificate cert(&certFile);
        if (!cert.isNull()) {
            QList<QSslCertificate> caCerts = sslConfig.caCertificates();
            caCerts.append(cert);
            sslConfig.setCaCertificates(caCerts);
        }
        certFile.close();
    }
    
    // 设置SSL选项
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
    
    return sslConfig;
}

void ProxyClient::handleNetworkReply(QNetworkReply *reply)
{
    // 重置连接状态
    connecting = false;
    
    if (reply->error() == QNetworkReply::NoError) {
        // 读取响应内容
        QByteArray data = reply->readAll();
        QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        
        // 构建响应信息
        QString result = QString("状态码: %1\n").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        result += QString("内容类型: %1\n").arg(contentType);
        result += QString("内容长度: %1 字节\n\n").arg(data.size());
        
        // 如果是文本内容，直接显示
        if (contentType.contains("text") || contentType.contains("json") || contentType.contains("xml")) {
            result += QString::fromUtf8(data);
        } else {
            result += "[二进制内容，无法显示]";
        }
        
        emit connectionFinished(true, result);
    } else {
        QString errorMsg = QString("网络错误: %1\n%2")
            .arg(reply->error())
            .arg(reply->errorString());
        emit connectionFinished(false, errorMsg);
    }
    
    // 清理当前回复对象
    if (reply == currentReply) {
        currentReply = nullptr;
    }
    
    reply->deleteLater();
}

void ProxyClient::handleSslErrors(const QList<QSslError> &errors)
{
    QString errorMsg = "SSL错误:\n";
    for (const QSslError &error : errors) {
        errorMsg += QString("- %1\n").arg(error.errorString());
    }
    
    emit sslErrors(errorMsg);
    
    // 可以选择忽略SSL错误继续连接
    if (currentReply) {
        currentReply->ignoreSslErrors();
    }
} 