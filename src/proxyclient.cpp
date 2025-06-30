#include "proxyclient.h"
#include <QDateTime>
#include <QFile>
#include <QUrl>
#include <QDebug>

// ───────────────────────────────────────────────────────────────────────────────
ProxyClient::ProxyClient(QObject *parent)
    : QObject(parent),
      socket_(new QSslSocket(this)),
      timer_(new QTimer(this))
{
    // socket signals
    connect(socket_, &QSslSocket::connected,       this, &ProxyClient::onSocketConnected);
    connect(socket_, &QSslSocket::encrypted,       this, &ProxyClient::onSocketEncrypted);
    connect(socket_, &QSslSocket::readyRead,       this, &ProxyClient::onSocketReadyRead);
    connect(socket_, &QSslSocket::disconnected,    this, &ProxyClient::onSocketDisconnected);
    connect(socket_, &QSslSocket::errorOccurred,   this, &ProxyClient::onSocketError);
    connect(socket_, &QSslSocket::sslErrors,       this, &ProxyClient::onSslErrors);

    // timeout
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &ProxyClient::onConnectionTimeout);
}

ProxyClient::~ProxyClient()
{
    if (socket_)
        socket_->disconnectFromHost();
}

// ───────────────────────────────────────────────────────────────────────────────
void ProxyClient::setProxySettings(const QString &host, int port,
                                   const QString &username, const QString &password)
{
    proxyHost_ = host;
    proxyPort_ = port;
    proxyUser_ = username;
    proxyPass_ = password;
}

void ProxyClient::setSslCertificate(const QString &certificatePath)
{
    caPath_ = certificatePath;
}

// ───────────────────────────────────────────────────────────────────────────────
void ProxyClient::appendDebug(const QString &msg)
{
    const QString stamped = QString("[%1] %2")
                                .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"), msg);
    debugLines_ << stamped;
    emit debugMessage(stamped);
    qDebug() << stamped;
}

void ProxyClient::finishWithError(const QString &msg)
{
    appendDebug("ERROR: " + msg);
    connecting_ = false;
    timer_->stop();
    socket_->disconnectFromHost();
    emit connectionFinished(false, msg);
}

// ───────────────────────────────────────────────────────────────────────────────
void ProxyClient::connectToUrl(const QString &url)
{
    if (connecting_) {
        emit networkError(tr("正在连接中，请等待当前请求完成"));
        return;
    }

    if (proxyHost_.isEmpty() || proxyPort_ <= 0) {
        emit networkError(tr("请填写有效的代理地址和端口"));
        return;
    }

    const QUrl u(url);
    if (!u.isValid() || u.host().isEmpty()) {
        emit networkError(tr("无效的目标URL"));
        return;
    }

    // set target
    targetUrl_  = url;
    targetHost_ = u.host();
    targetPort_ = u.port(443);

    // reset state
    buffer_.clear();
    stage_      = Stage::ProxyTlsHandshake;
    connecting_ = true;
    debugLines_.clear();

    emit connectionStarted();
    appendDebug(tr("开始连接流程 -> %1 via %2:%3").arg(targetUrl_).arg(proxyHost_).arg(proxyPort_));

    // timeout 30s
    timer_->start(30'000);

    // SSL config & connect
    socket_->setSslConfiguration(buildSslConfiguration());
    appendDebug(tr("正在 TLS 连接代理服务器 %1:%2 ...").arg(proxyHost_).arg(proxyPort_));
    socket_->connectToHostEncrypted(proxyHost_, proxyPort_);
}

void ProxyClient::cancelRequest()
{
    if (!connecting_)
        return;
    connecting_ = false;
    timer_->stop();
    socket_->disconnectFromHost();
    emit networkError(tr("请求已取消"));
}

// ───────────────────────────────────────────────────────────────────────────────
QSslConfiguration ProxyClient::buildSslConfiguration() const
{
    QSslConfiguration cfg = QSslConfiguration::defaultConfiguration();
    if (!caPath_.isEmpty()) {
        QFile f(caPath_);
        if (f.open(QIODevice::ReadOnly)) {
            const QSslCertificate cert(&f);
            if (!cert.isNull()) {
                auto list = cfg.caCertificates();
                list.append(cert);
                cfg.setCaCertificates(list);
            }
        }
    }
    cfg.setPeerVerifyMode(QSslSocket::VerifyNone);   // accept any proxy cert
    return cfg;
}

// ───────────────────────────────────────────────────────────────────────────────
void ProxyClient::onSocketConnected()
{
    appendDebug("TCP 链接已建立 (待 TLS 握手)");
}

void ProxyClient::onSocketEncrypted()
{
    if (stage_ == Stage::ProxyTlsHandshake) {
        appendDebug("与代理 TLS 握手完成，发送 CONNECT 请求 ...");
        sendConnectRequest();
        stage_ = Stage::WaitProxyResponse;
        return;
    }

    if (stage_ == Stage::TargetTlsHandshake) {
        appendDebug("与目标站点 TLS 握手完成，可发送 HTTP 请求 ...");
        stage_ = Stage::Ready;
        // now send the actual HTTP GET
        socket_->setPeerVerifyName(targetHost_); // already set, but safe
        socket_->flush();
        // send request immediately
        QString path = QUrl(targetUrl_).path();
        if (path.isEmpty()) path = "/";
        QString req = QString("GET %1 HTTP/1.1\r\nHost: %2\r\nUser-Agent: EasyProxyClient/2.0\r\nConnection: close\r\n\r\n")
                           .arg(path, targetHost_);
        appendDebug("发送 HTTP 请求:\n" + req);
        socket_->write(req.toUtf8());
        return;
    }

    // unexpected
    appendDebug("onSocketEncrypted: unexpected stage");
}

void ProxyClient::onSocketReadyRead()
{
    buffer_.append(socket_->readAll());

    if (stage_ == Stage::WaitProxyResponse) {
        if (tryParseConnectResponse()) {
            // CONNECT success, begin second TLS handshake
            stage_ = Stage::TargetTlsHandshake;
            socket_->setPeerVerifyName(targetHost_);
            socket_->startClientEncryption();
        }
        return;
    }

    if (stage_ == Stage::Ready) {
        if (tryParseHttpResponse()) {
            connecting_ = false;
            timer_->stop();
            socket_->disconnectFromHost();
        }
    }
}

void ProxyClient::onSocketDisconnected()
{
    appendDebug("socket disconnected");
    if (connecting_) {
        finishWithError(tr("连接中途断开"));
    }
}

void ProxyClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    finishWithError(socket_->errorString());
}

