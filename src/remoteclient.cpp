#include "remoteclient.h"

#include <QFileInfo>
#include <QDir>
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
    m_operation = ListOperation;
    m_cancelled = false;
    m_currentPath = path.isEmpty() ? connection.path.trimmed() : normalizePath(path);
    if (m_currentPath != QLatin1String("/") && !m_currentPath.endsWith(QLatin1Char('/'))) {
        m_currentPath.append(QLatin1Char('/'));
    }
    m_currentUrl = buildUrl(connection, m_currentPath);

    m_authenticationMode = BasicAuthentication;
    m_canRetryAnyAuth = isWebDavConnection(connection) && !connection.username.isEmpty();

    emit started(m_currentUrl);
    emit logMessage(tr("Listing %1").arg(m_currentUrl));
    startCurrentOperation();
}

void RemoteClient::download(const RemoteConnection &connection, const QString &remotePath, const QString &localPath, bool resume)
{
    if (isBusy()) {
        emit failed(tr("A remote operation is already running."), QString());
        return;
    }

    m_connection = connection;
    m_operation = DownloadOperation;
    m_cancelled = false;
    m_currentPath = normalizePath(remotePath);
    m_currentUrl = buildUrl(connection, m_currentPath);
    m_transferSource = m_currentUrl;
    m_transferDestination = localPath;
    m_resumeDownload = resume;
    m_authenticationMode = BasicAuthentication;
    m_canRetryAnyAuth = isWebDavConnection(connection) && !connection.username.isEmpty();

    emit started(m_currentUrl);
    emit logMessage(tr("Downloading %1 to %2").arg(m_currentUrl, localPath));
    startCurrentOperation();
}

void RemoteClient::makeDirectory(const RemoteConnection &connection, const QString &remotePath)
{
    if (isBusy()) {
        emit failed(tr("A remote operation is already running."), QString());
        return;
    }

    m_connection = connection;
    m_operation = MakeDirectoryOperation;
    m_cancelled = false;
    m_directoryPath = normalizePath(remotePath);
    if (!m_directoryPath.endsWith(QLatin1Char('/'))) {
        m_directoryPath.append(QLatin1Char('/'));
    }
    m_currentUrl = buildUrl(connection, m_directoryPath);
    m_transferSource = QString();
    m_transferDestination = m_currentUrl;
    m_authenticationMode = BasicAuthentication;
    m_canRetryAnyAuth = isWebDavConnection(connection) && !connection.username.isEmpty();

    emit started(m_currentUrl);
    emit logMessage(tr("Creating remote folder %1").arg(m_currentUrl));
    startCurrentOperation();
}

void RemoteClient::upload(const RemoteConnection &connection, const QString &localPath, const QString &remotePath)
{
    if (isBusy()) {
        emit failed(tr("A remote operation is already running."), QString());
        return;
    }

    m_connection = connection;
    m_operation = UploadOperation;
    m_cancelled = false;
    m_currentPath = normalizePath(remotePath);
    m_currentUrl = buildUrl(connection, m_currentPath);
    m_transferSource = localPath;
    m_transferDestination = m_currentUrl;
    m_authenticationMode = BasicAuthentication;
    m_canRetryAnyAuth = isWebDavConnection(connection) && !connection.username.isEmpty();

    emit started(m_currentUrl);
    emit logMessage(tr("Uploading %1 to %2").arg(localPath, m_currentUrl));
    startCurrentOperation();
}

void RemoteClient::remove(const RemoteConnection &connection, const QString &remotePath, bool directory)
{
    if (isBusy()) {
        emit failed(tr("A remote operation is already running."), QString());
        return;
    }

    m_connection = connection;
    m_operation = RemoveOperation;
    m_cancelled = false;
    m_removePath = normalizePath(remotePath);
    m_removeDirectory = directory;
    m_currentUrl = buildUrl(connection, m_removePath);
    m_authenticationMode = BasicAuthentication;
    m_canRetryAnyAuth = isWebDavConnection(connection) && !connection.username.isEmpty();

    emit started(m_currentUrl);
    emit logMessage(tr("Deleting %1").arg(m_currentUrl));
    startCurrentOperation();
}

