#include "mainwindow.h"

#include <DTitlebar>
#include <DPushButton>
#include <DSuggestButton>

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QIcon>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidgetItem>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
    , m_client(new RemoteClient(this))
{
    titlebar()->setTitle(tr("Remote File DTK2"));
    titlebar()->setSeparatorVisible(true);

    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(createConnectionBar());
    layout->addWidget(createBrowser(), 1);
    setCentralWidget(central);

    connect(m_client, &RemoteClient::started, this, [this](const QString &url) {
        setBusy(true);
        m_remoteStatusLabel->setText(tr("Working on %1").arg(url));
    });
    connect(m_client, &RemoteClient::listed, this, &MainWindow::showEntries);
    connect(m_client, &RemoteClient::transferFinished, this, &MainWindow::showTransferFinished);
    connect(m_client, &RemoteClient::failed, this, &MainWindow::showError);
    connect(m_client, &RemoteClient::logMessage, this, &MainWindow::appendLog);

    loadSavedSites();
    updateDefaultPort();
}

QWidget *MainWindow::createConnectionBar()
{
    QWidget *bar = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);

    m_siteCombo = new QComboBox(bar);
    m_siteCombo->setMinimumWidth(160);
    m_protocolCombo = new QComboBox(bar);
    m_protocolCombo->addItems({QStringLiteral("ftp"), QStringLiteral("sftp"), QStringLiteral("webdav"), QStringLiteral("webdavs")});
    m_hostEdit = new QLineEdit(bar);
    m_hostEdit->setPlaceholderText(tr("Host"));
    m_portSpin = new QSpinBox(bar);
    m_portSpin->setRange(0, 65535);
    m_portSpin->setSpecialValueText(tr("Auto"));
    m_userEdit = new QLineEdit(bar);
    m_userEdit->setPlaceholderText(tr("User"));
    m_passwordEdit = new QLineEdit(bar);
    m_passwordEdit->setPlaceholderText(tr("Password"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_remotePathEdit = new QLineEdit(QStringLiteral("/"), bar);
    m_remotePathEdit->setPlaceholderText(tr("Remote path"));
    m_saveSiteButton = new DPushButton(tr("Save"), bar);
    m_connectButton = new DSuggestButton(tr("Connect"), bar);

    layout->addWidget(m_siteCombo);
    layout->addWidget(m_protocolCombo);
    layout->addWidget(m_hostEdit, 2);
    layout->addWidget(m_portSpin);
    layout->addWidget(m_userEdit);
    layout->addWidget(m_passwordEdit);
    layout->addWidget(m_remotePathEdit, 2);
    layout->addWidget(m_saveSiteButton);
    layout->addWidget(m_connectButton);

    connect(m_siteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::loadSelectedSite);
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateDefaultPort);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::connectToRemote);
    connect(m_saveSiteButton, &QPushButton::clicked, this, &MainWindow::saveCurrentSite);
    connect(m_remotePathEdit, &QLineEdit::returnPressed, this, &MainWindow::refreshRemote);

    return bar;
}

QWidget *MainWindow::createBrowser()
{
    QSplitter *vertical = new QSplitter(Qt::Vertical, this);
    QSplitter *files = new QSplitter(Qt::Horizontal, vertical);

    files->addWidget(createLocalPane());
    files->addWidget(createRemotePane());
    files->setStretchFactor(0, 1);
    files->setStretchFactor(1, 1);

    QSplitter *bottom = new QSplitter(Qt::Horizontal, vertical);
    bottom->addWidget(createTransferPane());

    m_log = new QPlainTextEdit(bottom);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(500);
    m_log->setPlaceholderText(tr("Connection log"));
    bottom->addWidget(m_log);
    bottom->setStretchFactor(0, 2);
    bottom->setStretchFactor(1, 1);

    vertical->addWidget(files);
    vertical->addWidget(bottom);
    vertical->setStretchFactor(0, 4);
    vertical->setStretchFactor(1, 1);
    return vertical;
}

