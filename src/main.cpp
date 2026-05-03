#include "mainwindow.h"

#include <DApplication>
#include <DLog>

#include <QCommandLineParser>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    DApplication::loadDXcbPlugin();

    DApplication app(argc, argv);
    app.setOrganizationName("gxde");
    app.setApplicationName("gxde-filetransfer");
    app.setApplicationDisplayName("GXDE File Transfer");
    app.setApplicationVersion("0.1");
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/gxde-filetransfer.svg")));
    app.loadTranslator();

    QTranslator translator;
    if (translator.load(QStringLiteral(":/translations/remote-file-dtk2_%1.qm").arg(QLocale::system().name()))) {
        app.installTranslator(&translator);
    }

    Dtk::Core::DLogManager::registerConsoleAppender();

    QCommandLineParser parser;
    parser.setApplicationDescription("GXDE file transfer client for FTP, SFTP and WebDAV.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    MainWindow window;
    window.resize(1080, 680);
    window.show();

    return app.exec();
}
