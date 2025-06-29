QT += core widgets network

CONFIG += c++11

# 项目信息
TARGET = EasyProxyClient
TEMPLATE = app

# 源文件
SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/proxyclient.cpp \
    src/configmanager.cpp

HEADERS += \
    src/mainwindow.h \
    src/proxyclient.h \
    src/configmanager.h

# Windows特定设置
win32 {
    CONFIG += windows
}

# 输出目录设置
DESTDIR = $$PWD/bin

# 调试和发布配置
CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/bin/debug
} else {
    DESTDIR = $$PWD/bin/release
}

# 自动生成的文件目录
MOC_DIR = build/moc
RCC_DIR = build/rcc
UI_DIR = build/ui
OBJECTS_DIR = build/obj

# 清理构建目录
QMAKE_CLEAN += -r build/ 