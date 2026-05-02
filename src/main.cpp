#include "mainwindow.h"

#include <DApplication>
#include <DLog>

#include <QCommandLineParser>
#include <QLocale>
#include <QTranslator>

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    DApplication::loadDXcbPlugin();

    DApplication app(argc, argv);
    app.setOrganizationName("remote-file-dtk2");
    app.setApplicationName("Remote File DTK2");
    app.setApplicationDisplayName("Remote File DTK2");
    app.setApplicationVersion("0.1.0");
    app.loadTranslator();

    QTranslator translator;
    if (translator.load(QStringLiteral(":/translations/remote-file-dtk2_%1.qm").arg(QLocale::system().name()))) {
        app.installTranslator(&translator);
    }

    Dtk::Core::DLogManager::registerConsoleAppender();

    QCommandLineParser parser;
    parser.setApplicationDescription("DTK2 remote file client for FTP, SFTP and WebDAV.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    MainWindow window;
    window.resize(1080, 680);
    window.show();

    return app.exec();
}
