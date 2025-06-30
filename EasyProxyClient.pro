QT += core widgets network concurrent

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

# Windows配置
CONFIG += windows

# libcurl配置
INCLUDEPATH += $$PWD/depend/libcurl/include
LIBS += -L$$PWD/depend/libcurl/lib -llibcurl

# 确保链接器能找到库文件
win32-msvc* {
    LIBS += $$PWD/depend/libcurl/lib/libcurl.lib
}

QMAKE_POST_LINK += $$quote(cmd /c copy /y \"$$PWD\\depend\\libcurl\\bin\\libcurl.dll\" \"$$shell_path($$DESTDIR)\" && copy /y \"$$PWD\\depend\\libcurl\\bin\\zlib1.dll\" \"$$shell_path($$DESTDIR)\" && copy /y \"$$PWD\\depend\\libcurl\\bin\\libssl-3-x64.dll\" \"$$shell_path($$DESTDIR)\" && copy /y \"$$PWD\\depend\\libcurl\\bin\\libcrypto-3-x64.dll\" \"$$shell_path($$DESTDIR)\")
