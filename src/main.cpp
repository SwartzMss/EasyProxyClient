#include <QApplication>
#include <QStyleFactory>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("EasyProxyClient");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("EasyProxyClient");
    
    // 设置应用程序样式
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // 创建并显示主窗口
    MainWindow window;
    window.show();
    
    return app.exec();
} 