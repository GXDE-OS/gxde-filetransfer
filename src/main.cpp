#include "mainwindow.h"

#include <DApplication>
#include <DLog>

#include <QCommandLineParser>
#include <QCoreApplication>
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
    const QIcon appIcon(QStringLiteral(":/icons/gxde-filetransfer.svg"));
    app.setWindowIcon(appIcon);
    app.setProductIcon(appIcon);
    app.loadTranslator();

    QTranslator translator;
    const QString translatorName = QStringLiteral("remote-file-dtk2_%1.qm").arg(QLocale::system().name());
    if (translator.load(QStringLiteral("/usr/share/gxde-filetransfer/translations/%1").arg(translatorName))
        || translator.load(QStringLiteral(":/translations/%1").arg(translatorName))) {
        app.installTranslator(&translator);
    }

    const QString description = QCoreApplication::translate("Application", "A simple and easy-to-use FTP, SFTP, WebDAV and WebDAVS client.");
    app.setApplicationDescription(description);

    Dtk::Core::DLogManager::registerConsoleAppender();

    QCommandLineParser parser;
    parser.setApplicationDescription(description);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    MainWindow window;
    window.resize(1080, 680);
    window.show();

    return app.exec();
}
