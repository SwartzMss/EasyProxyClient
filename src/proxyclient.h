#ifndef PROXYCLIENT_H
#define PROXYCLIENT_H

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <curl/curl.h>

class ProxyClient : public QObject
{
    Q_OBJECT
public:
    explicit ProxyClient(QObject *parent = nullptr);
    ~ProxyClient() override;

    void setProxySettings(const QString &host, int port,
                          const QString &username = {},
                          const QString &password = {});
    void setSslCertificate(const QString &certificatePath);
    void connectToUrl(const QString &url);
    void cancelRequest();
    bool isConnecting() const { return connecting_; }

signals:
    void connectionStarted();
    void connectionFinished(bool success, const QString &result);
    void networkError(const QString &errorMessage);
    void debugMessage(const QString &message);

private:
    void appendDebug(const QString &msg);
    void finishWithError(const QString &msg);
    void performRequest();
    static size_t headerCallback(char *buffer, size_t size, size_t nitems, void *userdata);
    static size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata);

    QThread *worker_ { nullptr };
    QTimer  *timer_  { nullptr };

    QString proxyHost_;
    int     proxyPort_ { 8080 };
    QString proxyUser_;
    QString proxyPass_;

    QString caPath_;
    QString targetUrl_;

    QByteArray headerBuffer_;
    QByteArray bodyBuffer_;
    bool connecting_ { false };
    QStringList debugLines_;
};

#endif // PROXYCLIENT_H