QWidget *MainWindow::createLocalPane()
{
    QWidget *pane = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(pane);
    layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *tools = new QHBoxLayout;
    m_localStatusLabel = new QLabel(tr("Local"), pane);
    m_localPathEdit = new QLineEdit(QDir::homePath(), pane);
    m_localUpButton = new QPushButton(tr("Up"), pane);
    m_uploadButton = new DPushButton(tr("Upload >"), pane);
    tools->addWidget(m_localStatusLabel);
    tools->addWidget(m_localPathEdit, 1);
    tools->addWidget(m_localUpButton);
    tools->addWidget(m_uploadButton);

    m_localModel = new QFileSystemModel(this);
    m_localModel->setRootPath(QDir::homePath());
    m_localView = new QTreeView(pane);
    m_localView->setModel(m_localModel);
    m_localView->setRootIndex(m_localModel->index(QDir::homePath()));
    m_localView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_localView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_localView->setAlternatingRowColors(true);
    m_localView->header()->setStretchLastSection(true);

    layout->addLayout(tools);
    layout->addWidget(m_localView, 1);

    connect(m_localView, &QTreeView::doubleClicked, this, &MainWindow::openLocalEntry);
    connect(m_localPathEdit, &QLineEdit::returnPressed, this, &MainWindow::setLocalPathFromEdit);
    connect(m_localUpButton, &QPushButton::clicked, this, &MainWindow::goLocalUp);
    connect(m_uploadButton, &QPushButton::clicked, this, &MainWindow::uploadSelected);
    return pane;
}

QWidget *MainWindow::createRemotePane()
{
    QWidget *pane = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(pane);
    layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *tools = new QHBoxLayout;
    m_remoteStatusLabel = new QLabel(tr("Remote"), pane);
    m_remoteUpButton = new QPushButton(tr("Up"), pane);
    m_remoteRefreshButton = new QPushButton(tr("Refresh"), pane);
    m_downloadButton = new DPushButton(tr("< Download"), pane);
    tools->addWidget(m_remoteStatusLabel, 1);
    tools->addWidget(m_remoteUpButton);
    tools->addWidget(m_remoteRefreshButton);
    tools->addWidget(m_downloadButton);

    m_remoteTable = new QTableWidget(0, 4, pane);
    m_remoteTable->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Size"), tr("Modified")});
    m_remoteTable->horizontalHeader()->setStretchLastSection(true);
    m_remoteTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_remoteTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_remoteTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_remoteTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_remoteTable->setAlternatingRowColors(true);

    layout->addLayout(tools);
    layout->addWidget(m_remoteTable, 1);

    connect(m_remoteTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::openRemoteEntry);
    connect(m_remoteRefreshButton, &QPushButton::clicked, this, &MainWindow::refreshRemote);
    connect(m_remoteUpButton, &QPushButton::clicked, this, &MainWindow::goRemoteUp);
    connect(m_downloadButton, &QPushButton::clicked, this, &MainWindow::downloadSelected);
    return pane;
}

QWidget *MainWindow::createTransferPane()
{
    m_transferTable = new QTableWidget(0, 4, this);
    m_transferTable->setHorizontalHeaderLabels({tr("Direction"), tr("Source"), tr("Destination"), tr("Status")});
    m_transferTable->horizontalHeader()->setStretchLastSection(true);
    m_transferTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_transferTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_transferTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_transferTable->setAlternatingRowColors(true);
    return m_transferTable;
}

RemoteConnection MainWindow::currentConnection() const
{
    RemoteConnection connection;
    connection.protocol = m_protocolCombo->currentText();
    connection.host = m_hostEdit->text();
    connection.port = m_portSpin->value();
    connection.username = m_userEdit->text();
    connection.password = m_passwordEdit->text();
    connection.path = m_remotePathEdit->text();
    return connection;
}