void RemoteClient::move(const RemoteConnection &connection, const QString &remotePath, const QString &destinationPath)
{
    if (isBusy()) {
        emit failed(tr("A remote operation is already running."), QString());
        return;
    }

    m_connection = connection;
    m_operation = MoveOperation;
    m_cancelled = false;
    m_moveSourcePath = normalizePath(remotePath);
    m_moveDestinationPath = normalizePath(destinationPath);
    m_currentUrl = buildUrl(connection, m_moveSourcePath);
    m_authenticationMode = BasicAuthentication;
    m_canRetryAnyAuth = isWebDavConnection(connection) && !connection.username.isEmpty();

    emit started(m_currentUrl);
    emit logMessage(tr("Moving %1 to %2").arg(m_moveSourcePath, m_moveDestinationPath));
    startCurrentOperation();
}

void RemoteClient::cancel()
{
    if (!isBusy()) {
        return;
    }

    m_cancelled = true;
    m_process->kill();
}

void RemoteClient::startCurrentOperation()
{
    QStringList args;
    const QString protocol = m_connection.protocol.toLower();
    const bool webDav = isWebDavConnection(m_connection);

    switch (m_operation) {
    case ListOperation:
        args = curlArguments(m_connection, m_currentUrl, m_authenticationMode);
        break;
    case DownloadOperation:
        args << QStringLiteral("--show-error")
             << QStringLiteral("--fail")
             << QStringLiteral("--globoff")
             << QStringLiteral("--ftp-create-dirs")
             << QStringLiteral("--connect-timeout") << QStringLiteral("15")
             << QStringLiteral("--max-time") << QStringLiteral("0");
        addLocationArguments(&args, m_connection);
        addAuthenticationArguments(&args, m_connection, m_authenticationMode);
        if (m_resumeDownload) {
            args << QStringLiteral("--continue-at") << QStringLiteral("-");
        }
        args << QStringLiteral("--output") << m_transferDestination << m_currentUrl;
        break;
    case MakeDirectoryOperation:
        args << QStringLiteral("--silent")
             << QStringLiteral("--show-error")
             << QStringLiteral("--fail")
             << QStringLiteral("--globoff")
             << QStringLiteral("--connect-timeout") << QStringLiteral("15")
             << QStringLiteral("--max-time") << QStringLiteral("60");
        addLocationArguments(&args, m_connection);
        addAuthenticationArguments(&args, m_connection, m_authenticationMode);
        if (webDav) {
            args << QStringLiteral("--request") << QStringLiteral("MKCOL") << m_currentUrl;
        } else if (protocol == QLatin1String("sftp")) {
            args << QStringLiteral("--quote") << QStringLiteral("mkdir %1").arg(m_directoryPath)
                 << buildUrl(m_connection, QStringLiteral("/"));
        } else {
            args << QStringLiteral("--quote") << QStringLiteral("MKD %1").arg(m_directoryPath)
                 << buildUrl(m_connection, QStringLiteral("/"));
        }
        break;
    case UploadOperation:
        args << QStringLiteral("--show-error")
             << QStringLiteral("--fail")
             << QStringLiteral("--globoff")
             << QStringLiteral("--ftp-create-dirs")
             << QStringLiteral("--connect-timeout") << QStringLiteral("15")
             << QStringLiteral("--max-time") << QStringLiteral("0");
        addLocationArguments(&args, m_connection);
        addAuthenticationArguments(&args, m_connection, m_authenticationMode);
        args << QStringLiteral("--upload-file") << m_transferSource << m_currentUrl;
        break;
    case RemoveOperation:
        args << QStringLiteral("--silent")
             << QStringLiteral("--show-error")
             << QStringLiteral("--fail")
             << QStringLiteral("--globoff")
             << QStringLiteral("--connect-timeout") << QStringLiteral("15")
             << QStringLiteral("--max-time") << QStringLiteral("60");
        addLocationArguments(&args, m_connection);
        addAuthenticationArguments(&args, m_connection, m_authenticationMode);
        if (webDav) {
            args << QStringLiteral("--request") << QStringLiteral("DELETE") << m_currentUrl;
        } else {
            args << QStringLiteral("--quote") << QStringLiteral("%1 %2").arg(m_removeDirectory ? QStringLiteral("rmdir") : QStringLiteral("rm"), m_removePath)
                 << buildUrl(m_connection, QStringLiteral("/"));
        }
        break;
    case MoveOperation:
        args << QStringLiteral("--silent")
             << QStringLiteral("--show-error")
             << QStringLiteral("--fail")
             << QStringLiteral("--globoff")
             << QStringLiteral("--connect-timeout") << QStringLiteral("15")
             << QStringLiteral("--max-time") << QStringLiteral("60");
        addLocationArguments(&args, m_connection);
        addAuthenticationArguments(&args, m_connection, m_authenticationMode);
        if (webDav) {
            args << QStringLiteral("--request") << QStringLiteral("MOVE")
                 << QStringLiteral("--header") << QStringLiteral("Destination: %1").arg(buildUrl(m_connection, m_moveDestinationPath))
                 << m_currentUrl;
        } else if (protocol == QLatin1String("sftp")) {
            args << QStringLiteral("--quote") << QStringLiteral("rename %1 %2").arg(m_moveSourcePath, m_moveDestinationPath)
                 << buildUrl(m_connection, QStringLiteral("/"));
        } else {
            args << QStringLiteral("--quote") << QStringLiteral("RNFR %1").arg(m_moveSourcePath)
                 << QStringLiteral("--quote") << QStringLiteral("RNTO %1").arg(m_moveDestinationPath)
                 << buildUrl(m_connection, QStringLiteral("/"));
        }
        break;
    }

    m_process = new QProcess(this);
    m_process->setProgram(QStringLiteral("curl"));
    addDiagnosticsArguments(&args);
    m_process->setArguments(args);
    m_lastCurlArguments = args;
    m_errorBuffer.clear();
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RemoteClient::onFinished);
    if (m_operation == DownloadOperation || m_operation == UploadOperation) {
        connect(m_process, &QProcess::readyReadStandardError, this, &RemoteClient::readTransferProgress);
    }
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        Q_UNUSED(error)
        if (m_cancelled) {
            return;
        }
        emit failed(tr("Unable to start curl."), m_process ? m_process->errorString() : QString());
    });
    m_process->start();
}

