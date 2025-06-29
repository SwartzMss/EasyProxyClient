#include "mainwindow.h"
#include <QApplication>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , proxyClient(new ProxyClient(this))
    , configManager(new ConfigManager(this))
{
    setupUI();
    setupMenuBar();
    setupConnections();
    loadConfigToUI();
    
    // 设置窗口属性
    setWindowTitle("EasyProxyClient");
    setMinimumSize(600, 500);
    
    // 从配置加载窗口大小
    int width, height;
    configManager->getWindowSize(width, height);
    resize(width, height);
}

MainWindow::~MainWindow()
{
    saveConfigFromUI();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveConfigFromUI();
    event->accept();
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
    controlLayout = new QHBoxLayout();
    connectButton = new QPushButton("连接", centralWidget);
    connectButton->setMinimumHeight(40);
    controlLayout->addWidget(connectButton);
    
    saveConfigButton = new QPushButton("保存配置", centralWidget);
    saveConfigButton->setMinimumHeight(40);
    controlLayout->addWidget(saveConfigButton);
    
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

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();
    
    // 文件菜单
    fileMenu = menuBar->addMenu("文件(&F)");
    QAction *saveAction = fileMenu->addAction("保存设置(&S)");
    QAction *loadAction = fileMenu->addAction("加载设置(&L)");
    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction("退出(&X)");
    
    // 设置菜单
    settingsMenu = menuBar->addMenu("设置(&S)");
    QAction *resetAction = settingsMenu->addAction("重置为默认值(&R)");
    
    // 帮助菜单
    helpMenu = menuBar->addMenu("帮助(&H)");
    QAction *aboutAction = helpMenu->addAction("关于(&A)");
    
    // 连接菜单信号
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveSettings);
    connect(loadAction, &QAction::triggered, this, &MainWindow::loadSettings);
    connect(exitAction, &QAction::triggered, []() { QApplication::quit(); });
    connect(resetAction, &QAction::triggered, this, &MainWindow::resetSettings);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::about);
}

void MainWindow::setupConnections()
{
    connect(browseButton, &QPushButton::clicked, this, &MainWindow::browseCertificate);
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectToProxy);
    connect(saveConfigButton, &QPushButton::clicked, this, &MainWindow::saveConfigButtonClicked);
    
    // 连接代理客户端信号
    connect(proxyClient, &ProxyClient::connectionStarted, this, &MainWindow::onConnectionStarted);
    connect(proxyClient, &ProxyClient::connectionFinished, this, &MainWindow::onConnectionFinished);
    connect(proxyClient, &ProxyClient::sslErrors, this, &MainWindow::onSslErrors);
    connect(proxyClient, &ProxyClient::networkError, this, &MainWindow::onNetworkError);
}

void MainWindow::loadConfigToUI()
{
    // 加载代理设置
    proxyHostEdit->setText(configManager->getProxyHost());
    proxyPortEdit->setText(QString::number(configManager->getProxyPort()));
    usernameEdit->setText(configManager->getProxyUsername());
    passwordEdit->setText(configManager->getProxyPassword());
    
    // 加载SSL证书设置
    certificatePathEdit->setText(configManager->getCertificatePath());
    
    // 加载最后使用的URL
    urlEdit->setText(configManager->getLastUrl());
}

void MainWindow::saveConfigFromUI()
{
    // 保存代理设置
    configManager->setProxyHost(proxyHostEdit->text());
    configManager->setProxyPort(proxyPortEdit->text().toInt());
    configManager->setProxyUsername(usernameEdit->text());
    configManager->setProxyPassword(passwordEdit->text());
    
    // 保存SSL证书设置
    configManager->setCertificatePath(certificatePathEdit->text());
    
    // 保存最后使用的URL
    configManager->setLastUrl(urlEdit->text());
    
    // 保存窗口大小
    configManager->setWindowSize(width(), height());
    
    // 保存配置
    configManager->saveConfig();
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
    // 基本输入验证
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
    
    // 配置代理客户端
    proxyClient->setProxySettings(
        proxyHostEdit->text(),
        proxyPortEdit->text().toInt(),
        usernameEdit->text(),
        passwordEdit->text()
    );
    
    // 配置SSL证书
    if (!certificatePathEdit->text().isEmpty()) {
        proxyClient->setSslCertificate(certificatePathEdit->text());
    }
    
    // 发起连接
    proxyClient->connectToUrl(urlEdit->text());
}

void MainWindow::onConnectionStarted()
{
    // 显示进度条
    progressBar->setVisible(true);
    progressBar->setRange(0, 0); // 不确定进度
    connectButton->setEnabled(false);
}

void MainWindow::onConnectionFinished(bool success, const QString &result)
{
    // 隐藏进度条
    progressBar->setVisible(false);
    connectButton->setEnabled(true);
    
    // 显示结果
    resultText->setText(result);
}

void MainWindow::onSslErrors(const QString &errorMessage)
{
    QMessageBox::warning(this, "SSL错误", errorMessage);
}

void MainWindow::onNetworkError(const QString &errorMessage)
{
    showError(errorMessage);
}

void MainWindow::saveSettings()
{
    saveConfigFromUI();
    QMessageBox::information(this, "保存设置", "设置已保存");
}

void MainWindow::loadSettings()
{
    loadConfigToUI();
    QMessageBox::information(this, "加载设置", "设置已加载");
}

void MainWindow::resetSettings()
{
    int ret = QMessageBox::question(this, "重置设置", 
                                   "确定要重置所有设置为默认值吗？",
                                   QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        configManager->resetToDefaults();
        loadConfigToUI();
        QMessageBox::information(this, "重置设置", "设置已重置为默认值");
    }
}

void MainWindow::about()
{
    QMessageBox::about(this, "关于 EasyProxyClient",
        "EasyProxyClient v1.0.0\n\n"
        "一个用于连接 EasyProxy 的客户端程序\n"
        "支持 HTTP 代理和 SSL 证书配置\n\n"
        "基于 Qt 6 开发\n"
        "MIT License");
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::warning(this, "错误", message);
}

void MainWindow::saveConfigButtonClicked()
{
    saveConfigFromUI();
    QMessageBox::information(this, "保存配置", 
        QString("配置已保存到:\n%1").arg(QApplication::applicationDirPath() + "/config.ini"));
} 