void MainWindow::connectToRemote()
{
    if (m_hostEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Missing host"), tr("Please enter a remote host."));
        return;
    }

    m_client->list(currentConnection(), m_remotePathEdit->text());
}

void MainWindow::refreshRemote()
{
    connectToRemote();
}

void MainWindow::goRemoteUp()
{
    m_remotePathEdit->setText(parentPath(m_remotePathEdit->text()));
    refreshRemote();
}

void MainWindow::goLocalUp()
{
    const QString parent = QFileInfo(m_localPathEdit->text()).dir().absolutePath();
    m_localPathEdit->setText(parent);
    setLocalPathFromEdit();
}

void MainWindow::openRemoteEntry(int row, int column)
{
    Q_UNUSED(column)
    QTableWidgetItem *nameItem = m_remoteTable->item(row, 0);
    if (!nameItem) {
        return;
    }

    const bool isDirectory = nameItem->data(Qt::UserRole + 1).toBool();
    const QString path = nameItem->data(Qt::UserRole).toString();
    if (!isDirectory) {
        downloadSelected();
        return;
    }

    m_remotePathEdit->setText(path);
    refreshRemote();
}

void MainWindow::openLocalEntry(const QModelIndex &index)
{
    const QString path = m_localModel->filePath(index);
    if (!QFileInfo(path).isDir()) {
        return;
    }
    m_localPathEdit->setText(path);
    m_localView->setRootIndex(m_localModel->index(path));
}

void MainWindow::uploadSelected()
{
    const QString localPath = selectedLocalPath();
    if (localPath.isEmpty()) {
        QMessageBox::information(this, tr("Select file"), tr("Please select a local file to upload."));
        return;
    }
    QFileInfo info(localPath);
    if (info.isDir()) {
        QMessageBox::information(this, tr("Folder upload"), tr("Folder upload is not implemented yet."));
        return;
    }

    const QString remotePath = joinRemotePath(m_remotePathEdit->text(), info.fileName());
    addTransferRow(tr("Upload"), localPath, remotePath);
    m_lastTransferWasUpload = true;
    m_client->upload(currentConnection(), localPath, remotePath);
}

void MainWindow::downloadSelected()
{
    const RemoteEntry entry = selectedRemoteEntry();
    if (entry.name.isEmpty()) {
        QMessageBox::information(this, tr("Select file"), tr("Please select a remote file to download."));
        return;
    }
    if (entry.directory) {
        QMessageBox::information(this, tr("Folder download"), tr("Folder download is not implemented yet."));
        return;
    }

    const QString localPath = QDir(m_localPathEdit->text()).filePath(entry.name);
    addTransferRow(tr("Download"), entry.path, localPath);
    m_lastTransferWasUpload = false;
    m_client->download(currentConnection(), entry.path, localPath);
}

void MainWindow::saveCurrentSite()
{
    RemoteConnection connection = currentConnection();
    if (connection.host.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Missing host"), tr("Please enter a host before saving."));
        return;
    }

    const QString name = siteDisplayName(connection);
    int existing = -1;
    for (int i = 0; i < m_savedSites.size(); ++i) {
        if (siteDisplayName(m_savedSites.at(i)) == name) {
            existing = i;
            break;
        }
    }

    if (existing >= 0) {
        m_savedSites[existing] = connection;
    } else {
        m_savedSites.append(connection);
    }
    persistSavedSites();
    loadSavedSites();
    appendLog(tr("Saved site %1. Passwords are stored in the user configuration for convenience.").arg(name));
}

void MainWindow::loadSelectedSite(int index)
{
    const int savedIndex = m_siteCombo->itemData(index).toInt();
    if (savedIndex < 0 || savedIndex >= m_savedSites.size()) {
        return;
    }

    const RemoteConnection connection = m_savedSites.at(savedIndex);
    m_protocolCombo->setCurrentText(connection.protocol);
    m_hostEdit->setText(connection.host);
    m_portSpin->setValue(connection.port);
    m_userEdit->setText(connection.username);
    m_passwordEdit->setText(connection.password);
    m_remotePathEdit->setText(connection.path.isEmpty() ? QStringLiteral("/") : connection.path);
}

