#include "mainwindow.h"

#include <DTitlebar>

#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QHeaderView>
#include <QIcon>
#include <QMessageBox>
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
        m_statusLabel->setText(tr("Loading %1").arg(url));
    });
    connect(m_client, &RemoteClient::listed, this, &MainWindow::showEntries);
    connect(m_client, &RemoteClient::failed, this, &MainWindow::showError);
    connect(m_client, &RemoteClient::logMessage, this, &MainWindow::appendLog);

    updateDefaultPort();
}

QWidget *MainWindow::createConnectionBar()
{
    QWidget *bar = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);

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
    m_pathEdit = new QLineEdit(QStringLiteral("/"), bar);
    m_pathEdit->setPlaceholderText(tr("Remote path"));
    m_connectButton = new QPushButton(tr("Connect"), bar);
    m_refreshButton = new QPushButton(tr("Refresh"), bar);
    m_upButton = new QPushButton(tr("Up"), bar);

    layout->addWidget(m_protocolCombo);
    layout->addWidget(m_hostEdit, 2);
    layout->addWidget(m_portSpin);
    layout->addWidget(m_userEdit);
    layout->addWidget(m_passwordEdit);
    layout->addWidget(m_pathEdit, 2);
    layout->addWidget(m_upButton);
    layout->addWidget(m_refreshButton);
    layout->addWidget(m_connectButton);

    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateDefaultPort);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::connectToRemote);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refresh);
    connect(m_upButton, &QPushButton::clicked, this, &MainWindow::goUp);
    connect(m_pathEdit, &QLineEdit::returnPressed, this, &MainWindow::refresh);

    return bar;
}

QWidget *MainWindow::createBrowser()
{
    QSplitter *splitter = new QSplitter(Qt::Vertical, this);

    QWidget *browser = new QWidget(splitter);
    QVBoxLayout *browserLayout = new QVBoxLayout(browser);
    browserLayout->setContentsMargins(0, 0, 0, 0);
    m_statusLabel = new QLabel(tr("Enter a host and connect."), browser);
    m_table = new QTableWidget(0, 4, browser);
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Size"), tr("Modified")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    browserLayout->addWidget(m_statusLabel);
    browserLayout->addWidget(m_table, 1);

    m_log = new QPlainTextEdit(splitter);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(500);
    m_log->setPlaceholderText(tr("Connection log"));

    splitter->addWidget(browser);
    splitter->addWidget(m_log);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, &MainWindow::openEntry);

    return splitter;
}

RemoteConnection MainWindow::currentConnection() const
{
    RemoteConnection connection;
    connection.protocol = m_protocolCombo->currentText();
    connection.host = m_hostEdit->text();
    connection.port = m_portSpin->value();
    connection.username = m_userEdit->text();
    connection.password = m_passwordEdit->text();
    connection.path = m_pathEdit->text();
    return connection;
}

void MainWindow::connectToRemote()
{
    if (m_hostEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Missing host"), tr("Please enter a remote host."));
        return;
    }

    m_client->list(currentConnection(), m_pathEdit->text());
}

void MainWindow::refresh()
{
    connectToRemote();
}

void MainWindow::goUp()
{
    m_pathEdit->setText(parentPath(m_pathEdit->text()));
    refresh();
}

void MainWindow::openEntry(int row, int column)
{
    Q_UNUSED(column)
    QTableWidgetItem *nameItem = m_table->item(row, 0);
    if (!nameItem) {
        return;
    }

    const bool isDirectory = nameItem->data(Qt::UserRole + 1).toBool();
    const QString path = nameItem->data(Qt::UserRole).toString();
    if (!isDirectory) {
        appendLog(tr("Download/open is not implemented yet: %1").arg(path));
        return;
    }

    m_pathEdit->setText(path);
    refresh();
}

void MainWindow::showEntries(const QString &path, const QVector<RemoteEntry> &entries)
{
    setBusy(false);
    m_pathEdit->setText(path);
    m_table->setRowCount(entries.size());

    for (int row = 0; row < entries.size(); ++row) {
        const RemoteEntry &entry = entries.at(row);
        QTableWidgetItem *name = new QTableWidgetItem(entry.directory ? QIcon::fromTheme(QStringLiteral("folder")) : QIcon::fromTheme(QStringLiteral("text-x-generic")), entry.name);
        name->setData(Qt::UserRole, entry.path);
        name->setData(Qt::UserRole + 1, entry.directory);

        QTableWidgetItem *type = new QTableWidgetItem(entry.directory ? tr("Folder") : tr("File"));
        QTableWidgetItem *size = new QTableWidgetItem(entry.size >= 0 ? QString::number(entry.size) : QStringLiteral("-"));
        QTableWidgetItem *modified = new QTableWidgetItem(entry.modified);

        m_table->setItem(row, 0, name);
        m_table->setItem(row, 1, type);
        m_table->setItem(row, 2, size);
        m_table->setItem(row, 3, modified);
    }

    m_statusLabel->setText(tr("%1 entries in %2").arg(entries.size()).arg(path));
}

void MainWindow::showError(const QString &message, const QString &details)
{
    setBusy(false);
    const QString fullMessage = details.isEmpty() ? message : QStringLiteral("%1\n%2").arg(message, details);
    m_statusLabel->setText(message);
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

void MainWindow::setBusy(bool busy)
{
    m_connectButton->setEnabled(!busy);
    m_refreshButton->setEnabled(!busy);
    m_upButton->setEnabled(!busy);
}

void MainWindow::appendLog(const QString &message)
{
    m_log->appendPlainText(QStringLiteral("[%1] %2")
                           .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message));
}
