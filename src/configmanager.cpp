#include "configmanager.h"
#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>

// 默认值定义
const QString ConfigManager::DEFAULT_PROXY_HOST = "127.0.0.1";
const int ConfigManager::DEFAULT_PROXY_PORT = 8080;
const QString ConfigManager::DEFAULT_PROXY_USERNAME = "";
const QString ConfigManager::DEFAULT_PROXY_PASSWORD = "";
const QString ConfigManager::DEFAULT_CERTIFICATE_PATH = "";
const QString ConfigManager::DEFAULT_LAST_URL = "https://example.com";
const int ConfigManager::DEFAULT_WINDOW_WIDTH = 800;
const int ConfigManager::DEFAULT_WINDOW_HEIGHT = 600;

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , settings(nullptr)
{
    // 获取exe所在目录
    QString exePath = QApplication::applicationDirPath();
    QString configPath = exePath + "/config.ini";
    
    // 创建QSettings对象，保存到exe同级目录
    settings = new QSettings(configPath, QSettings::IniFormat, this);
    
    loadConfig();
}

ConfigManager::~ConfigManager()
{
    saveConfig();
}

// 代理设置
void ConfigManager::setProxyHost(const QString &host)
{
    settings->setValue("proxy/host", host);
}

void ConfigManager::setProxyPort(int port)
{
    settings->setValue("proxy/port", port);
}

void ConfigManager::setProxyUsername(const QString &username)
{
    settings->setValue("proxy/username", username);
}

void ConfigManager::setProxyPassword(const QString &password)
{
    settings->setValue("proxy/password", password);
}

QString ConfigManager::getProxyHost() const
{
    return settings->value("proxy/host", DEFAULT_PROXY_HOST).toString();
}

int ConfigManager::getProxyPort() const
{
    return settings->value("proxy/port", DEFAULT_PROXY_PORT).toInt();
}

QString ConfigManager::getProxyUsername() const
{
    return settings->value("proxy/username", DEFAULT_PROXY_USERNAME).toString();
}

QString ConfigManager::getProxyPassword() const
{
    return settings->value("proxy/password", DEFAULT_PROXY_PASSWORD).toString();
}

// SSL证书设置
void ConfigManager::setCertificatePath(const QString &path)
{
    settings->setValue("ssl/certificate_path", path);
}

QString ConfigManager::getCertificatePath() const
{
    return settings->value("ssl/certificate_path", DEFAULT_CERTIFICATE_PATH).toString();
}

// 目标URL设置
void ConfigManager::setLastUrl(const QString &url)
{
    settings->setValue("ui/last_url", url);
}

QString ConfigManager::getLastUrl() const
{
    return settings->value("ui/last_url", DEFAULT_LAST_URL).toString();
}

// 窗口设置
void ConfigManager::setWindowSize(int width, int height)
{
    settings->setValue("ui/window_width", width);
    settings->setValue("ui/window_height", height);
}

void ConfigManager::getWindowSize(int &width, int &height) const
{
    width = settings->value("ui/window_width", DEFAULT_WINDOW_WIDTH).toInt();
    height = settings->value("ui/window_height", DEFAULT_WINDOW_HEIGHT).toInt();
}

// 保存和加载配置
void ConfigManager::saveConfig()
{
    settings->sync();
    qDebug() << "配置已保存到:" << settings->fileName();
}

void ConfigManager::loadConfig()
{
    qDebug() << "配置已从以下位置加载:" << settings->fileName();
}

// 重置为默认值
void ConfigManager::resetToDefaults()
{
    settings->clear();
    
    // 设置默认值
    setProxyHost(DEFAULT_PROXY_HOST);
    setProxyPort(DEFAULT_PROXY_PORT);
    setProxyUsername(DEFAULT_PROXY_USERNAME);
    setProxyPassword(DEFAULT_PROXY_PASSWORD);
    setCertificatePath(DEFAULT_CERTIFICATE_PATH);
    setLastUrl(DEFAULT_LAST_URL);
    setWindowSize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    
    saveConfig();
    qDebug() << "配置已重置为默认值";
} 