void MainWindow::showEntries(const QString &path, const QVector<RemoteEntry> &entries)
{
    setBusy(false);
    m_remotePathEdit->setText(path);
    m_remoteTable->setRowCount(entries.size());

    for (int row = 0; row < entries.size(); ++row) {
        const RemoteEntry &entry = entries.at(row);
        QTableWidgetItem *name = new QTableWidgetItem(entry.directory ? QIcon::fromTheme(QStringLiteral("folder")) : QIcon::fromTheme(QStringLiteral("text-x-generic")), entry.name);
        name->setData(Qt::UserRole, entry.path);
        name->setData(Qt::UserRole + 1, entry.directory);

        m_remoteTable->setItem(row, 0, name);
        m_remoteTable->setItem(row, 1, new QTableWidgetItem(entry.directory ? tr("Folder") : tr("File")));
        m_remoteTable->setItem(row, 2, new QTableWidgetItem(entry.size >= 0 ? QString::number(entry.size) : QStringLiteral("-")));
        m_remoteTable->setItem(row, 3, new QTableWidgetItem(entry.modified));
    }

    m_remoteStatusLabel->setText(tr("%1 entries in %2").arg(entries.size()).arg(path));
}

void MainWindow::showTransferFinished(const QString &source, const QString &destination)
{
    Q_UNUSED(source)
    Q_UNUSED(destination)
    setBusy(false);
    updateFirstRunningTransfer(tr("Done"));
    if (m_lastTransferWasUpload) {
        refreshRemote();
    } else {
        m_localModel->setRootPath(m_localPathEdit->text());
    }
}

void MainWindow::showError(const QString &message, const QString &details)
{
    setBusy(false);
    updateFirstRunningTransfer(tr("Failed"));
    const QString fullMessage = details.isEmpty() ? message : QStringLiteral("%1\n%2").arg(message, details);
    m_remoteStatusLabel->setText(message);
    appendLog(fullMessage);
    QMessageBox::warning(this, tr("Remote error"), fullMessage);
}

void MainWindow::updateDefaultPort()
{
    const QString protocol = m_protocolCombo->currentText();
    if (protocol == QLatin1String("ftp")) {
        m_portSpin->setValue(21);
    } else if (protocol == QLatin1String("sftp")) {
        m_portSpin->setValue(22);
    } else if (protocol == QLatin1String("webdavs")) {
        m_portSpin->setValue(443);
    } else {
        m_portSpin->setValue(80);
    }
}

void MainWindow::setLocalPathFromEdit()
{
    const QString path = m_localPathEdit->text();
    if (!QFileInfo(path).isDir()) {
        QMessageBox::warning(this, tr("Invalid local path"), tr("The local path does not exist or is not a folder."));
        return;
    }
    m_localView->setRootIndex(m_localModel->setRootPath(path));
}

QString MainWindow::parentPath(const QString &path) const
{
    QString normalized = path.trimmed();
    if (normalized.isEmpty() || normalized == QLatin1String("/")) {
        return QStringLiteral("/");
    }
    if (normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }
    const int slash = normalized.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0) {
        return QStringLiteral("/");
    }
    return normalized.left(slash);
}

QString MainWindow::joinRemotePath(const QString &basePath, const QString &name) const
{
    QString base = basePath.trimmed();
    if (base.isEmpty()) {
        base = QStringLiteral("/");
    }
    if (!base.startsWith(QLatin1Char('/'))) {
        base.prepend(QLatin1Char('/'));
    }
    if (!base.endsWith(QLatin1Char('/'))) {
        base.append(QLatin1Char('/'));
    }
    return base + name;
}

