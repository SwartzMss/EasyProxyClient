#ifndef PROXYCLIENT_H
#define PROXYCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslError>
#include <QString>
#include <QTimer>
#include <QStringList>

class ProxyClient : public QObject
{
    Q_OBJECT

public:
    explicit ProxyClient(QObject *parent = nullptr);
    ~ProxyClient();

    // 配置代理设置
    void setProxySettings(const QString &host, int port,
                         const QString &username = QString(),
                         const QString &password = QString());
    
    // 配置SSL证书
    void setSslCertificate(const QString &certificatePath);
    
    // 发起网络请求
    void connectToUrl(const QString &url);
    
    // 取消当前请求
    void cancelRequest();
    
    // 检查是否正在连接
    bool isConnecting() const;

signals:
    // 连接开始
    void connectionStarted();
    
    // 连接完成
    void connectionFinished(bool success, const QString &result);
    
    // SSL错误
    void sslErrors(const QString &errorMessage);
    
    // 网络错误
    void networkError(const QString &errorMessage);
    
    // 实时调试信息
    void debugMessage(const QString &message);

private slots:
    void handleReplyFinished();
    void handleReplySslErrors(const QList<QSslError> &errors);
    void handleReplyReadyRead();
    void handleConnectionTimeout();

private:
    QSslConfiguration createSslConfiguration();
    void showError(const QString &message);

    QNetworkAccessManager *networkManager;
    QNetworkReply *networkReply;
    QTimer *connectionTimer;
    
    // 代理设置
    QString proxyHost;
    int proxyPort;
    QString proxyUsername;
    QString proxyPassword;
    
    // SSL设置
    QString certificatePath;
    
    // 目标URL
    QString targetUrl;
    
    // 状态
    bool connecting;
    
    // 缓冲区
    QByteArray responseBuffer;
    
    // 调试信息收集
    QStringList debugMessages;
    void addDebugMessage(const QString &message);
};

#endif // PROXYCLIENT_H 