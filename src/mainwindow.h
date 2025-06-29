#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QFileDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QProgressBar>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void browseCertificate();
    void connectToProxy();
    void handleNetworkReply(QNetworkReply *reply);
    void handleSslErrors(const QList<QSslError> &errors);

private:
    void setupUI();
    void setupConnections();
    QSslConfiguration createSslConfiguration();
    void showError(const QString &message);

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
    QPushButton *connectButton;
    QProgressBar *progressBar;
    
    // 结果显示
    QTextEdit *resultText;
    
    // 网络管理器
    QNetworkAccessManager *networkManager;
    
    // 当前网络回复对象
    QNetworkReply *currentReply;
};

#endif // MAINWINDOW_H 