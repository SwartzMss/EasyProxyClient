#include "mainwindow.h"
#include <QApplication>
#include <QNetworkProxy>
#include <QSslSocket>
#include <QFile>
#include <QTextStream>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , networkManager(new QNetworkAccessManager(this))
    , currentReply(nullptr)
{
    setupUI();
    setupConnections();
    
    // 设置窗口属性
    setWindowTitle("EasyProxyClient");
    setMinimumSize(600, 500);
    resize(800, 600);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    mainLayout = new QVBoxLayout(centralWidget);
    
    // 目标网址组
    targetGroup = new QGroupBox("目标网址", centralWidget);
    QVBoxLayout *targetLayout = new QVBoxLayout(targetGroup);
    urlEdit = new QLineEdit(targetGroup);
    urlEdit->setPlaceholderText("请输入要访问的网址 (例如: https://example.com)");
    targetLayout->addWidget(urlEdit);
    mainLayout->addWidget(targetGroup);
    
    // 代理设置组
    proxyGroup = new QGroupBox("代理设置", centralWidget);
    QGridLayout *proxyLayout = new QGridLayout(proxyGroup);
    
    proxyLayout->addWidget(new QLabel("代理主机:"), 0, 0);
    proxyHostEdit = new QLineEdit(proxyGroup);
    proxyHostEdit->setPlaceholderText("代理服务器IP或域名");
    proxyLayout->addWidget(proxyHostEdit, 0, 1);
    
    proxyLayout->addWidget(new QLabel("代理端口:"), 0, 2);
    proxyPortEdit = new QLineEdit(proxyGroup);
    proxyPortEdit->setPlaceholderText("端口");
    proxyPortEdit->setText("8080");
    proxyLayout->addWidget(proxyPortEdit, 0, 3);
    
    proxyLayout->addWidget(new QLabel("用户名:"), 1, 0);
    usernameEdit = new QLineEdit(proxyGroup);
    usernameEdit->setPlaceholderText("代理用户名");
    proxyLayout->addWidget(usernameEdit, 1, 1);
    
    proxyLayout->addWidget(new QLabel("密码:"), 1, 2);
    passwordEdit = new QLineEdit(proxyGroup);
    passwordEdit->setPlaceholderText("代理密码");
    passwordEdit->setEchoMode(QLineEdit::Password);
    proxyLayout->addWidget(passwordEdit, 1, 3);
    
    mainLayout->addWidget(proxyGroup);
    
    // 证书设置组
    certificateGroup = new QGroupBox("SSL证书设置", centralWidget);
    QHBoxLayout *certLayout = new QHBoxLayout(certificateGroup);
    
    certificatePathEdit = new QLineEdit(certificateGroup);
    certificatePathEdit->setPlaceholderText("选择自签CA证书文件路径");
    certLayout->addWidget(certificatePathEdit);
    
    browseButton = new QPushButton("浏览", certificateGroup);
    certLayout->addWidget(browseButton);
    
    mainLayout->addWidget(certificateGroup);
    
    // 控制按钮
    QHBoxLayout *controlLayout = new QHBoxLayout();
    connectButton = new QPushButton("连接", centralWidget);
    connectButton->setMinimumHeight(40);
    controlLayout->addWidget(connectButton);
    
    progressBar = new QProgressBar(centralWidget);
    progressBar->setVisible(false);
    controlLayout->addWidget(progressBar);
    
    mainLayout->addLayout(controlLayout);
    
    // 结果显示
    QGroupBox *resultGroup = new QGroupBox("响应结果", centralWidget);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    resultText = new QTextEdit(resultGroup);
    resultText->setReadOnly(true);
    resultLayout->addWidget(resultText);
    mainLayout->addWidget(resultGroup);
}

void MainWindow::setupConnections()
{
    connect(browseButton, &QPushButton::clicked, this, &MainWindow::browseCertificate);
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectToProxy);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::handleNetworkReply);
}

