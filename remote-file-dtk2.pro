QT += core gui widgets network xml

CONFIG += c++11 link_pkgconfig
PKGCONFIG += dtkwidget dtkcore

TARGET = gxde-filetransfer
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/remoteclient.cpp

HEADERS += \
    src/mainwindow.h \
    src/remoteclient.h

RESOURCES += \
    translations.qrc \
    app.qrc

TRANSLATIONS += \
    translations/remote-file-dtk2_zh_CN.ts

target.path = /usr/bin
desktop.files = data/gxde-filetransfer.desktop
desktop.path = /usr/share/applications
icons.files = assets/gxde-filetransfer.svg
icons.path = /usr/share/icons/hicolor/scalable/apps

INSTALLS += target desktop icons
