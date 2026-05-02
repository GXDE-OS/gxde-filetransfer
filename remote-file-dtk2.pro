QT += core gui widgets network xml

CONFIG += c++11 link_pkgconfig
PKGCONFIG += dtkwidget dtkcore

TARGET = remote-file-dtk2
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/remoteclient.cpp

HEADERS += \
    src/mainwindow.h \
    src/remoteclient.h

RESOURCES += \
    translations.qrc

TRANSLATIONS += \
    translations/remote-file-dtk2_zh_CN.ts
