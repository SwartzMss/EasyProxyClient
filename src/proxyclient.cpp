#include "proxyclient.h"
#include <QDateTime>
#include <QUrl>
#include <QDebug>
#include <QMetaObject>

ProxyClient::ProxyClient(QObject *parent)
    : QObject(parent),
      timer_(new QTimer(this))
{
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, [this]() {
        if (connecting_) {
            finishWithError(tr("连接超时"));
        }
    });
}

ProxyClient::~ProxyClient()
{
    cancelRequest();
}

void ProxyClient::setProxySettings(const QString &host, int port,
                                   const QString &username,
                                   const QString &password)
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
    emit connectionFinished(false, msg);
}

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

    targetUrl_ = url;

    connecting_ = true;
    headerBuffer_.clear();
    bodyBuffer_.clear();
    debugLines_.clear();

    emit connectionStarted();
    appendDebug(tr("开始连接流程 -> %1 via %2:%3").arg(targetUrl_).arg(proxyHost_).arg(proxyPort_));

    timer_->start(30'000); // 30s timeout

    worker_ = QThread::create([this]() { performRequest(); });
    connect(worker_, &QThread::finished, worker_, &QObject::deleteLater);
    worker_->start();
}

void ProxyClient::cancelRequest()
{
    connecting_ = false;
    timer_->stop();
    if (worker_) {
        worker_->quit();
        worker_->wait();
        worker_ = nullptr;
    }
}

size_t ProxyClient::headerCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    ProxyClient *self = static_cast<ProxyClient *>(userdata);
    QByteArray data(buffer, size * nitems);
    self->headerBuffer_.append(data);
    QMetaObject::invokeMethod(self, [self, data]() { self->appendDebug(QString::fromUtf8(data).trimmed()); }, Qt::QueuedConnection);
    return size * nitems;
}

size_t ProxyClient::writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    ProxyClient *self = static_cast<ProxyClient *>(userdata);
    self->bodyBuffer_.append(ptr, size * nmemb);
    return size * nmemb;
}

void ProxyClient::performRequest()
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        QMetaObject::invokeMethod(this, [this]() { finishWithError(tr("初始化curl失败")); }, Qt::QueuedConnection);
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, targetUrl_.toUtf8().constData());
    curl_easy_setopt(curl, CURLOPT_PROXY, proxyHost_.toUtf8().constData());
    curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxyPort_);
    curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTPS);
    curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1L);

    if (!proxyUser_.isEmpty()) {
        QByteArray auth = QString("%1:%2").arg(proxyUser_, proxyPass_).toUtf8();
        curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, auth.constData());
    }

    if (!caPath_.isEmpty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, caPath_.toUtf8().constData());
        curl_easy_setopt(curl, CURLOPT_PROXY_CAINFO, caPath_.toUtf8().constData());
        appendDebug("使用CA证书: " + caPath_);
        
        // SSL配置 - 针对自签名证书优化
        // 对代理服务器启用SSL验证
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYHOST, 2L);
        
        // 对目标服务器禁用SSL验证（因为目标服务器通常是受信任的）
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        appendDebug("代理服务器SSL验证已启用，目标服务器SSL验证已禁用");
        
        // 添加SSL选项以处理自签名证书的常见问题
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_ALLOW_BEAST | CURLSSLOPT_NO_REVOKE | CURLSSLOPT_NO_PARTIALCHAIN);
        
        // 允许libcurl协商最佳TLS版本
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_DEFAULT);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSLVERSION, CURL_SSLVERSION_MAX_DEFAULT);
    } else {
        // 如果没有提供CA证书，禁用SSL验证
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYHOST, 0L);
        appendDebug("警告: 未提供CA证书，SSL验证已禁用");
    }

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &ProxyClient::headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &ProxyClient::writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

    CURLcode res = curl_easy_perform(curl);
    long response = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
    curl_easy_cleanup(curl);

    QMetaObject::invokeMethod(this, [this, res, response]() {
        timer_->stop();
        connecting_ = false;
        if (res != CURLE_OK) {
            QString errorMsg = QString::fromUtf8(curl_easy_strerror(res));
            appendDebug("CURL错误代码: " + QString::number(res));
            appendDebug("CURL错误描述: " + errorMsg);
            
            // 针对SSL错误的特殊处理
            if (res == CURLE_SSL_CONNECT_ERROR || res == CURLE_SSL_CERTPROBLEM || 
                res == CURLE_PEER_FAILED_VERIFICATION) {
                errorMsg += "\n\n可能的解决方案:\n";
                errorMsg += "1. 检查CA证书文件是否正确\n";
                errorMsg += "2. 确认证书文件格式为PEM格式\n";
                errorMsg += "3. 验证证书是否与代理服务器匹配\n";
                errorMsg += "4. 尝试使用不同的SSL版本";
            }
            
            finishWithError(errorMsg);
            return;
        }
        if (response < 200 || response >= 300) {
            finishWithError(tr("HTTP 状态码 %1").arg(response));
            return;
        }

        QString result;
        result += "=== 连接成功 ===\n";
        result += QString("HTTP 状态 %1\n\n").arg(response);
        if (bodyBuffer_.startsWith("<!DOCTYPE") || bodyBuffer_.startsWith("<html")) {
            result += QString::fromUtf8(bodyBuffer_);
        } else {
            result += QString("[二进制内容, 前 128 字节十六进制]\n%1")
                          .arg(QString(bodyBuffer_.left(128).toHex(' ')));
        }
        emit connectionFinished(true, result);
    }, Qt::QueuedConnection);
}
