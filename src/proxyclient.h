#ifndef PROXYCLIENT_H
#define PROXYCLIENT_H

#include <QObject>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QTimer>
#include <QStringList>

class ProxyClient : public QObject
{
    Q_OBJECT

public:
    explicit ProxyClient(QObject *parent = nullptr);
    ~ProxyClient() override;

    // ---- Public API ---------------------------------------------------
    void setProxySettings(const QString &host, int port,
                          const QString &username = {},
                          const QString &password = {});
    void setSslCertificate(const QString &certificatePath);
    void connectToUrl(const QString &url);          // begin full TLS→CONNECT→TLS flow
    void cancelRequest();
    bool isConnecting() const { return connecting_; }

signals:
    void connectionStarted();
    void connectionFinished(bool success, const QString &result);
    void sslErrors(const QString &errorMessage);
    void networkError(const QString &errorMessage);
    void debugMessage(const QString &message);

private slots:
    // QSslSocket handlers
    void onSocketConnected();
    void onSocketEncrypted();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSslErrors(const QList<QSslError> &errors);

    // timeout guard
    void onConnectionTimeout();

private:
    // ---- Internal helpers --------------------------------------------
    enum class Stage {
        ProxyTlsHandshake,
        WaitProxyResponse,
        TargetTlsHandshake,
        Ready
    };

    QSslConfiguration buildSslConfiguration() const;
    void sendConnectRequest();
    bool tryParseConnectResponse();           // returns true when CONNECT finished
    bool tryParseHttpResponse();              // returns true when HTTP finished
    void appendDebug(const QString &msg);
    void finishWithError(const QString &msg);

    // ---- Data ---------------------------------------------------------
    QSslSocket   *socket_         { nullptr };
    QTimer       *timer_          { nullptr };

    // proxy
    QString proxyHost_;
    int     proxyPort_            { 8080 };
    QString proxyUser_;
    QString proxyPass_;

    // certificate
    QString caPath_;

    // target
    QString targetUrl_;
    QString targetHost_;
    int     targetPort_           { 443 };

    // state
    bool  connecting_             { false };
    Stage stage_                  { Stage::ProxyTlsHandshake };
    QByteArray buffer_;                               // unified receive buffer

    // debug
    QStringList debugLines_;
};
#endif // PROXYCLIENT_H