void MainWindow::setBusy(bool busy)
{
    m_connectButton->setEnabled(!busy);
    m_saveSiteButton->setEnabled(!busy);
    m_remoteRefreshButton->setEnabled(!busy);
    m_remoteUpButton->setEnabled(!busy);
    m_uploadButton->setEnabled(!busy);
    m_downloadButton->setEnabled(!busy);
}

void MainWindow::appendLog(const QString &message)
{
    m_log->appendPlainText(QStringLiteral("[%1] %2")
                           .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message));
}

void MainWindow::addTransferRow(const QString &direction, const QString &source, const QString &destination)
{
    const int row = m_transferTable->rowCount();
    m_transferTable->insertRow(row);
    m_transferTable->setItem(row, 0, new QTableWidgetItem(direction));
    m_transferTable->setItem(row, 1, new QTableWidgetItem(source));
    m_transferTable->setItem(row, 2, new QTableWidgetItem(destination));
    m_transferTable->setItem(row, 3, new QTableWidgetItem(tr("Running")));
}

void MainWindow::updateFirstRunningTransfer(const QString &status)
{
    for (int row = 0; row < m_transferTable->rowCount(); ++row) {
        QTableWidgetItem *item = m_transferTable->item(row, 3);
        if (item && item->text() == tr("Running")) {
            item->setText(status);
            return;
        }
    }
}

void MainWindow::loadSavedSites()
{
    m_savedSites.clear();
    QSettings settings;
    const int count = settings.beginReadArray(QStringLiteral("sites"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        RemoteConnection connection;
        connection.protocol = settings.value(QStringLiteral("protocol"), QStringLiteral("ftp")).toString();
        connection.host = settings.value(QStringLiteral("host")).toString();
        connection.port = settings.value(QStringLiteral("port"), 0).toInt();
        connection.username = settings.value(QStringLiteral("username")).toString();
        connection.password = QString::fromUtf8(QByteArray::fromBase64(settings.value(QStringLiteral("password")).toByteArray()));
        connection.path = settings.value(QStringLiteral("path"), QStringLiteral("/")).toString();
        if (!connection.host.isEmpty()) {
            m_savedSites.append(connection);
        }
    }
    settings.endArray();

    m_siteCombo->blockSignals(true);
    m_siteCombo->clear();
    m_siteCombo->addItem(tr("Quick Connect"), -1);
    for (int i = 0; i < m_savedSites.size(); ++i) {
        m_siteCombo->addItem(siteDisplayName(m_savedSites.at(i)), i);
    }
    m_siteCombo->blockSignals(false);
}

void MainWindow::persistSavedSites()
{
    QSettings settings;
    settings.beginWriteArray(QStringLiteral("sites"));
    for (int i = 0; i < m_savedSites.size(); ++i) {
        const RemoteConnection &connection = m_savedSites.at(i);
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("protocol"), connection.protocol);
        settings.setValue(QStringLiteral("host"), connection.host);
        settings.setValue(QStringLiteral("port"), connection.port);
        settings.setValue(QStringLiteral("username"), connection.username);
        settings.setValue(QStringLiteral("password"), connection.password.toUtf8().toBase64());
        settings.setValue(QStringLiteral("path"), connection.path);
    }
    settings.endArray();
}

QString MainWindow::siteDisplayName(const RemoteConnection &connection) const
{
    return QStringLiteral("%1://%2:%3").arg(connection.protocol, connection.host).arg(connection.port);
}

QString MainWindow::selectedLocalPath() const
{
    const QModelIndex index = m_localView->currentIndex();
    if (!index.isValid()) {
        return QString();
    }
    return m_localModel->filePath(index);
}

RemoteEntry MainWindow::selectedRemoteEntry() const
{
    RemoteEntry entry;
    const int row = m_remoteTable->currentRow();
    QTableWidgetItem *item = row >= 0 ? m_remoteTable->item(row, 0) : nullptr;
    if (!item) {
        return entry;
    }

    entry.name = item->text();
    entry.path = item->data(Qt::UserRole).toString();
    entry.directory = item->data(Qt::UserRole + 1).toBool();
    return entry;
}
