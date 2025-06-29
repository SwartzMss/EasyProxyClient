QT += core widgets network

CONFIG += c++17

# 项目信息
TARGET = EasyProxyClient
TEMPLATE = app

# 源文件
SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp

HEADERS += \
    src/mainwindow.h

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

# 编译器警告设置
win32-msvc {
    QMAKE_CXXFLAGS += /W4
}

# 定义
DEFINES += QT_DEPRECATED_WARNINGS

# 自动生成的文件目录
MOC_DIR = build/moc
RCC_DIR = build/rcc
UI_DIR = build/ui
OBJECTS_DIR = build/obj

# 清理构建目录
QMAKE_CLEAN += -r build/ 