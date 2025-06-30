#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QProgressBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include "proxyclient.h"
#include "configmanager.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void browseCertificate();
    void connectToProxy();
    void onConnectionStarted();
    void onConnectionFinished(bool success, const QString &result);
    void onNetworkError(const QString &errorMessage);
    void onDebugMessage(const QString &message);
    
    // 配置相关槽函数
    void saveSettings();
    void loadSettings();
    void resetSettings();
    void about();
    void saveConfigButtonClicked();

private:
    void setupUI();
    void setupMenuBar();
    void setupConnections();
    void showError(const QString &message);
    void loadConfigToUI();
    void saveConfigFromUI();

    // UI组件
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    
    // 目标网址
    QGroupBox *targetGroup;
    QLineEdit *urlEdit;
    
    // 代理设置
    QGroupBox *proxyGroup;
    QLineEdit *proxyHostEdit;
    QLineEdit *proxyPortEdit;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    
    // 证书设置
    QGroupBox *certificateGroup;
    QLineEdit *certificatePathEdit;
    QPushButton *browseButton;
    
    // 控制按钮
    QHBoxLayout *controlLayout;
    QPushButton *connectButton;
    QPushButton *saveConfigButton;
    QProgressBar *progressBar;
    
    // 调试信息显示
    QTextEdit *debugText;
    
    // 菜单
    QMenu *fileMenu;
    QMenu *settingsMenu;
    QMenu *helpMenu;
    
    // 代理客户端
    ProxyClient *proxyClient;
    
    // 配置管理器
    ConfigManager *configManager;
};

#endif // MAINWINDOW_H 