void MainWindow::browseCertificate()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "选择CA证书文件", "", "证书文件 (*.pem *.crt *.cer);;所有文件 (*)");
    if (!fileName.isEmpty()) {
        certificatePathEdit->setText(fileName);
    }
}

void MainWindow::connectToProxy()
{
    // 验证输入
    if (urlEdit->text().isEmpty()) {
        showError("请输入目标网址");
        return;
    }
    
    if (proxyHostEdit->text().isEmpty()) {
        showError("请输入代理主机地址");
        return;
    }
    
    if (proxyPortEdit->text().isEmpty()) {
        showError("请输入代理端口");
        return;
    }
    
    // 清空之前的结果
    resultText->clear();
    
    // 显示进度条
    progressBar->setVisible(true);
    progressBar->setRange(0, 0); // 不确定进度
    connectButton->setEnabled(false);
    
    // 配置代理
    QNetworkProxy proxy;
    proxy.setType(QNetworkProxy::HttpProxy);
    proxy.setHostName(proxyHostEdit->text());
    proxy.setPort(proxyPortEdit->text().toInt());
    
    if (!usernameEdit->text().isEmpty()) {
        proxy.setUser(usernameEdit->text());
        proxy.setPassword(passwordEdit->text());
    }
    
    networkManager->setProxy(proxy);
    
    // 创建请求
    QNetworkRequest request(QUrl(urlEdit->text()));
    
    // 配置SSL
    if (!certificatePathEdit->text().isEmpty()) {
        QSslConfiguration sslConfig = createSslConfiguration();
        request.setSslConfiguration(sslConfig);
    }
    
    // 发送请求
    currentReply = networkManager->get(request);
    
    // 连接SSL错误信号
    connect(currentReply, &QNetworkReply::sslErrors, this, &MainWindow::handleSslErrors);
}

QSslConfiguration MainWindow::createSslConfiguration()
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    
    // 加载CA证书
    QFile certFile(certificatePathEdit->text());
    if (certFile.open(QIODevice::ReadOnly)) {
        QSslCertificate cert(&certFile);
        if (!cert.isNull()) {
            QList<QSslCertificate> caCerts = sslConfig.caCertificates();
            caCerts.append(cert);
            sslConfig.setCaCertificates(caCerts);
        }
        certFile.close();
    }
    
    // 设置SSL选项
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
    
    return sslConfig;
}

void MainWindow::handleNetworkReply(QNetworkReply *reply)
{
    // 隐藏进度条
    progressBar->setVisible(false);
    connectButton->setEnabled(true);
    
    if (reply->error() == QNetworkReply::NoError) {
        // 读取响应内容
        QByteArray data = reply->readAll();
        QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        
        // 显示响应信息
        QString result = QString("状态码: %1\n").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        result += QString("内容类型: %1\n").arg(contentType);
        result += QString("内容长度: %1 字节\n\n").arg(data.size());
        
        // 如果是文本内容，直接显示
        if (contentType.contains("text") || contentType.contains("json") || contentType.contains("xml")) {
            result += QString::fromUtf8(data);
        } else {
            result += "[二进制内容，无法显示]";
        }
        
        resultText->setText(result);
    } else {
        QString errorMsg = QString("网络错误: %1\n%2")
            .arg(reply->error())
            .arg(reply->errorString());
        resultText->setText(errorMsg);
    }
    
    // 清理当前回复对象
    if (reply == currentReply) {
        currentReply = nullptr;
    }
    
    reply->deleteLater();
}

void MainWindow::handleSslErrors(const QList<QSslError> &errors)
{
    QString errorMsg = "SSL错误:\n";
    for (const QSslError &error : errors) {
        errorMsg += QString("- %1\n").arg(error.errorString());
    }
    
    QMessageBox::warning(this, "SSL错误", errorMsg);
    
    // 可以选择忽略SSL错误继续连接
    if (currentReply) {
        currentReply->ignoreSslErrors();
    }
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::warning(this, "错误", message);
} 