#include "remoteclient.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
#include <QXmlStreamReader>

RemoteClient::RemoteClient(QObject *parent)
    : QObject(parent)
{
}

bool RemoteClient::isBusy() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void RemoteClient::list(const RemoteConnection &connection, const QString &path)
{
    if (isBusy()) {
        emit failed(tr("A remote operation is already running."), QString());
        return;
    }

    m_connection = connection;
    m_currentPath = normalizePath(path.isEmpty() ? connection.path : path);
    m_currentUrl = buildUrl(connection, m_currentPath);

    m_process = new QProcess(this);
    m_process->setProgram(QStringLiteral("curl"));
    m_process->setArguments(curlArguments(connection, m_currentUrl));
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RemoteClient::onFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        Q_UNUSED(error)
        emit failed(tr("Unable to start curl."), m_process ? m_process->errorString() : QString());
    });

    emit started(m_currentUrl);
    emit logMessage(tr("Listing %1").arg(m_currentUrl));
    m_process->start();
}

void RemoteClient::cancel()
{
    if (!isBusy()) {
        return;
    }

    m_process->kill();
}

void RemoteClient::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *process = m_process;
    const QByteArray output = process->readAllStandardOutput();
    const QByteArray errorOutput = process->readAllStandardError();

    process->deleteLater();
    m_process = nullptr;

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        emit failed(tr("Remote listing failed."), QString::fromLocal8Bit(errorOutput).trimmed());
        return;
    }

    QVector<RemoteEntry> entries = parseDirectoryListing(m_connection.protocol, m_currentPath, output);
    emit listed(m_currentPath, entries);
    emit logMessage(tr("Loaded %1 entries from %2").arg(entries.size()).arg(m_currentUrl));
}

QString RemoteClient::buildUrl(const RemoteConnection &connection, const QString &path) const
{
    QUrl url;
    QString scheme = connection.protocol.toLower();
    if (scheme == QLatin1String("webdav")) {
        scheme = QStringLiteral("http");
    } else if (scheme == QLatin1String("webdavs")) {
        scheme = QStringLiteral("https");
    }

    url.setScheme(scheme);
    url.setHost(connection.host.trimmed());
    if (connection.port > 0) {
        url.setPort(connection.port);
    }
    url.setPath(normalizePath(path));
    return url.toString(QUrl::FullyEncoded);
}

QString RemoteClient::normalizePath(const QString &path) const
{
    QString result = path.trimmed();
    if (result.isEmpty()) {
        result = QStringLiteral("/");
    }
    if (!result.startsWith(QLatin1Char('/'))) {
        result.prepend(QLatin1Char('/'));
    }
    return QUrl::fromPercentEncoding(result.toUtf8());
}

QString RemoteClient::joinPath(const QString &basePath, const QString &name) const
{
    QString base = normalizePath(basePath);
    if (!base.endsWith(QLatin1Char('/'))) {
        base.append(QLatin1Char('/'));
    }
    return normalizePath(base + name);
}

QStringList RemoteClient::curlArguments(const RemoteConnection &connection, const QString &url) const
{
    QStringList args;
    args << QStringLiteral("--silent")
         << QStringLiteral("--show-error")
         << QStringLiteral("--location")
         << QStringLiteral("--connect-timeout") << QStringLiteral("15")
         << QStringLiteral("--max-time") << QStringLiteral("60");

    if (!connection.username.isEmpty()) {
        args << QStringLiteral("--user") << QStringLiteral("%1:%2").arg(connection.username, connection.password);
    }

    const QString protocol = connection.protocol.toLower();
    if (protocol == QLatin1String("webdav") || protocol == QLatin1String("webdavs")) {
        args << QStringLiteral("--request") << QStringLiteral("PROPFIND")
             << QStringLiteral("--header") << QStringLiteral("Depth: 1")
             << QStringLiteral("--header") << QStringLiteral("Content-Type: application/xml")
             << QStringLiteral("--data")
             << QStringLiteral("<?xml version=\"1.0\"?><d:propfind xmlns:d=\"DAV:\"><d:prop><d:resourcetype/><d:getcontentlength/><d:getlastmodified/><d:displayname/></d:prop></d:propfind>");
    }

    args << url;
    return args;
}