void RemoteClient::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *process = m_process;
    const QByteArray output = process->readAllStandardOutput();
    const QByteArray errorOutput = m_errorBuffer + process->readAllStandardError();

    process->deleteLater();
    m_process = nullptr;

    if (m_cancelled) {
        m_cancelled = false;
        emit cancelled();
        emit logMessage(tr("Transfer cancelled."));
        return;
    }

    if (shouldRetryWithAnyAuth(exitCode, exitStatus, errorOutput)) {
        m_authenticationMode = AnyAuthentication;
        m_canRetryAnyAuth = false;
        emit logMessage(tr("Basic authentication failed, retrying with negotiated authentication."));
        startCurrentOperation();
        return;
    }

    if (m_operation == MakeDirectoryOperation && (exitStatus != QProcess::NormalExit || exitCode != 0)) {
        emit logMessage(tr("Remote folder create reported an error, continuing: %1").arg(QString::fromLocal8Bit(errorOutput).trimmed()));
        emit transferProgress(100);
        emit transferFinished(m_transferSource, m_transferDestination);
        return;
    }

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        emit failed(m_operation == ListOperation ? tr("Remote listing failed.") : tr("Remote operation failed."),
                    operationDebugDetails(exitCode, exitStatus, errorOutput));
        return;
    }

    if (m_operation == RemoveOperation) {
        emit removeFinished(m_removePath);
        emit logMessage(tr("Deleted %1").arg(m_removePath));
        return;
    }

    if (m_operation == MoveOperation) {
        emit moveFinished(m_moveSourcePath, m_moveDestinationPath);
        emit logMessage(tr("Moved %1 to %2").arg(m_moveSourcePath, m_moveDestinationPath));
        return;
    }

    if (m_operation != ListOperation) {
        emit transferProgress(100);
        emit transferFinished(m_transferSource, m_transferDestination);
        emit logMessage(tr("Transfer complete: %1 -> %2").arg(m_transferSource, m_transferDestination));
        return;
    }

    QVector<RemoteEntry> entries = parseDirectoryListing(m_connection.protocol, m_currentPath, output);
    emit listed(m_currentPath, entries);
    emit logMessage(tr("Loaded %1 entries from %2").arg(entries.size()).arg(m_currentUrl));
}