void ProxyClient::onSslErrors(const QList<QSslError> &errors)
{
    QStringList lines;
    for (const auto &e : errors) lines << e.errorString();
    appendDebug("代理 TLS 证书错误 (忽略):\n" + lines.join("\n"));
    socket_->ignoreSslErrors();
}

void ProxyClient::onConnectionTimeout()
{
    if (!connecting_) return;
    finishWithError(tr("连接超时"));
}

// ───────────────────────────────────────────────────────────────────────────────
void ProxyClient::sendConnectRequest()
{
    QString req = QString("CONNECT %1:%2 HTTP/1.1\r\nHost: %1:%2\r\n")
                      .arg(targetHost_).arg(targetPort_);

    if (!proxyUser_.isEmpty()) {
        const QByteArray token = QString("%1:%2").arg(proxyUser_, proxyPass_).toUtf8().toBase64();
        req += QString("Proxy-Authorization: Basic %1\r\n").arg(QString::fromUtf8(token));
    }

    req += "User-Agent: EasyProxyClient/2.0\r\nConnection: keep-alive\r\n\r\n";
    appendDebug("发送 CONNECT 请求:\n" + req);
    socket_->write(req.toUtf8());
}

bool ProxyClient::tryParseConnectResponse()
{
    int headerEnd = buffer_.indexOf("\r\n\r\n");
    if (headerEnd == -1) return false;  // not yet

    const QByteArray header = buffer_.left(headerEnd + 4);
    appendDebug("收到 CONNECT 响应:\n" + QString::fromUtf8(header));

    if (!header.contains(" 200 ")) {
        finishWithError(tr("CONNECT 失败"));
        return false;
    }

    buffer_.remove(0, headerEnd + 4);
    return true;
}

bool ProxyClient::tryParseHttpResponse()
{
    int headerEnd = buffer_.indexOf("\r\n\r\n");
    if (headerEnd == -1) return false;

    const QByteArray header = buffer_.left(headerEnd + 4);
    if (!header.startsWith("HTTP/")) return false;

    appendDebug("收到 HTTP 头:\n" + QString::fromUtf8(header));

    // simple Content-Length check
    int contentLength = -1;
    const QByteArray key("Content-Length:");
    int idx = header.indexOf(key);
    if (idx != -1) {
        int lineEnd = header.indexOf("\r\n", idx);
        if (lineEnd != -1) {
            contentLength = header.mid(idx + key.size(), lineEnd - idx - key.size()).trimmed().toInt();
        }
    }

    if (contentLength >= 0 && buffer_.size() - (headerEnd + 4) < contentLength)
        return false; // body incomplete

    QByteArray body = buffer_.mid(headerEnd + 4);

    QString result;
    result += "=== 连接成功 ===\n";
    result += QString("HTTP 返回 %1 字节\n\n").arg(body.size());
    if (body.startsWith("<!DOCTYPE") || body.startsWith("<html")) {
        result += QString::fromUtf8(body);
    } else {
        result += QString("[二进制内容, 前 128 字节十六进制]\n%1")
                          .arg(QString(body.left(128).toHex(' ')));
    }

    emit connectionFinished(true, result);
    return true;
}