QVector<RemoteEntry> RemoteClient::parseDirectoryListing(const QString &protocol, const QString &path, const QByteArray &data) const
{
    const QString normalizedProtocol = protocol.toLower();
    if (normalizedProtocol == QLatin1String("webdav") || normalizedProtocol == QLatin1String("webdavs")) {
        return parseWebDavListing(path, data);
    }

    return parseUnixListing(path, QString::fromUtf8(data));
}

QVector<RemoteEntry> RemoteClient::parseUnixListing(const QString &path, const QString &text) const
{
    QVector<RemoteEntry> entries;
    QTextStream stream(const_cast<QString *>(&text), QIODevice::ReadOnly);
    const QRegularExpression unixLine(QStringLiteral("^([bcdlps-])[rwxStTs-]{9}\\s+\\S+\\s+\\S+\\s+\\S+\\s+(\\d+)\\s+(\\S+\\s+\\S+\\s+(?:\\S+))\\s+(.+)$"));

    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QStringLiteral("total "))) {
            continue;
        }

        RemoteEntry entry;
        const QRegularExpressionMatch match = unixLine.match(line);
        if (match.hasMatch()) {
            entry.directory = match.captured(1) == QLatin1String("d");
            entry.size = match.captured(2).toLongLong();
            entry.modified = match.captured(3);
            entry.name = match.captured(4);
            if (entry.name.contains(QStringLiteral(" -> "))) {
                entry.name = entry.name.section(QStringLiteral(" -> "), 0, 0);
            }
        } else {
            entry.name = line;
            entry.directory = true;
        }

        if (entry.name == QLatin1String(".") || entry.name == QLatin1String("..")) {
            continue;
        }

        entry.path = joinPath(path, entry.name);
        entries.append(entry);
    }

    return entries;
}

QVector<RemoteEntry> RemoteClient::parseWebDavListing(const QString &path, const QByteArray &data) const
{
    QVector<RemoteEntry> entries;
    QXmlStreamReader xml(data);
    RemoteEntry current;
    QString currentText;
    bool inResponse = false;
    bool inResourceType = false;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            const QString name = xml.name().toString().toLower();
            currentText.clear();
            if (name == QLatin1String("response")) {
                current = RemoteEntry();
                inResponse = true;
            } else if (name == QLatin1String("resourcetype")) {
                inResourceType = true;
            } else if (inResourceType && name == QLatin1String("collection")) {
                current.directory = true;
            }
        } else if (xml.isCharacters() && !xml.isWhitespace()) {
            currentText += xml.text().toString();
        } else if (xml.isEndElement()) {
            const QString name = xml.name().toString().toLower();
            if (!inResponse) {
                continue;
            }

            if (name == QLatin1String("href")) {
                const QUrl hrefUrl = QUrl::fromEncoded(currentText.trimmed().toUtf8());
                current.path = normalizePath(hrefUrl.isValid() && !hrefUrl.path().isEmpty()
                                             ? hrefUrl.path()
                                             : QUrl::fromPercentEncoding(currentText.trimmed().toUtf8()));
                current.name = displayNameFromHref(current.path);
            } else if (name == QLatin1String("displayname") && !currentText.trimmed().isEmpty()) {
                current.name = currentText.trimmed();
            } else if (name == QLatin1String("getcontentlength")) {
                current.size = currentText.trimmed().toLongLong();
            } else if (name == QLatin1String("getlastmodified")) {
                current.modified = currentText.trimmed();
            } else if (name == QLatin1String("resourcetype")) {
                inResourceType = false;
            } else if (name == QLatin1String("response")) {
                const QString normalizedCurrent = normalizePath(current.path);
                const QString normalizedPath = normalizePath(path);
                if (!current.name.isEmpty() && normalizedCurrent != normalizedPath) {
                    if (current.path.isEmpty()) {
                        current.path = joinPath(path, current.name);
                    }
                    entries.append(current);
                }
                inResponse = false;
            }
        }
    }

    if (xml.hasError()) {
        emit const_cast<RemoteClient *>(this)->logMessage(tr("WebDAV XML parse warning: %1").arg(xml.errorString()));
    }

    return entries;
}

QString RemoteClient::displayNameFromHref(const QString &href) const
{
    QString clean = href;
    while (clean.length() > 1 && clean.endsWith(QLatin1Char('/'))) {
        clean.chop(1);
    }
    return QFileInfo(clean).fileName();
}
