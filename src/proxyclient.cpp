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
#include <QNetworkProxy>
#include <QDateTime>

ProxyClient::ProxyClient(QObject *parent)
    : QObject(parent)
    , networkManager(new QNetworkAccessManager(this))
    , networkReply(nullptr)
    , connectionTimer(new QTimer(this))
    , proxyPort(8080)
    , targetPort(443)
    , connecting(false)
{
    // 连接超时定时器
    connect(connectionTimer, &QTimer::timeout,
            this, &ProxyClient::handleConnectionTimeout);
    connectionTimer->setSingleShot(true);
}

ProxyClient::~ProxyClient()
{
    if (networkReply) {
        networkReply->abort();
        networkReply->deleteLater();
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
    if (connecting) {
        emit networkError("正在连接中，请等待当前请求完成");
        return;
    }

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

    QUrl targetUrlObj(url);
    if (!targetUrlObj.isValid()) {
        emit networkError("无效的目标URL");
        return;
    }

    targetUrl = url;

    // 重置状态
    connecting = true;
    responseBuffer.clear();
    debugMessages.clear();

    addDebugMessage("开始连接流程");
    addDebugMessage(QString("目标URL: %1").arg(url));
    addDebugMessage(QString("代理服务器: %1:%2").arg(proxyHost).arg(proxyPort));

    emit connectionStarted();

    connectionTimer->start(30000);

    // 设置HTTPS代理
    QNetworkProxy proxy(QNetworkProxy::HttpsProxy, proxyHost, proxyPort, proxyUsername, proxyPassword);
    networkManager->setProxy(proxy);

    QNetworkRequest request(targetUrlObj);
    request.setSslConfiguration(createSslConfiguration());

    addDebugMessage("发送网络请求...");
    networkReply = networkManager->get(request);
    connect(networkReply, &QNetworkReply::finished, this, &ProxyClient::handleReplyFinished);
    connect(networkReply, &QNetworkReply::readyRead, this, &ProxyClient::handleReplyReadyRead);
    connect(networkReply, &QNetworkReply::sslErrors, this, &ProxyClient::handleReplySslErrors);
}

void ProxyClient::cancelRequest()
{
    if (connecting) {
        if (networkReply) {
            networkReply->abort();
            networkReply->deleteLater();
            networkReply = nullptr;
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

void ProxyClient::handleReplyReadyRead()
{
    if (!networkReply)
        return;
    QByteArray data = networkReply->readAll();
    addDebugMessage(QString("收到数据，长度: %1 字节").arg(data.size()));
    responseBuffer.append(data);
}

void ProxyClient::handleReplySslErrors(const QList<QSslError> &errors)
{
    QString errorMsg = "SSL错误:\n";
    for (const QSslError &e : errors) {
        errorMsg += QString("- %1\n").arg(e.errorString());
    }
    addDebugMessage(errorMsg);
    emit sslErrors(errorMsg);
    if (networkReply)
        networkReply->ignoreSslErrors();
}

void ProxyClient::handleReplyFinished()
{
    if (!networkReply)
        return;

    connectionTimer->stop();
    connecting = false;

    if (networkReply->error() != QNetworkReply::NoError) {
        QString err = networkReply->errorString();
        addDebugMessage("请求失败: " + err);
        emit connectionFinished(false, err);
        networkReply->deleteLater();
        networkReply = nullptr;
        return;
    }

    QString result = QString::fromUtf8(responseBuffer);
    emit connectionFinished(true, result);
    networkReply->deleteLater();
    networkReply = nullptr;
}

void ProxyClient::handleConnectionTimeout()
{
    if (connecting) {
        connecting = false;
        if (networkReply) {
            networkReply->abort();
            networkReply->deleteLater();
            networkReply = nullptr;
        }
        emit connectionFinished(false, "连接超时");
    }
}

void ProxyClient::showError(const QString &message)
{
    qDebug() << "错误:" << message;
    emit networkError(message);
} 