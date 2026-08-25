QT += core gui widgets network xml

CONFIG += c++11 link_pkgconfig
PKGCONFIG += dtk2widget dtk6core

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
translationfiles.files = translations/remote-file-dtk2_zh_CN.qm
translationfiles.path = /usr/share/gxde-filetransfer/translations

INSTALLS += target desktop icons translationfiles