void RemoteClient::readTransferProgress()
{
    if (!m_process || m_operation == ListOperation) {
        return;
    }

    const QByteArray data = m_process->readAllStandardError();
    m_errorBuffer += data;
    const QString text = QString::fromLocal8Bit(data);
    const QRegularExpression percentPattern(QStringLiteral("(\\d{1,3}(?:\\.\\d+)?)%"));
    QRegularExpressionMatchIterator it = percentPattern.globalMatch(text);
    int lastPercent = -1;
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        lastPercent = qBound(0, qRound(match.captured(1).toDouble()), 100);
    }
    if (lastPercent >= 0) {
        emit transferProgress(lastPercent);
    }

    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QStringList columns = it->simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (columns.size() < 12) {
            continue;
        }
        bool ok = false;
        const int percent = columns.first().toInt(&ok);
        if (!ok) {
            continue;
        }
        emit transferProgress(qBound(0, percent, 100));
        const QString speed = curlSpeedText(columns.last());
        if (!speed.isEmpty()) {
            emit transferSpeed(speed);
        }
        break;
    }
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
    if (!path.isEmpty()) {
        url.setPath(normalizePath(path));
    }
    return url.toString(QUrl::FullyEncoded);
}

QString RemoteClient::normalizePath(const QString &path) const
{
    QString result = path.trimmed();
    if (result.isEmpty()) {
        result = QStringLiteral("/");
    }
    if (result == QLatin1String("~") || result.startsWith(QStringLiteral("~/"))) {
        return QUrl::fromPercentEncoding(result.toUtf8());
    }
    if (!result.startsWith(QLatin1Char('/'))) {
        result.prepend(QLatin1Char('/'));
    }
    return QUrl::fromPercentEncoding(result.toUtf8());
}

QString RemoteClient::joinPath(const QString &basePath, const QString &name) const
{
    if (basePath.trimmed().isEmpty()) {
        return name;
    }

    QString base = normalizePath(basePath);
    if (!base.endsWith(QLatin1Char('/'))) {
        base.append(QLatin1Char('/'));
    }
    return normalizePath(base + name);
}

QString RemoteClient::authenticationModeName() const
{
    return m_authenticationMode == BasicAuthentication ? QStringLiteral("basic") : QStringLiteral("anyauth");
}

QString RemoteClient::sanitizedArguments(const QStringList &args) const
{
    QStringList sanitized;
    bool redactNext = false;
    for (const QString &arg : args) {
        if (redactNext) {
            sanitized << QStringLiteral("<redacted>");
            redactNext = false;
            continue;
        }
        sanitized << arg;
        if (arg == QLatin1String("--user")) {
            redactNext = true;
        }
    }
    return sanitized.join(QLatin1Char(' '));
}

