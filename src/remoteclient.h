#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QVector>

struct RemoteConnection
{
    QString protocol;
    QString host;
    int port = 0;
    QString username;
    QString password;
    QString path;
};

struct RemoteEntry
{
    QString name;
    QString path;
    bool directory = false;
    qint64 size = -1;
    QString modified;
};

class RemoteClient : public QObject
{
    Q_OBJECT

public:
    explicit RemoteClient(QObject *parent = nullptr);

    bool isBusy() const;
    void list(const RemoteConnection &connection, const QString &path);
    void download(const RemoteConnection &connection, const QString &remotePath, const QString &localPath);
    void upload(const RemoteConnection &connection, const QString &localPath, const QString &remotePath);
    void cancel();

signals:
    void started(const QString &url);
    void listed(const QString &path, const QVector<RemoteEntry> &entries);
    void transferFinished(const QString &source, const QString &destination);
    void failed(const QString &message, const QString &details);
    void logMessage(const QString &message);

private slots:
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum Operation {
        ListOperation,
        DownloadOperation,
        UploadOperation
    };

    QString buildUrl(const RemoteConnection &connection, const QString &path) const;
    QString normalizePath(const QString &path) const;
    QString joinPath(const QString &basePath, const QString &name) const;
    QStringList curlArguments(const RemoteConnection &connection, const QString &url) const;
    QVector<RemoteEntry> parseDirectoryListing(const QString &protocol, const QString &path, const QByteArray &data) const;
    QVector<RemoteEntry> parseUnixListing(const QString &path, const QString &text) const;
    QVector<RemoteEntry> parseWebDavListing(const QString &path, const QByteArray &data) const;
    QString displayNameFromHref(const QString &href) const;

    QProcess *m_process = nullptr;
    Operation m_operation = ListOperation;
    RemoteConnection m_connection;
    QString m_currentPath;
    QString m_currentUrl;
    QString m_transferSource;
    QString m_transferDestination;
};
