#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QSettings>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();

    // 代理设置
    void setProxyHost(const QString &host);
    void setProxyPort(int port);
    void setProxyUsername(const QString &username);
    void setProxyPassword(const QString &password);
    
    QString getProxyHost() const;
    int getProxyPort() const;
    QString getProxyUsername() const;
    QString getProxyPassword() const;
    
    // SSL证书设置
    void setCertificatePath(const QString &path);
    QString getCertificatePath() const;
    
    // 目标URL设置
    void setLastUrl(const QString &url);
    QString getLastUrl() const;
    
    // 窗口设置
    void setWindowSize(int width, int height);
    void getWindowSize(int &width, int &height) const;
    
    // 保存和加载配置
    void saveConfig();
    void loadConfig();
    
    // 重置为默认值
    void resetToDefaults();

private:
    QSettings *settings;
    
    // 默认值
    static const QString DEFAULT_PROXY_HOST;
    static const int DEFAULT_PROXY_PORT;
    static const QString DEFAULT_PROXY_USERNAME;
    static const QString DEFAULT_PROXY_PASSWORD;
    static const QString DEFAULT_CERTIFICATE_PATH;
    static const QString DEFAULT_LAST_URL;
    static const int DEFAULT_WINDOW_WIDTH;
    static const int DEFAULT_WINDOW_HEIGHT;
};

#endif // CONFIGMANAGER_H 