QString RemoteClient::operationDebugDetails(int exitCode, QProcess::ExitStatus exitStatus, const QByteArray &errorOutput) const
{
    QStringList details;
    details << tr("URL: %1").arg(m_currentUrl)
            << tr("Protocol: %1").arg(m_connection.protocol)
            << tr("Authentication: %1").arg(authenticationModeName())
            << tr("Exit code: %1").arg(exitCode)
            << tr("Exit status: %1").arg(exitStatus == QProcess::NormalExit ? QStringLiteral("normal") : QStringLiteral("crashed"))
            << tr("Curl arguments: curl %1").arg(sanitizedArguments(m_lastCurlArguments));

    const QString stderrText = QString::fromLocal8Bit(errorOutput).trimmed();
    if (!stderrText.isEmpty()) {
        details << tr("Curl stderr:") << stderrText;
    }
    return details.join(QLatin1Char('\n'));
}

QString RemoteClient::curlSpeedText(const QString &speed) const
{
    const QString value = speed.trimmed();
    if (value.isEmpty() || value == QLatin1String("0") || value == QLatin1String("-")) {
        return QString();
    }
    if (value.at(value.size() - 1).isDigit()) {
        return tr("%1 B/s").arg(value);
    }
    return tr("%1/s").arg(value);
}

bool RemoteClient::isWebDavConnection(const RemoteConnection &connection) const
{
    const QString protocol = connection.protocol.toLower();
    return protocol == QLatin1String("webdav") || protocol == QLatin1String("webdavs");
}

void RemoteClient::addDiagnosticsArguments(QStringList *args) const
{
    args->append(QStringLiteral("--write-out"));
    args->append(QStringLiteral("%{stderr}\nCurl diagnostics: http_code=%{http_code} url_effective=%{url_effective} redirect_url=%{redirect_url} content_type=%{content_type} size_download=%{size_download} size_upload=%{size_upload} time_total=%{time_total}\n"));
}

void RemoteClient::addLocationArguments(QStringList *args, const RemoteConnection &connection) const
{
    *args << (isWebDavConnection(connection) ? QStringLiteral("--location-trusted") : QStringLiteral("--location"));
}

void RemoteClient::addAuthenticationArguments(QStringList *args, const RemoteConnection &connection, AuthenticationMode mode) const
{
    if (connection.username.isEmpty()) {
        return;
    }

    if (isWebDavConnection(connection)) {
        *args << (mode == BasicAuthentication ? QStringLiteral("--basic") : QStringLiteral("--anyauth"));
    }
    *args << QStringLiteral("--user") << QStringLiteral("%1:%2").arg(connection.username, connection.password);
}

bool RemoteClient::shouldRetryWithAnyAuth(int exitCode, QProcess::ExitStatus exitStatus, const QByteArray &errorOutput) const
{
    if (!m_canRetryAnyAuth || m_authenticationMode != BasicAuthentication || !isWebDavConnection(m_connection)) {
        return false;
    }
    if (exitStatus == QProcess::NormalExit && (exitCode == 22 || exitCode == 67)) {
        return true;
    }

    const QString errorText = QString::fromLocal8Bit(errorOutput).toLower();
    return errorText.contains(QStringLiteral("401"))
        || errorText.contains(QStringLiteral("authentication"))
        || errorText.contains(QStringLiteral("authenticate"))
        || errorText.contains(QStringLiteral("unauthorized"));
}

QStringList RemoteClient::curlArguments(const RemoteConnection &connection, const QString &url, AuthenticationMode mode) const
{
    QStringList args;
    args << QStringLiteral("--silent")
         << QStringLiteral("--show-error")
         << QStringLiteral("--fail")
         << QStringLiteral("--globoff")
         << QStringLiteral("--connect-timeout") << QStringLiteral("15")
         << QStringLiteral("--max-time") << QStringLiteral("60");
    addLocationArguments(&args, connection);

    addAuthenticationArguments(&args, connection, mode);

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
        if (entry.directory && !entry.path.endsWith(QLatin1Char('/'))) {
            entry.path.append(QLatin1Char('/'));
        }
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
                    if (current.directory && !current.path.endsWith(QLatin1Char('/'))) {
                        current.path.append(QLatin1Char('/'));
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
