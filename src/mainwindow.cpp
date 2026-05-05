#include "mainwindow.h"

#include <DTitlebar>
#include <DPushButton>
#include <DSuggestButton>
#include <DDialog>

#include <QComboBox>
#include <QCheckBox>
#include <QApplication>
#include <QDesktopServices>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDrag>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHash>
#include <QIcon>
#include <QKeyEvent>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QMimeType>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QProgressBar>
#include <QSettings>
#include <QSizePolicy>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

DWIDGET_USE_NAMESPACE

namespace {
const char kRemotePathsMime[] = "application/x-remote-file-dtk2-paths";
const char kXdndDirectSaveMime[] = "XdndDirectSave0";

void polishFileTable(QTableWidget *table)
{
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->setIconSize(QSize(24, 24));
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(36);
    table->horizontalHeader()->setHighlightSections(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setStyleSheet(QStringLiteral(
        "QTableWidget { border: 1px solid palette(mid); border-radius: 8px; background: palette(base); alternate-background-color: rgba(0, 0, 0, 5%); }"
        "QTableWidget::item { border: 0; padding: 5px 8px; }"
        "QTableWidget::item:selected { border-radius: 6px; background: palette(highlight); color: palette(highlighted-text); }"
        "QHeaderView::section { border: 0; border-bottom: 1px solid palette(mid); padding: 6px 8px; background: palette(window); font-weight: 600; }"));
}

QPixmap dragPreviewPixmap(const QStringList &names, const QIcon &icon)
{
    const int shown = qMin(names.size(), 3);
    const int width = 260;
    const int rowHeight = 30;
    const int height = 14 + shown * rowHeight + (names.size() > shown ? 22 : 0);
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(0, 0, 0, 35));
    painter.setBrush(QColor(245, 245, 245, 235));
    painter.drawRoundedRect(pixmap.rect().adjusted(1, 1, -1, -1), 9, 9);

    const QPixmap iconPixmap = icon.pixmap(22, 22);
    for (int i = 0; i < shown; ++i) {
        const int y = 8 + i * rowHeight;
        painter.drawPixmap(12, y + 3, iconPixmap);
        painter.setPen(QColor(35, 35, 35));
        painter.drawText(QRect(42, y, width - 54, rowHeight), Qt::AlignVCenter | Qt::TextSingleLine,
                         painter.fontMetrics().elidedText(names.at(i), Qt::ElideMiddle, width - 54));
    }
    if (names.size() > shown) {
        painter.setPen(QColor(80, 80, 80));
        painter.drawText(QRect(42, 8 + shown * rowHeight, width - 54, 20), Qt::AlignVCenter,
                         QObject::tr("+%1 more").arg(names.size() - shown));
    }

    return pixmap;
}

bool copyDirectoryRecursively(const QString &sourcePath, const QString &destinationPath)
{
    QDir sourceDir(sourcePath);
    if (!sourceDir.exists() || !QDir().mkpath(destinationPath)) {
        return false;
    }

    QDirIterator it(sourcePath, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString source = it.next();
        const QString relative = sourceDir.relativeFilePath(source);
        const QString destination = QDir(destinationPath).filePath(relative);
        const QFileInfo info(source);
        if (info.isDir()) {
            if (!QDir().mkpath(destination)) {
                return false;
            }
        } else if (!QFile::copy(source, destination)) {
            return false;
        }
    }

    return true;
}

QString protocolDisplayName(const QString &protocol)
{
    const QString normalized = protocol.toLower();
    if (normalized == QLatin1String("webdav")) {
        return QStringLiteral("WebDAV");
    }
    if (normalized == QLatin1String("webdavs")) {
        return QStringLiteral("WebDAVS");
    }
    return normalized.toUpper();
}

QIcon themedIcon(const QString &iconName, const QString &fallbackName = QStringLiteral("text-x-generic"))
{
    return QIcon::fromTheme(iconName, QIcon::fromTheme(fallbackName));
}

QIcon fileIcon(const QString &fileName, bool directory)
{
    if (directory) {
        return themedIcon(QStringLiteral("folder"), QStringLiteral("inode-directory"));
    }

    static const QHash<QString, QString> extensionIcons = {
        {QStringLiteral("jpg"), QStringLiteral("image-x-generic")}, {QStringLiteral("jpeg"), QStringLiteral("image-x-generic")},
        {QStringLiteral("png"), QStringLiteral("image-x-generic")}, {QStringLiteral("gif"), QStringLiteral("image-x-generic")},
        {QStringLiteral("bmp"), QStringLiteral("image-x-generic")}, {QStringLiteral("webp"), QStringLiteral("image-x-generic")},
        {QStringLiteral("svg"), QStringLiteral("image-x-generic")}, {QStringLiteral("ico"), QStringLiteral("image-x-generic")},
        {QStringLiteral("mp4"), QStringLiteral("video-x-generic")}, {QStringLiteral("mkv"), QStringLiteral("video-x-generic")},
        {QStringLiteral("avi"), QStringLiteral("video-x-generic")}, {QStringLiteral("mov"), QStringLiteral("video-x-generic")},
        {QStringLiteral("wmv"), QStringLiteral("video-x-generic")}, {QStringLiteral("flv"), QStringLiteral("video-x-generic")},
        {QStringLiteral("webm"), QStringLiteral("video-x-generic")}, {QStringLiteral("mp3"), QStringLiteral("audio-x-generic")},
        {QStringLiteral("flac"), QStringLiteral("audio-x-generic")}, {QStringLiteral("wav"), QStringLiteral("audio-x-generic")},
        {QStringLiteral("ogg"), QStringLiteral("audio-x-generic")}, {QStringLiteral("m4a"), QStringLiteral("audio-x-generic")},
        {QStringLiteral("zip"), QStringLiteral("package-x-generic")}, {QStringLiteral("rar"), QStringLiteral("package-x-generic")},
        {QStringLiteral("7z"), QStringLiteral("package-x-generic")}, {QStringLiteral("tar"), QStringLiteral("package-x-generic")},
        {QStringLiteral("gz"), QStringLiteral("package-x-generic")}, {QStringLiteral("bz2"), QStringLiteral("package-x-generic")},
        {QStringLiteral("xz"), QStringLiteral("package-x-generic")}, {QStringLiteral("deb"), QStringLiteral("package-x-generic")},
        {QStringLiteral("rpm"), QStringLiteral("package-x-generic")}, {QStringLiteral("pdf"), QStringLiteral("application-pdf")},
        {QStringLiteral("doc"), QStringLiteral("x-office-document")}, {QStringLiteral("docx"), QStringLiteral("x-office-document")},
        {QStringLiteral("odt"), QStringLiteral("x-office-document")}, {QStringLiteral("xls"), QStringLiteral("x-office-spreadsheet")},
        {QStringLiteral("xlsx"), QStringLiteral("x-office-spreadsheet")}, {QStringLiteral("ods"), QStringLiteral("x-office-spreadsheet")},
        {QStringLiteral("ppt"), QStringLiteral("x-office-presentation")}, {QStringLiteral("pptx"), QStringLiteral("x-office-presentation")},
        {QStringLiteral("odp"), QStringLiteral("x-office-presentation")}, {QStringLiteral("txt"), QStringLiteral("text-x-generic")},
        {QStringLiteral("log"), QStringLiteral("text-x-generic")}, {QStringLiteral("md"), QStringLiteral("text-x-generic")},
        {QStringLiteral("cpp"), QStringLiteral("text-x-source")}, {QStringLiteral("h"), QStringLiteral("text-x-source")},
        {QStringLiteral("c"), QStringLiteral("text-x-source")}, {QStringLiteral("py"), QStringLiteral("text-x-script")},
        {QStringLiteral("js"), QStringLiteral("text-x-script")}, {QStringLiteral("sh"), QStringLiteral("text-x-script")},
        {QStringLiteral("html"), QStringLiteral("text-html")}, {QStringLiteral("css"), QStringLiteral("text-css")}
    };

    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (extensionIcons.contains(suffix)) {
        return themedIcon(extensionIcons.value(suffix));
    }

    const QMimeType mime = QMimeDatabase().mimeTypeForFile(fileName, QMimeDatabase::MatchExtension);
    if (!mime.iconName().isEmpty()) {
        return themedIcon(mime.iconName());
    }
    if (!mime.genericIconName().isEmpty()) {
        return themedIcon(mime.genericIconName());
    }
    return themedIcon(QStringLiteral("text-x-generic"));
}
}

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
    , m_client(new RemoteClient(this))
    , m_transferClient(new RemoteClient(this))
{
    titlebar()->setTitle(tr("GXDE File Transfer"));
    titlebar()->setIcon(QIcon(QStringLiteral(":/icons/gxde-filetransfer.svg")));
    titlebar()->setSeparatorVisible(true);

    QMenu *settingsMenu = new QMenu(this);
    QAction *manageSitesAction = settingsMenu->addAction(tr("Settings"));
    connect(manageSitesAction, &QAction::triggered, this, &MainWindow::showSavedSitesDialog);
    titlebar()->setMenu(settingsMenu);

    QWidget *central = new QWidget(this);
    central->setStyleSheet(QStringLiteral(
        "QGroupBox { border: 1px solid palette(mid); border-radius: 8px; margin-top: 12px; padding: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
        "QLineEdit, QComboBox, QSpinBox { min-height: 26px; }"));
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(createConnectionBar());
    layout->addWidget(createBrowser(), 1);
    setCentralWidget(central);

    connect(m_client, &RemoteClient::started, this, [this](const QString &url) {
        Q_UNUSED(url)
        setBrowsingBusy(true);
        m_remoteStatusLabel->setText(tr("Loading"));
    });
    connect(m_client, &RemoteClient::listed, this, &MainWindow::showEntries);
    connect(m_client, &RemoteClient::removeFinished, this, &MainWindow::showRemoteRemoved);
    connect(m_client, &RemoteClient::moveFinished, this, &MainWindow::showRemoteMoved);
    connect(m_client, &RemoteClient::transferFinished, this, &MainWindow::showTransferFinished);
    connect(m_client, &RemoteClient::failed, this, &MainWindow::showError);
    connect(m_client, &RemoteClient::logMessage, this, &MainWindow::appendLog);

    connect(m_transferClient, &RemoteClient::started, this, [this](const QString &url) {
        setTransferBusy(true);
        appendLog(tr("Transfer started: %1").arg(url));
    });
    connect(m_transferClient, &RemoteClient::transferProgress, this, &MainWindow::showTransferProgress);
    connect(m_transferClient, &RemoteClient::transferSpeed, this, &MainWindow::showTransferSpeed);
    connect(m_transferClient, &RemoteClient::transferFinished, this, &MainWindow::showTransferFinished);
    connect(m_transferClient, &RemoteClient::cancelled, this, &MainWindow::showTransferCancelled);
    connect(m_transferClient, &RemoteClient::failed, this, &MainWindow::showTransferError);
    connect(m_transferClient, &RemoteClient::logMessage, this, &MainWindow::appendLog);

    loadSavedSites();
    updateDefaultPort();
}

QWidget *MainWindow::createConnectionBar()
{
    QGroupBox *bar = new QGroupBox(tr("Connection"), this);
    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 8, 8, 8);

    m_siteCombo = new QComboBox(bar);
    m_siteCombo->setMinimumWidth(160);
    m_protocolCombo = new QComboBox(bar);
    m_protocolCombo->addItems({QStringLiteral("FTP"), QStringLiteral("SFTP"), QStringLiteral("WebDAV"), QStringLiteral("WebDAVS")});
    m_hostEdit = new QLineEdit(bar);
    m_hostEdit->setPlaceholderText(tr("Host"));
    m_portSpin = new QSpinBox(bar);
    m_portSpin->setRange(0, 65535);
    m_portSpin->setSpecialValueText(tr("Auto"));
    m_portSpin->setMinimumWidth(120);
    m_userEdit = new QLineEdit(bar);
    m_userEdit->setPlaceholderText(tr("User"));
    m_passwordEdit = new QLineEdit(bar);
    m_passwordEdit->setPlaceholderText(tr("Password"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_saveSiteButton = new DPushButton(tr("Save"), bar);
    m_connectButton = new DSuggestButton(tr("Connect"), bar);
    m_saveSiteButton->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    m_connectButton->setIcon(QIcon::fromTheme(QStringLiteral("network-connect")));

    layout->addWidget(m_siteCombo);
    layout->addWidget(m_protocolCombo);
    layout->addWidget(m_hostEdit, 2);
    layout->addWidget(m_portSpin);
    layout->addWidget(m_userEdit);
    layout->addWidget(m_passwordEdit);
    layout->addWidget(m_saveSiteButton);
    layout->addWidget(m_connectButton);

    connect(m_siteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::loadSelectedSite);
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateDefaultPort);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::connectToRemote);
    connect(m_saveSiteButton, &QPushButton::clicked, this, &MainWindow::saveCurrentSite);

    return bar;
}

QWidget *MainWindow::createBrowser()
{
    QSplitter *vertical = new QSplitter(Qt::Vertical, this);
    QSplitter *files = new QSplitter(Qt::Horizontal, vertical);

    files->addWidget(createLocalPane());
    files->addWidget(createRemotePane());
    files->setChildrenCollapsible(false);
    files->setStretchFactor(0, 3);
    files->setStretchFactor(1, 2);
    files->setHandleWidth(8);
    files->setSizes({3, 2});

    vertical->addWidget(files);
    vertical->addWidget(createTransferPane());
    vertical->setChildrenCollapsible(false);
    vertical->setHandleWidth(8);
    vertical->setStretchFactor(0, 4);
    vertical->setStretchFactor(1, 1);
    vertical->setSizes({4, 1});
    return vertical;
}

QWidget *MainWindow::createLocalPane()
{
    QGroupBox *pane = new QGroupBox(tr("Local Files"), this);
    pane->setMinimumWidth(560);
    QVBoxLayout *layout = new QVBoxLayout(pane);
    layout->setContentsMargins(8, 8, 8, 8);

    QHBoxLayout *tools = new QHBoxLayout;
    m_localStatusLabel = new QLabel(tr("Local"), pane);
    m_localPathEdit = new QLineEdit(QDir::homePath(), pane);
    m_localUpButton = new DPushButton(tr("Up"), pane);
    m_uploadButton = new DPushButton(tr("Upload >"), pane);
    m_localUpButton->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    m_uploadButton->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    tools->addWidget(m_localStatusLabel);
    tools->addWidget(m_localPathEdit, 1);
    tools->addWidget(m_localUpButton);
    tools->addWidget(m_uploadButton);

    m_localView = new QTableWidget(0, 4, pane);
    m_localView->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Size"), tr("Modified")});
    m_localView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_localView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_localView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_localView->setAlternatingRowColors(true);
    m_localView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_localView->setDragEnabled(true);
    m_localView->setAcceptDrops(true);
    m_localView->viewport()->setAcceptDrops(true);
    m_localView->horizontalHeader()->setStretchLastSection(false);
    m_localView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_localView->setColumnWidth(0, 260);
    m_localView->setColumnWidth(1, 90);
    m_localView->setColumnWidth(2, 120);
    polishFileTable(m_localView);
    m_localView->viewport()->installEventFilter(this);
    m_localView->installEventFilter(this);

    layout->addLayout(tools);
    layout->addWidget(m_localView, 1);

    loadLocalDirectory(QDir::homePath());

    connect(m_localView, &QTableWidget::cellDoubleClicked, this, &MainWindow::openLocalEntry);
    connect(m_localPathEdit, &QLineEdit::returnPressed, this, &MainWindow::setLocalPathFromEdit);
    connect(m_localUpButton, &QPushButton::clicked, this, &MainWindow::goLocalUp);
    connect(m_uploadButton, &QPushButton::clicked, this, &MainWindow::uploadSelected);
    connect(m_localView, &QTableWidget::customContextMenuRequested, this, &MainWindow::showLocalContextMenu);
    return pane;
}

QWidget *MainWindow::createRemotePane()
{
    QGroupBox *pane = new QGroupBox(tr("Remote Files"), this);
    QVBoxLayout *layout = new QVBoxLayout(pane);
    layout->setContentsMargins(8, 8, 8, 8);

    QHBoxLayout *tools = new QHBoxLayout;
    m_remoteStatusLabel = new QLabel(tr("Remote"), pane);
    m_remoteStatusLabel->setFixedWidth(54);
    m_remoteUpButton = new DPushButton(tr("Up"), pane);
    m_remoteRefreshButton = new DPushButton(tr("Refresh"), pane);
    m_downloadButton = new DPushButton(tr("< Download"), pane);
    m_remoteUpButton->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    m_remoteRefreshButton->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    m_downloadButton->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    m_remotePathEdit = new QLineEdit(pane);
    m_remotePathEdit->setPlaceholderText(tr("Remote path"));
    m_remotePathEdit->setMinimumWidth(160);
    m_remotePathEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    tools->addWidget(m_remoteStatusLabel);
    tools->addWidget(m_remotePathEdit, 1);
    tools->addWidget(m_remoteUpButton);
    tools->addWidget(m_remoteRefreshButton);
    tools->addWidget(m_downloadButton);

    m_remoteTable = new QTableWidget(0, 4, pane);
    m_remoteTable->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Size"), tr("Modified")});
    m_remoteTable->horizontalHeader()->setStretchLastSection(false);
    m_remoteTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_remoteTable->setColumnWidth(0, 280);
    m_remoteTable->setColumnWidth(1, 90);
    m_remoteTable->setColumnWidth(2, 100);
    m_remoteTable->setColumnWidth(3, 180);
    m_remoteTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_remoteTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_remoteTable->setDragEnabled(true);
    m_remoteTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_remoteTable->setAlternatingRowColors(true);
    m_remoteTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_remoteTable->setAcceptDrops(true);
    m_remoteTable->viewport()->setAcceptDrops(true);
    polishFileTable(m_remoteTable);
    m_remoteTable->viewport()->installEventFilter(this);
    m_remoteTable->installEventFilter(this);

    layout->addLayout(tools);
    layout->addWidget(m_remoteTable, 1);

    connect(m_remoteTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::openRemoteEntry);
    connect(m_remoteTable, &QTableWidget::cellClicked, this, &MainWindow::handleRemoteCellClicked);
    connect(m_remoteRefreshButton, &QPushButton::clicked, this, &MainWindow::refreshRemote);
    connect(m_remoteUpButton, &QPushButton::clicked, this, &MainWindow::goRemoteUp);
    connect(m_downloadButton, &QPushButton::clicked, this, &MainWindow::downloadSelected);
    connect(m_remoteTable, &QTableWidget::customContextMenuRequested, this, &MainWindow::showRemoteContextMenu);
    connect(m_remotePathEdit, &QLineEdit::returnPressed, this, &MainWindow::refreshRemote);
    return pane;
}

QWidget *MainWindow::createTransferPane()
{
    QGroupBox *pane = new QGroupBox(tr("Transfers"), this);
    pane->setMinimumWidth(560);
    QVBoxLayout *layout = new QVBoxLayout(pane);
    layout->setContentsMargins(8, 8, 8, 8);

    m_transferTabs = new QTabWidget(pane);
    m_transferTable = createTransferTable(m_transferTabs);
    m_completedTransferTable = createTransferTable(m_transferTabs);
    m_errorTransferTable = createTransferTable(m_transferTabs);
    m_transferTabs->addTab(m_transferTable, tr("Processing"));
    m_transferTabs->addTab(m_completedTransferTable, tr("Completed"));
    m_transferTabs->addTab(m_errorTransferTable, tr("Errors"));
    layout->addWidget(m_transferTabs, 1);

    m_logGroup = new QGroupBox(tr("Log"), pane);
    m_logGroup->setCheckable(true);
    m_logGroup->setChecked(false);
    m_logGroup->setMaximumHeight(32);
    QVBoxLayout *logLayout = new QVBoxLayout(m_logGroup);
    logLayout->setContentsMargins(8, 8, 8, 8);
    m_log = new QPlainTextEdit(m_logGroup);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1000);
    m_log->setPlaceholderText(tr("Connection log"));
    m_log->setVisible(false);
    logLayout->addWidget(m_log);
    connect(m_logGroup, &QGroupBox::toggled, this, [this](bool checked) {
        m_log->setVisible(checked);
        m_logGroup->setMaximumHeight(checked ? QWIDGETSIZE_MAX : 32);
    });
    layout->addWidget(m_logGroup);
    return pane;
}

QTableWidget *MainWindow::createTransferTable(QWidget *parent)
{
    QTableWidget *table = new QTableWidget(0, 5, parent);
    table->setHorizontalHeaderLabels({tr("Direction"), tr("Source"), tr("Destination"), tr("Progress"), tr("Speed")});
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->setColumnWidth(0, 100);
    table->setColumnWidth(1, 260);
    table->setColumnWidth(2, 260);
    table->setColumnWidth(3, 150);
    table->setColumnWidth(4, 120);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    polishFileTable(table);
    connect(table, &QTableWidget::customContextMenuRequested, this, &MainWindow::showTransferContextMenu);
    return table;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    static QPoint remoteDragStart;
    static QPoint localDragStart;
    if (watched == m_remoteTable && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            const int row = m_remoteTable->currentRow();
            if (row >= 0) {
                openRemoteEntry(row, 0);
                return true;
            }
        } else if (keyEvent->key() == Qt::Key_Backspace) {
            goRemoteUp();
            return true;
        } else if (keyEvent->key() == Qt::Key_Delete) {
            deleteSelectedRemote();
            return true;
        } else if (keyEvent->key() == Qt::Key_F5) {
            refreshRemote();
            return true;
        }
    } else if (watched == m_localView && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            const int row = m_localView->currentRow();
            if (row >= 0) {
                openLocalEntry(row, 0);
                return true;
            }
        } else if (keyEvent->key() == Qt::Key_Backspace) {
            goLocalUp();
            return true;
        } else if (keyEvent->key() == Qt::Key_Delete) {
            deleteSelectedLocal();
            return true;
        }
    }

    if (m_remoteTable && watched == m_remoteTable->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                remoteDragStart = mouseEvent->pos();
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton)
                && (mouseEvent->pos() - remoteDragStart).manhattanLength() >= QApplication::startDragDistance()) {
                const QVector<RemoteEntry> entries = selectedRemoteEntries();
                QStringList remotePaths;
                QStringList names;
                for (const RemoteEntry &entry : entries) {
                    remotePaths << entry.path;
                    names << entry.name;
                }
                if (!remotePaths.isEmpty()) {
                    QMimeData *mime = new QMimeData;
                    mime->setData(kRemotePathsMime, remotePaths.join(QLatin1Char('\n')).toUtf8());
                    mime->setData(kXdndDirectSaveMime, entries.first().name.toUtf8());
                    mime->setProperty("IsDirectSaveMode", true);
                    QDrag *drag = new QDrag(m_remoteTable);
                    drag->setMimeData(mime);
                    drag->setPixmap(dragPreviewPixmap(names, fileIcon(entries.first().name, entries.first().directory)));
                    drag->setHotSpot(QPoint(18, 18));
                    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
                    const QUrl directSaveUrl = mime->property("DirectSaveUrl").toUrl();
                    if (directSaveUrl.isLocalFile()) {
                        queueDownloads(entries, directSaveUrl.toLocalFile());
                    }
                    return true;
                }
            }
        }
        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent *dragEvent = static_cast<QDragEnterEvent *>(event);
            if (dragEvent->mimeData()->hasUrls() || dragEvent->mimeData()->hasFormat(kRemotePathsMime)) {
                dragEvent->setDropAction(dragEvent->source() == m_remoteTable ? Qt::MoveAction : Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        } else if (event->type() == QEvent::DragMove) {
            QDragMoveEvent *dragEvent = static_cast<QDragMoveEvent *>(event);
            if (dragEvent->mimeData()->hasUrls() || dragEvent->mimeData()->hasFormat(kRemotePathsMime)) {
                autoScrollTable(m_remoteTable, dragEvent->pos());
                const QModelIndex index = m_remoteTable->indexAt(dragEvent->pos());
                int hoverRow = -1;
                if (index.isValid()) {
                    QTableWidgetItem *item = m_remoteTable->item(index.row(), 0);
                    if (item && item->data(Qt::UserRole + 1).toBool()) {
                        hoverRow = index.row();
                    }
                }
                updateDropHover(m_remoteTable, hoverRow);
                dragEvent->setDropAction(dragEvent->source() == m_remoteTable ? Qt::MoveAction : Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        } else if (event->type() == QEvent::DragLeave) {
            clearDropHover(m_remoteTable);
            return true;
        } else if (event->type() == QEvent::Drop) {
            clearDropHover(m_remoteTable);
            QDropEvent *dropEvent = static_cast<QDropEvent *>(event);
            if (dropEvent->mimeData()->hasFormat(kRemotePathsMime) && dropEvent->source() == m_remoteTable) {
                const QString text = QString::fromUtf8(dropEvent->mimeData()->data(kRemotePathsMime));
                const QStringList paths = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                const QVector<RemoteEntry> entries = remoteEntriesForPaths(paths);
                if (!entries.isEmpty()) {
                    queueRemoteMoves(entries, remoteDropBasePath(dropEvent->pos()));
                    dropEvent->setDropAction(Qt::MoveAction);
                    dropEvent->accept();
                    return true;
                }
            }

            const QList<QUrl> urls = dropEvent->mimeData()->urls();
            if (!urls.isEmpty()) {
                const QString remoteBasePath = remoteDropBasePath(dropEvent->pos());
                bool uploaded = false;
                m_uploadConflictChoice.clear();
                for (const QUrl &url : urls) {
                    const QString path = url.toLocalFile();
                    if (!path.isEmpty()) {
                        uploaded = uploadPath(path, remoteBasePath) || uploaded;
                    }
                }
                if (uploaded) {
                    dropEvent->setDropAction(Qt::CopyAction);
                    dropEvent->accept();
                    return true;
                }
            }
        }
    } else if (m_localView && watched == m_localView->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                localDragStart = mouseEvent->pos();
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton)
                && (mouseEvent->pos() - localDragStart).manhattanLength() >= QApplication::startDragDistance()) {
                const QStringList paths = selectedLocalPaths();
                if (!paths.isEmpty()) {
                    QList<QUrl> urls;
                    QStringList names;
                    for (const QString &path : paths) {
                        urls << QUrl::fromLocalFile(path);
                        names << QFileInfo(path).fileName();
                    }
                    QMimeData *mime = new QMimeData;
                    mime->setUrls(urls);
                    QDrag *drag = new QDrag(m_localView);
                    drag->setMimeData(mime);
                    drag->setPixmap(dragPreviewPixmap(names, fileIcon(QFileInfo(paths.first()).fileName(), QFileInfo(paths.first()).isDir())));
                    drag->setHotSpot(QPoint(18, 18));
                    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::MoveAction);
                    return true;
                }
            }
        } else if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent *dragEvent = static_cast<QDragEnterEvent *>(event);
            if (dragEvent->mimeData()->hasFormat(kRemotePathsMime) || dragEvent->mimeData()->hasUrls()) {
                dragEvent->setDropAction(dragEvent->source() == m_localView ? Qt::MoveAction : Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        } else if (event->type() == QEvent::DragMove) {
            QDragMoveEvent *dragEvent = static_cast<QDragMoveEvent *>(event);
            if (dragEvent->mimeData()->hasFormat(kRemotePathsMime) || dragEvent->mimeData()->hasUrls()) {
                autoScrollTable(m_localView, dragEvent->pos());
                const QModelIndex index = m_localView->indexAt(dragEvent->pos());
                int hoverRow = -1;
                if (index.isValid()) {
                    QTableWidgetItem *item = m_localView->item(index.row(), 0);
                    if (item && item->data(Qt::UserRole + 1).toBool()) {
                        hoverRow = index.row();
                    }
                }
                updateDropHover(m_localView, hoverRow);
                dragEvent->setDropAction(dragEvent->source() == m_localView ? Qt::MoveAction : Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        } else if (event->type() == QEvent::DragLeave) {
            clearDropHover(m_localView);
            return true;
        } else if (event->type() == QEvent::Drop) {
            clearDropHover(m_localView);
            QDropEvent *dropEvent = static_cast<QDropEvent *>(event);
            if (dropEvent->mimeData()->hasFormat(kRemotePathsMime)) {
                const QString text = QString::fromUtf8(dropEvent->mimeData()->data(kRemotePathsMime));
                const QStringList paths = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                const QVector<RemoteEntry> entries = remoteEntriesForPaths(paths);
                if (!entries.isEmpty()) {
                    queueDownloads(entries, localDropDirectory(dropEvent->pos()));
                    dropEvent->setDropAction(Qt::CopyAction);
                    dropEvent->accept();
                    return true;
                }
            } else if (dropEvent->mimeData()->hasUrls()) {
                QStringList paths;
                for (const QUrl &url : dropEvent->mimeData()->urls()) {
                    const QString path = url.toLocalFile();
                    if (!path.isEmpty()) {
                        paths << path;
                    }
                }
                if (!paths.isEmpty()) {
                    const bool move = dropEvent->source() == m_localView;
                    copyOrMoveLocalPaths(paths, localDropDirectory(dropEvent->pos()), move);
                    dropEvent->setDropAction(move ? Qt::MoveAction : Qt::CopyAction);
                    dropEvent->accept();
                    return true;
                }
            }
        }
    }

    return DMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F5) {
        refreshRemote();
        event->accept();
        return;
    }
    DMainWindow::keyPressEvent(event);
}

RemoteConnection MainWindow::currentConnection() const
{
    RemoteConnection connection;
    connection.protocol = m_protocolCombo->currentText().toLower();
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

    QString path = m_remotePathEdit->text();
    const QString protocol = m_protocolCombo->currentText().toLower();
    if (path.isEmpty()) {
        if (protocol == QLatin1String("webdav") || protocol == QLatin1String("webdavs")) {
            path = QStringLiteral("/");
        } else if (protocol == QLatin1String("ftp") || protocol == QLatin1String("sftp")) {
            path = QStringLiteral("~");
        }
    }
    m_client->list(currentConnection(), path);
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
    if (nameItem->data(Qt::UserRole + 2).toBool()) {
        goRemoteUp();
        return;
    }
    if (!isDirectory) {
        m_nextDownloadShouldOpen = true;
        downloadSelected();
        return;
    }

    m_remotePathEdit->setText(path);
    refreshRemote();
}

void MainWindow::openLocalEntry(int row, int column)
{
    Q_UNUSED(column)
    QTableWidgetItem *item = m_localView->item(row, 0);
    if (!item) {
        return;
    }
    if (item->data(Qt::UserRole + 2).toBool()) {
        goLocalUp();
        return;
    }
    const QString path = item->data(Qt::UserRole).toString();
    if (!QFileInfo(path).isDir()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        return;
    }
    m_localPathEdit->setText(path);
    loadLocalDirectory(path);
}

void MainWindow::handleRemoteCellClicked(int row, int column)
{
    if (column != 2) {
        return;
    }
    QTableWidgetItem *item = m_remoteTable->item(row, 0);
    if (!item || item->data(Qt::UserRole + 2).toBool() || !item->data(Qt::UserRole + 1).toBool()) {
        return;
    }
    calculateRemoteDirectorySize(row);
}

void MainWindow::uploadSelected()
{
    const QStringList paths = selectedLocalPaths();
    if (paths.isEmpty()) {
        QMessageBox::information(this, tr("Select file"), tr("Please select a local file to upload."));
        return;
    }
    m_uploadConflictChoice.clear();
    for (const QString &path : paths) {
        uploadPath(path, m_remotePathEdit->text());
    }
}

void MainWindow::downloadSelected()
{
    const QVector<RemoteEntry> entries = selectedRemoteEntries();
    if (entries.isEmpty()) {
        QMessageBox::information(this, tr("Select file"), tr("Please select a remote file to download."));
        return;
    }
    if (m_transferClient->isBusy()) {
        QMessageBox::information(this, tr("Transfer busy"), tr("A transfer is already running. Queueing multiple transfers is not implemented yet."));
        m_nextDownloadShouldOpen = false;
        return;
    }
    m_openDownloadedAfterTransfer = m_nextDownloadShouldOpen;
    m_nextDownloadShouldOpen = false;
    queueDownloads(entries, m_localPathEdit->text());
}

void MainWindow::deleteSelectedLocal()
{
    const QStringList paths = selectedLocalPaths();
    if (paths.isEmpty()) {
        return;
    }
    if (QMessageBox::question(this, tr("Delete local files"), tr("Delete selected local files?")) != QMessageBox::Yes) {
        return;
    }
    for (const QString &path : paths) {
        QFileInfo info(path);
        if (info.isDir()) {
            QDir(path).removeRecursively();
        } else {
            QFile::remove(path);
        }
    }
    loadLocalDirectory(m_localPathEdit->text());
}

void MainWindow::deleteSelectedRemote()
{
    const QVector<RemoteEntry> entries = selectedRemoteEntries();
    if (entries.isEmpty()) {
        return;
    }
    if (m_client->isBusy()) {
        return;
    }
    if (QMessageBox::question(this, tr("Delete remote files"), tr("Delete selected remote files?")) != QMessageBox::Yes) {
        return;
    }
    const QString protocol = m_protocolCombo->currentText().toLower();
    const bool serverSideDirectoryDelete = protocol == QLatin1String("webdav") || protocol == QLatin1String("webdavs");
    m_pendingRemoteDeletes.clear();
    for (const RemoteEntry &entry : entries) {
        if (serverSideDirectoryDelete) {
            m_pendingRemoteDeletes << entry;
        } else {
            collectRemoteDeletes(entry);
        }
    }
    startNextRemoteDelete();
}

void MainWindow::openLocalFile()
{
    const QString path = selectedLocalPath();
    if (!path.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void MainWindow::cancelSelectedTransfer()
{
    if (m_transferClient->isBusy()) {
        m_transferClient->cancel();
    }
}

void MainWindow::showLocalContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_localView->indexAt(pos);
    if (index.isValid()) {
        m_localView->setCurrentIndex(index);
    }

    const QString path = selectedLocalPath();
    const bool hasSelection = !path.isEmpty();
    const bool isDirectory = hasSelection && QFileInfo(path).isDir();

    QMenu menu(this);
    QAction *openAction = menu.addAction(isDirectory ? tr("Open Folder") : tr("Open File"));
    openAction->setIcon(QIcon::fromTheme(isDirectory ? QStringLiteral("folder-open") : QStringLiteral("document-open")));
    QAction *uploadAction = menu.addAction(tr("Upload"));
    uploadAction->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    QAction *deleteAction = menu.addAction(tr("Delete"));
    deleteAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    menu.addSeparator();
    QAction *refreshAction = menu.addAction(tr("Refresh"));
    refreshAction->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    openAction->setEnabled(hasSelection);
    uploadAction->setEnabled(hasSelection && !m_transferClient->isBusy());
    deleteAction->setEnabled(hasSelection);

    QAction *chosen = menu.exec(m_localView->viewport()->mapToGlobal(pos));
    if (chosen == openAction && hasSelection) {
        if (isDirectory) {
            m_localPathEdit->setText(path);
            loadLocalDirectory(path);
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    } else if (chosen == uploadAction) {
        uploadSelected();
    } else if (chosen == deleteAction) {
        deleteSelectedLocal();
    } else if (chosen == refreshAction) {
        loadLocalDirectory(m_localPathEdit->text());
    }
}

void MainWindow::showRemoteContextMenu(const QPoint &pos)
{
    const int row = m_remoteTable->rowAt(pos.y());
    if (row >= 0) {
        m_remoteTable->selectRow(row);
    }

    const RemoteEntry entry = selectedRemoteEntry();
    const bool hasSelection = !entry.name.isEmpty();

    QMenu menu(this);
    QAction *openAction = nullptr;
    QAction *downloadAction = nullptr;
    QAction *calculateSizeAction = nullptr;
    QAction *deleteAction = nullptr;
    if (hasSelection) {
        openAction = menu.addAction(entry.directory ? tr("Open Folder") : tr("Open File"));
        openAction->setIcon(QIcon::fromTheme(entry.directory ? QStringLiteral("folder-open") : QStringLiteral("document-open")));
        downloadAction = menu.addAction(tr("Download"));
        downloadAction->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
        if (entry.directory) {
            calculateSizeAction = menu.addAction(tr("Calculate Size"));
        }
        deleteAction = menu.addAction(tr("Delete"));
        deleteAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
        menu.addSeparator();
    }
    QAction *refreshAction = menu.addAction(tr("Refresh"));
    refreshAction->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    QAction *upAction = menu.addAction(tr("Go Up"));
    upAction->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));

    if (openAction) {
        openAction->setEnabled(entry.directory && !m_client->isBusy());
    }
    if (downloadAction) {
        downloadAction->setEnabled(!m_transferClient->isBusy());
    }
    if (calculateSizeAction) {
        calculateSizeAction->setEnabled(!m_client->isBusy());
    }
    if (deleteAction) {
        deleteAction->setEnabled(entry.name != QLatin1String("..") && !m_client->isBusy());
    }
    refreshAction->setEnabled(!m_client->isBusy());
    upAction->setEnabled(!m_client->isBusy());

    QAction *chosen = menu.exec(m_remoteTable->viewport()->mapToGlobal(pos));
    if (chosen == openAction) {
        m_remotePathEdit->setText(entry.path);
        refreshRemote();
    } else if (chosen == downloadAction) {
        downloadSelected();
    } else if (chosen == calculateSizeAction && row >= 0) {
        calculateRemoteDirectorySize(row);
    } else if (chosen == deleteAction) {
        deleteSelectedRemote();
    } else if (chosen == refreshAction) {
        refreshRemote();
    } else if (chosen == upAction) {
        goRemoteUp();
    }
}

void MainWindow::showTransferContextMenu(const QPoint &pos)
{
    QTableWidget *table = qobject_cast<QTableWidget *>(sender());
    if (!table) {
        return;
    }

    const int row = table->rowAt(pos.y());
    if (row >= 0 && !table->selectionModel()->isRowSelected(row, QModelIndex())) {
        table->selectRow(row);
    }

    QList<int> allRows;
    for (int i = 0; i < table->rowCount(); ++i) {
        allRows << i;
    }
    const bool hasSelection = !table->selectionModel()->selectedRows().isEmpty();

    QMenu menu(this);
    QAction *cancelAction = nullptr;
    if (table == m_transferTable) {
        cancelAction = menu.addAction(tr("Cancel Transfer"));
        cancelAction->setEnabled(m_transferClient->isBusy());
        menu.addSeparator();
    }
    QAction *deleteSelectedAction = menu.addAction(tr("Delete Selected Record"));
    deleteSelectedAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    deleteSelectedAction->setEnabled(hasSelection);
    QAction *deleteAllAction = nullptr;
    if (table == m_transferTable) {
        deleteAllAction = menu.addAction(tr("Delete Running Records"));
    } else if (table == m_completedTransferTable) {
        deleteAllAction = menu.addAction(tr("Delete Completed Records"));
    } else {
        deleteAllAction = menu.addAction(tr("Delete Error Records"));
    }
    deleteAllAction->setEnabled(!allRows.isEmpty());

    QAction *chosen = menu.exec(table->viewport()->mapToGlobal(pos));
    if (chosen == cancelAction && cancelAction) {
        cancelSelectedTransfer();
    } else if (chosen == deleteSelectedAction) {
        QList<int> selectedRows;
        const QModelIndexList indexes = table->selectionModel()->selectedRows();
        for (const QModelIndex &index : indexes) {
            selectedRows << index.row();
        }
        if (table == m_transferTable && !selectedRows.isEmpty()) {
            cancelSelectedTransfer();
        }
        removeTransferRows(table, selectedRows);
    } else if (chosen == deleteAllAction) {
        if (table == m_transferTable) {
            cancelSelectedTransfer();
        }
        removeTransferRows(table, allRows);
    }
}

void MainWindow::showSavedSitesDialog()
{
    DDialog dialog(this);
    dialog.setTitle(tr("Settings"));
    dialog.setMessage(tr("Manage saved remote connections"));
    dialog.addButton(tr("Close"), true);
    dialog.resize(720, 480);

    QWidget *central = new QWidget(&dialog);
    central->setMinimumSize(680, 360);
    QHBoxLayout *layout = new QHBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QGroupBox *listGroup = new QGroupBox(tr("Saved Connections"), central);
    QVBoxLayout *listLayout = new QVBoxLayout(listGroup);
    listLayout->setContentsMargins(8, 8, 8, 8);

    QListWidget *list = new QListWidget(listGroup);
    list->setMinimumWidth(240);
    list->setSpacing(4);
    list->setUniformItemSizes(true);
    list->setGridSize(QSize(220, 44));
    list->setAlternatingRowColors(true);
    list->setStyleSheet(QStringLiteral(
        "QListWidget { border: 1px solid palette(mid); border-radius: 8px; background: palette(base); padding: 4px; }"
        "QListWidget::item { height: 40px; border-radius: 6px; padding: 5px 8px; }"
        "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
        "QListWidget::item:disabled { color: palette(mid); }"));
    listLayout->addWidget(list);
    layout->addWidget(listGroup, 1);

    QGroupBox *detailsGroup = new QGroupBox(tr("Saved Connection"), central);
    QVBoxLayout *detailsLayout = new QVBoxLayout(detailsGroup);
    QFormLayout *form = new QFormLayout;
    QComboBox *protocolCombo = new QComboBox(detailsGroup);
    protocolCombo->addItems({QStringLiteral("FTP"), QStringLiteral("SFTP"), QStringLiteral("WebDAV"), QStringLiteral("WebDAVS")});
    QLineEdit *hostEdit = new QLineEdit(detailsGroup);
    QSpinBox *portSpin = new QSpinBox(detailsGroup);
    portSpin->setRange(0, 65535);
    portSpin->setSpecialValueText(tr("Auto"));
    QLineEdit *userEdit = new QLineEdit(detailsGroup);
    QLineEdit *passwordEdit = new QLineEdit(detailsGroup);
    passwordEdit->setEchoMode(QLineEdit::Password);
    QLineEdit *pathEdit = new QLineEdit(detailsGroup);
    pathEdit->setPlaceholderText(QStringLiteral("/"));
    form->addRow(tr("Protocol"), protocolCombo);
    form->addRow(tr("Host"), hostEdit);
    form->addRow(tr("Port"), portSpin);
    form->addRow(tr("User"), userEdit);
    form->addRow(tr("Password"), passwordEdit);
    form->addRow(tr("Path"), pathEdit);
    detailsLayout->addLayout(form);
    detailsLayout->addStretch(1);

    QHBoxLayout *buttons = new QHBoxLayout;
    DPushButton *newButton = new DPushButton(tr("New"), detailsGroup);
    DPushButton *saveButton = new DPushButton(tr("Save"), detailsGroup);
    DPushButton *loadButton = new DPushButton(tr("Load"), detailsGroup);
    DPushButton *deleteButton = new DPushButton(tr("Delete"), detailsGroup);
    newButton->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    saveButton->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    loadButton->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    deleteButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    buttons->addWidget(newButton);
    buttons->addStretch(1);
    buttons->addWidget(saveButton);
    buttons->addWidget(loadButton);
    buttons->addWidget(deleteButton);
    detailsLayout->addLayout(buttons);
    layout->addWidget(detailsGroup, 2);

    dialog.addContent(central, Qt::AlignCenter);

    auto defaultPort = [](const QString &protocol) {
        const QString normalized = protocol.toLower();
        if (normalized == QLatin1String("ftp")) {
            return 21;
        }
        if (normalized == QLatin1String("sftp")) {
            return 22;
        }
        if (normalized == QLatin1String("webdavs")) {
            return 443;
        }
        return 80;
    };

    auto defaultPath = [](const QString &protocol) {
        return protocol.toLower() == QLatin1String("sftp") ? QStringLiteral("~") : QStringLiteral("/");
    };

    auto clearForm = [=]() {
        list->clearSelection();
        list->setCurrentRow(-1);
        protocolCombo->setCurrentText(QStringLiteral("FTP"));
        hostEdit->clear();
        portSpin->setValue(defaultPort(protocolCombo->currentText()));
        userEdit->clear();
        passwordEdit->clear();
        pathEdit->setText(defaultPath(protocolCombo->currentText()));
        loadButton->setEnabled(false);
        deleteButton->setEnabled(false);
    };

    auto connectionFromForm = [=]() {
        RemoteConnection connection;
        connection.protocol = protocolCombo->currentText().toLower();
        connection.host = hostEdit->text().trimmed();
        connection.port = portSpin->value();
        connection.username = userEdit->text();
        connection.password = passwordEdit->text();
        connection.path = pathEdit->text().trimmed().isEmpty() ? defaultPath(connection.protocol) : pathEdit->text().trimmed();
        return connection;
    };

    std::function<void()> populateList = [&]() {
        list->clear();
        for (int i = 0; i < m_savedSites.size(); ++i) {
            QListWidgetItem *item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("network-server")), siteDisplayName(m_savedSites.at(i)), list);
            item->setData(Qt::UserRole, i);
            item->setSizeHint(QSize(220, 44));
            item->setToolTip(m_savedSites.at(i).path);
        }
        if (list->count() == 0) {
            QListWidgetItem *empty = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("list-add")), tr("No saved connections yet"), list);
            empty->setData(Qt::UserRole, -1);
            empty->setSizeHint(QSize(220, 44));
            empty->setFlags(Qt::NoItemFlags);
            empty->setToolTip(tr("Click New to add a saved connection."));
        }
        if (list->count() > 0) {
            list->setCurrentRow(0);
        }
    };

    auto updateDetails = [=]() {
        const QListWidgetItem *item = list->currentItem();
        const int index = item ? item->data(Qt::UserRole).toInt() : -1;
        const bool valid = index >= 0 && index < m_savedSites.size();
        if (valid) {
            const RemoteConnection connection = m_savedSites.at(index);
            protocolCombo->setCurrentText(protocolDisplayName(connection.protocol));
            hostEdit->setText(connection.host);
            portSpin->setValue(connection.port);
            userEdit->setText(connection.username);
            passwordEdit->setText(connection.password);
            pathEdit->setText(connection.path);
        }
        loadButton->setEnabled(valid);
        deleteButton->setEnabled(valid);
    };

    connect(protocolCombo, &QComboBox::currentTextChanged, &dialog, [=](const QString &protocol) {
        const QString oldDefaultPath = protocol.toLower() == QLatin1String("sftp") ? QStringLiteral("/") : QStringLiteral("~");
        portSpin->setValue(defaultPort(protocol));
        if (pathEdit->text().trimmed().isEmpty() || pathEdit->text().trimmed() == oldDefaultPath) {
            pathEdit->setText(defaultPath(protocol));
        }
    });
    connect(list, &QListWidget::currentRowChanged, &dialog, updateDetails);
    connect(newButton, &QPushButton::clicked, &dialog, clearForm);
    connect(saveButton, &QPushButton::clicked, &dialog, [=, &dialog, &populateList]() {
        const RemoteConnection connection = connectionFromForm();
        if (connection.host.isEmpty()) {
            QMessageBox::warning(&dialog, tr("Missing host"), tr("Please enter a host before saving."));
            return;
        }

        const QListWidgetItem *item = list->currentItem();
        const int selectedIndex = item ? item->data(Qt::UserRole).toInt() : -1;
        int saveIndex = selectedIndex >= 0 && selectedIndex < m_savedSites.size() ? selectedIndex : -1;
        if (saveIndex < 0) {
            const QString name = siteDisplayName(connection);
            for (int i = 0; i < m_savedSites.size(); ++i) {
                if (siteDisplayName(m_savedSites.at(i)) == name) {
                    saveIndex = i;
                    break;
                }
            }
        }

        if (saveIndex >= 0) {
            m_savedSites[saveIndex] = connection;
        } else {
            m_savedSites.append(connection);
            saveIndex = m_savedSites.size() - 1;
        }
        persistSavedSites();
        loadSavedSites();
        populateList();
        list->setCurrentRow(saveIndex);
    });
    connect(loadButton, &QPushButton::clicked, &dialog, [=, &dialog]() {
        const QListWidgetItem *item = list->currentItem();
        const int index = item ? item->data(Qt::UserRole).toInt() : -1;
        if (index < 0 || index >= m_savedSites.size()) {
            return;
        }
        const RemoteConnection connection = m_savedSites.at(index);
        m_protocolCombo->setCurrentText(protocolDisplayName(connection.protocol));
        m_hostEdit->setText(connection.host);
        m_portSpin->setValue(connection.port);
        m_userEdit->setText(connection.username);
        m_passwordEdit->setText(connection.password);
        m_remotePathEdit->setText(connection.path);
        dialog.accept();
    });
    connect(deleteButton, &QPushButton::clicked, &dialog, [=, &populateList]() {
        const QListWidgetItem *item = list->currentItem();
        const int index = item ? item->data(Qt::UserRole).toInt() : -1;
        if (index < 0 || index >= m_savedSites.size()) {
            return;
        }
        m_savedSites.removeAt(index);
        persistSavedSites();
        loadSavedSites();
        populateList();
        updateDetails();
    });

    populateList();
    if (!m_savedSites.isEmpty()) {
        updateDetails();
    } else {
        clearForm();
    }
    dialog.exec();
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
    m_protocolCombo->setCurrentText(protocolDisplayName(connection.protocol));
    m_hostEdit->setText(connection.host);
    m_portSpin->setValue(connection.port);
    m_userEdit->setText(connection.username);
    m_passwordEdit->setText(connection.password);
    m_remotePathEdit->setText(connection.path);
}

void MainWindow::showEntries(const QString &path, const QVector<RemoteEntry> &entries)
{
    setBrowsingBusy(false);
    m_remotePathEdit->setText(path);
    m_remoteTable->setRowCount(entries.size() + 1);

    QTableWidgetItem *up = new QTableWidgetItem(QIcon::fromTheme(QStringLiteral("go-up")), QStringLiteral(".."));
    up->setData(Qt::UserRole, parentPath(path));
    up->setData(Qt::UserRole + 1, true);
    up->setData(Qt::UserRole + 2, true);
    m_remoteTable->setItem(0, 0, up);
    m_remoteTable->setItem(0, 1, new QTableWidgetItem(tr("Parent")));
    m_remoteTable->setItem(0, 2, new QTableWidgetItem(QStringLiteral("-")));
    m_remoteTable->setItem(0, 3, new QTableWidgetItem(QString()));

    for (int row = 0; row < entries.size(); ++row) {
        const RemoteEntry &entry = entries.at(row);
        const int tableRow = row + 1;
        QTableWidgetItem *name = new QTableWidgetItem(fileIcon(entry.name, entry.directory), entry.name);
        name->setData(Qt::UserRole, entry.path);
        name->setData(Qt::UserRole + 1, entry.directory);
        name->setData(Qt::UserRole + 2, false);

        m_remoteTable->setItem(tableRow, 0, name);
        m_remoteTable->setItem(tableRow, 1, new QTableWidgetItem(entry.directory ? tr("Folder") : tr("File")));
        QTableWidgetItem *sizeItem = new QTableWidgetItem(entry.directory
            ? tr("Calculate size")
            : (entry.size >= 0 ? humanReadableSize(entry.size) : QStringLiteral("-")));
        sizeItem->setData(Qt::UserRole, entry.size);
        if (entry.directory) {
            QFont linkFont = sizeItem->font();
            linkFont.setUnderline(true);
            sizeItem->setFont(linkFont);
            sizeItem->setForeground(QColor(0, 102, 204));
            sizeItem->setToolTip(tr("Click to calculate the actual remote folder size"));
        }
        m_remoteTable->setItem(tableRow, 2, sizeItem);
        m_remoteTable->setItem(tableRow, 3, new QTableWidgetItem(entry.modified));
    }

    Q_UNUSED(path)
    m_remoteStatusLabel->setText(tr("%1 items").arg(entries.size()));
}

void MainWindow::showTransferFinished(const QString &source, const QString &destination)
{
    Q_UNUSED(source)
    Q_UNUSED(destination)
    setTransferBusy(false);
    updateFirstRunningTransfer(tr("Done"));
    if (m_lastTransferWasUpload) {
        if (!m_pendingUploadDirectories.isEmpty() || !m_pendingUploadLocalPaths.isEmpty()) {
            startNextUpload();
        } else {
            refreshRemote();
        }
    } else {
        loadLocalDirectory(m_localPathEdit->text());
        if (m_openDownloadedAfterTransfer) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(destination));
        }
        m_openDownloadedAfterTransfer = false;
        startNextDownload();
    }
}

void MainWindow::showTransferProgress(int percent)
{
    if (m_activeTransferRow < 0) {
        return;
    }
    QProgressBar *bar = qobject_cast<QProgressBar *>(m_transferTable->cellWidget(m_activeTransferRow, 3));
    if (bar) {
        bar->setRange(0, 100);
        bar->setValue(percent);
        bar->setFormat(QStringLiteral("%p%"));
    }
}

void MainWindow::showTransferSpeed(const QString &speed)
{
    if (m_activeTransferRow < 0) {
        return;
    }
    QTableWidgetItem *speedItem = m_transferTable->item(m_activeTransferRow, 4);
    if (speedItem) {
        speedItem->setText(speed);
    }
}

void MainWindow::showTransferCancelled()
{
    setTransferBusy(false);
    updateFirstRunningTransfer(tr("Cancelled"));
    m_pendingUploadDirectories.clear();
    m_pendingUploadLocalPaths.clear();
    m_pendingUploadRemotePaths.clear();
    m_pendingDownloads.clear();
    m_pendingDownloadLocalPaths.clear();
    m_openDownloadedAfterTransfer = false;
    m_nextDownloadShouldOpen = false;
}

void MainWindow::showRemoteRemoved(const QString &path)
{
    Q_UNUSED(path)
    setBrowsingBusy(false);
    if (!m_pendingRemoteDeletes.isEmpty()) {
        startNextRemoteDelete();
    } else {
        refreshRemote();
    }
}

void MainWindow::showRemoteMoved(const QString &source, const QString &destination)
{
    Q_UNUSED(source)
    Q_UNUSED(destination)
    setBrowsingBusy(false);
    if (!m_pendingRemoteMoves.isEmpty()) {
        startNextRemoteMove();
    } else {
        refreshRemote();
    }
}

void MainWindow::showTransferError(const QString &message, const QString &details)
{
    setTransferBusy(false);
    updateFirstRunningTransfer(tr("Failed"));
    m_pendingUploadDirectories.clear();
    m_pendingUploadLocalPaths.clear();
    m_pendingUploadRemotePaths.clear();
    m_pendingDownloads.clear();
    m_pendingDownloadLocalPaths.clear();
    m_openDownloadedAfterTransfer = false;
    m_nextDownloadShouldOpen = false;
    const QString fullMessage = details.isEmpty() ? message : QStringLiteral("%1\n%2").arg(message, details);
    appendLog(fullMessage);
    showCopyableWarning(tr("Transfer error"), message, details);
}

void MainWindow::showError(const QString &message, const QString &details)
{
    setBrowsingBusy(false);
    setTransferBusy(false);
    updateFirstRunningTransfer(tr("Failed"));
    const QString fullMessage = details.isEmpty() ? message : QStringLiteral("%1\n%2").arg(message, details);
    m_remoteStatusLabel->setText(message);
    appendLog(fullMessage);
    showCopyableWarning(tr("Remote error"), message, details);
}

void MainWindow::updateDefaultPort()
{
    const QString protocol = m_protocolCombo->currentText().toLower();
    if (protocol == QLatin1String("ftp")) {
        m_portSpin->setValue(21);
    } else if (protocol == QLatin1String("sftp")) {
        m_portSpin->setValue(22);
    } else if (protocol == QLatin1String("webdavs")) {
        m_portSpin->setValue(443);
    } else {
        m_portSpin->setValue(80);
    }

    const QString path = m_remotePathEdit->text().trimmed();
    if (protocol == QLatin1String("sftp")) {
        if (path.isEmpty() || path == QLatin1String("/")) {
            m_remotePathEdit->setText(QStringLiteral("~"));
        }
    } else if (protocol == QLatin1String("webdav") || protocol == QLatin1String("webdavs")) {
        if (path.isEmpty() || path == QLatin1String("~")) {
            m_remotePathEdit->setText(QStringLiteral("/"));
        }
    }
}

void MainWindow::setLocalPathFromEdit()
{
    const QString path = m_localPathEdit->text();
    if (!QFileInfo(path).isDir()) {
        QMessageBox::warning(this, tr("Invalid local path"), tr("The local path does not exist or is not a folder."));
        return;
    }
    loadLocalDirectory(path);
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

QString MainWindow::remoteFileName(const QString &path) const
{
    QString clean = path;
    while (clean.length() > 1 && clean.endsWith(QLatin1Char('/'))) {
        clean.chop(1);
    }
    const int slash = clean.lastIndexOf(QLatin1Char('/'));
    return slash >= 0 ? clean.mid(slash + 1) : clean;
}

QString MainWindow::humanReadableSize(qint64 size) const
{
    if (size < 0) {
        return QStringLiteral("-");
    }
    const QStringList units = {tr("B"), tr("KB"), tr("MB"), tr("GB"), tr("TB")};
    double value = size;
    int unit = 0;
    while (value >= 1024.0 && unit < units.size() - 1) {
        value /= 1024.0;
        ++unit;
    }
    return unit == 0 ? QStringLiteral("%1 %2").arg(size).arg(units.at(unit))
                     : QStringLiteral("%1 %2").arg(value, 0, 'f', value >= 10.0 ? 1 : 2).arg(units.at(unit));
}

QString MainWindow::remoteUrlForPath(const QString &path) const
{
    RemoteConnection connection = currentConnection();
    QString scheme = connection.protocol.toLower();
    if (scheme == QLatin1String("webdav")) {
        scheme = QStringLiteral("http");
    } else if (scheme == QLatin1String("webdavs")) {
        scheme = QStringLiteral("https");
    }

    QUrl url;
    url.setScheme(scheme);
    url.setHost(connection.host.trimmed());
    if (connection.port > 0) {
        url.setPort(connection.port);
    }
    url.setPath(path);
    if (!connection.username.isEmpty()) {
        url.setUserName(connection.username);
    }
    return url.toString(QUrl::FullyEncoded);
}

bool MainWindow::remoteEntryForPath(const QString &path, RemoteEntry *entry) const
{
    const QString parent = parentPath(path);
    QVector<RemoteEntry> entries;
    if (!listRemoteDirectorySync(parent, &entries)) {
        return false;
    }
    const QString targetName = remoteFileName(path);
    for (const RemoteEntry &candidate : entries) {
        if (candidate.path == path || candidate.name == targetName) {
            if (entry) {
                *entry = candidate;
            }
            return true;
        }
    }
    return false;
}

bool MainWindow::listRemoteDirectorySync(const QString &path, QVector<RemoteEntry> *entries) const
{
    RemoteClient lister;
    QEventLoop loop;
    bool ok = false;
    QObject::connect(&lister, &RemoteClient::listed, &loop, [&](const QString &listedPath, const QVector<RemoteEntry> &listedEntries) {
        Q_UNUSED(listedPath)
        if (entries) {
            *entries = listedEntries;
        }
        ok = true;
        loop.quit();
    });
    QObject::connect(&lister, &RemoteClient::failed, &loop, [&](const QString &message, const QString &details) {
        Q_UNUSED(message)
        Q_UNUSED(details)
        loop.quit();
    });
    lister.list(currentConnection(), path);
    loop.exec();
    return ok;
}

void MainWindow::loadLocalDirectory(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return;
    }

    m_localPathEdit->setText(dir.absolutePath());
    const QFileInfoList files = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
    m_localView->setRowCount(files.size() + 1);

    QTableWidgetItem *up = new QTableWidgetItem(QIcon::fromTheme(QStringLiteral("go-up")), QStringLiteral(".."));
    up->setData(Qt::UserRole, QFileInfo(dir.absolutePath()).dir().absolutePath());
    up->setData(Qt::UserRole + 1, true);
    up->setData(Qt::UserRole + 2, true);
    m_localView->setItem(0, 0, up);
    m_localView->setItem(0, 1, new QTableWidgetItem(tr("Parent")));
    m_localView->setItem(0, 2, new QTableWidgetItem(QStringLiteral("-")));
    m_localView->setItem(0, 3, new QTableWidgetItem(QString()));

    for (int i = 0; i < files.size(); ++i) {
        const QFileInfo info = files.at(i);
        const int row = i + 1;
        QTableWidgetItem *name = new QTableWidgetItem(fileIcon(info.fileName(), info.isDir()), info.fileName());
        name->setData(Qt::UserRole, info.absoluteFilePath());
        name->setData(Qt::UserRole + 1, info.isDir());
        name->setData(Qt::UserRole + 2, false);
        m_localView->setItem(row, 0, name);
        m_localView->setItem(row, 1, new QTableWidgetItem(info.isDir() ? tr("Folder") : tr("File")));
        m_localView->setItem(row, 2, new QTableWidgetItem(info.isDir() ? QStringLiteral("-") : humanReadableSize(info.size())));
        m_localView->setItem(row, 3, new QTableWidgetItem(info.lastModified().toString(Qt::ISODate)));
    }
}

QStringList MainWindow::selectedLocalPaths() const
{
    QStringList paths;
    const QModelIndexList rows = m_localView->selectionModel()->selectedRows();
    for (const QModelIndex &index : rows) {
        QTableWidgetItem *item = m_localView->item(index.row(), 0);
        if (!item || item->data(Qt::UserRole + 2).toBool()) {
            continue;
        }
        paths << item->data(Qt::UserRole).toString();
    }
    return paths;
}

QVector<RemoteEntry> MainWindow::selectedRemoteEntries() const
{
    QVector<RemoteEntry> entries;
    const QModelIndexList rows = m_remoteTable->selectionModel()->selectedRows();
    for (const QModelIndex &index : rows) {
        QTableWidgetItem *item = m_remoteTable->item(index.row(), 0);
        if (!item || item->data(Qt::UserRole + 2).toBool()) {
            continue;
        }
        RemoteEntry entry;
        entry.name = item->text();
        entry.path = item->data(Qt::UserRole).toString();
        entry.directory = item->data(Qt::UserRole + 1).toBool();
        QTableWidgetItem *sizeItem = m_remoteTable->item(index.row(), 2);
        entry.size = sizeItem ? sizeItem->data(Qt::UserRole).toLongLong() : -1;
        entries << entry;
    }
    return entries;
}

QVector<RemoteEntry> MainWindow::remoteEntriesForPaths(const QStringList &paths) const
{
    QVector<RemoteEntry> entries;
    for (int row = 0; row < m_remoteTable->rowCount(); ++row) {
        QTableWidgetItem *item = m_remoteTable->item(row, 0);
        if (!item || item->data(Qt::UserRole + 2).toBool()) {
            continue;
        }
        const QString path = item->data(Qt::UserRole).toString();
        if (!paths.contains(path)) {
            continue;
        }

        RemoteEntry entry;
        entry.name = item->text();
        entry.path = path;
        entry.directory = item->data(Qt::UserRole + 1).toBool();
        QTableWidgetItem *sizeItem = m_remoteTable->item(row, 2);
        entry.size = sizeItem ? sizeItem->data(Qt::UserRole).toLongLong() : -1;
        entries << entry;
    }
    return entries;
}

void MainWindow::queueDownloads(const QVector<RemoteEntry> &entries, const QString &localDirectory)
{
    QDir targetDir(localDirectory.isEmpty() ? m_localPathEdit->text() : localDirectory);
    if (!targetDir.exists()) {
        appendLog(tr("Download target folder does not exist: %1").arg(targetDir.absolutePath()));
        return;
    }

    for (const RemoteEntry &entry : entries) {
        if (entry.directory) {
            collectRemoteDownloads(entry, targetDir.filePath(entry.name));
            continue;
        }
        m_pendingDownloads << entry;
        m_pendingDownloadLocalPaths << targetDir.filePath(entry.name);
    }
    startNextDownload();
}

bool MainWindow::collectRemoteDownloads(const RemoteEntry &entry, const QString &localDirectory)
{
    QDir localDir(localDirectory);
    if (!localDir.exists() && !QDir().mkpath(localDir.absolutePath())) {
        appendLog(tr("Unable to create download folder: %1").arg(localDir.absolutePath()));
        return false;
    }

    RemoteClient lister;
    QEventLoop loop;
    QVector<RemoteEntry> children;
    bool ok = false;
    connect(&lister, &RemoteClient::listed, this, [&](const QString &path, const QVector<RemoteEntry> &entries) {
        Q_UNUSED(path)
        children = entries;
        ok = true;
        loop.quit();
    });
    connect(&lister, &RemoteClient::failed, this, [&](const QString &message, const QString &details) {
        appendLog(details.isEmpty() ? message : QStringLiteral("%1 %2").arg(message, details));
        loop.quit();
    });
    connect(&lister, &RemoteClient::logMessage, this, &MainWindow::appendLog);

    lister.list(currentConnection(), entry.path);
    loop.exec();
    if (!ok) {
        return false;
    }

    for (const RemoteEntry &child : children) {
        if (child.directory) {
            collectRemoteDownloads(child, localDir.filePath(child.name));
        } else {
            m_pendingDownloads << child;
            m_pendingDownloadLocalPaths << localDir.filePath(child.name);
        }
    }
    return true;
}

qint64 MainWindow::calculateRemoteDirectorySize(const RemoteEntry &entry) const
{
    QVector<RemoteEntry> children;
    if (!listRemoteDirectorySync(entry.path, &children)) {
        return -1;
    }
    qint64 total = 0;
    for (const RemoteEntry &child : children) {
        if (child.directory) {
            const qint64 childSize = calculateRemoteDirectorySize(child);
            if (childSize >= 0) {
                total += childSize;
            }
        } else if (child.size > 0) {
            total += child.size;
        }
    }
    return total;
}

void MainWindow::calculateRemoteDirectorySize(int row)
{
    QTableWidgetItem *nameItem = m_remoteTable->item(row, 0);
    QTableWidgetItem *sizeItem = m_remoteTable->item(row, 2);
    if (!nameItem || !sizeItem || !nameItem->data(Qt::UserRole + 1).toBool()) {
        return;
    }
    sizeItem->setText(tr("Calculating"));
    QApplication::processEvents();

    RemoteEntry entry;
    entry.name = nameItem->text();
    entry.path = nameItem->data(Qt::UserRole).toString();
    entry.directory = true;
    const qint64 size = calculateRemoteDirectorySize(entry);
    sizeItem->setData(Qt::UserRole, size);
    sizeItem->setText(size >= 0 ? humanReadableSize(size) : tr("Failed"));
    QFont font = sizeItem->font();
    font.setUnderline(false);
    sizeItem->setFont(font);
    sizeItem->setForeground(QBrush());
    sizeItem->setIcon(QIcon());
    sizeItem->setToolTip(QString());
}

void MainWindow::collectRemoteDeletes(const RemoteEntry &entry)
{
    if (entry.directory) {
        QVector<RemoteEntry> children;
        if (listRemoteDirectorySync(entry.path, &children)) {
            for (const RemoteEntry &child : children) {
                collectRemoteDeletes(child);
            }
        }
    }
    m_pendingRemoteDeletes << entry;
}

QString MainWindow::remoteDropBasePath(const QPoint &pos) const
{
    const QModelIndex index = m_remoteTable->indexAt(pos);
    if (index.isValid()) {
        QTableWidgetItem *item = m_remoteTable->item(index.row(), 0);
        if (item && item->data(Qt::UserRole + 1).toBool()) {
            return item->data(Qt::UserRole).toString();
        }
    }
    return m_remotePathEdit->text();
}

QString MainWindow::localDropDirectory(const QPoint &pos) const
{
    const QModelIndex index = m_localView->indexAt(pos);
    if (index.isValid()) {
        QTableWidgetItem *item = m_localView->item(index.row(), 0);
        if (item && item->data(Qt::UserRole + 1).toBool()) {
            return item->data(Qt::UserRole).toString();
        }
    }
    return m_localPathEdit->text();
}

void MainWindow::updateDropHover(QTableWidget *table, int row)
{
    int *hoverRow = table == m_remoteTable ? &m_remoteDropHoverRow : &m_localDropHoverRow;
    if (*hoverRow == row) {
        return;
    }
    if (*hoverRow >= 0 && *hoverRow < table->rowCount()) {
        for (int column = 0; column < table->columnCount(); ++column) {
            QTableWidgetItem *item = table->item(*hoverRow, column);
            if (item) {
                item->setBackground(QBrush());
            }
        }
    }
    *hoverRow = row;
    if (row >= 0 && row < table->rowCount()) {
        const QColor color = table->palette().highlight().color().lighter(150);
        for (int column = 0; column < table->columnCount(); ++column) {
            QTableWidgetItem *item = table->item(row, column);
            if (item) {
                item->setBackground(color);
            }
        }
    }
}

void MainWindow::clearDropHover(QTableWidget *table)
{
    updateDropHover(table, -1);
}

void MainWindow::autoScrollTable(QTableWidget *table, const QPoint &pos)
{
    QScrollBar *bar = table->verticalScrollBar();
    if (!bar) {
        return;
    }
    const int margin = 36;
    const int step = qMax(1, table->verticalHeader()->defaultSectionSize() / 2);
    if (pos.y() < margin) {
        bar->setValue(bar->value() - step);
    } else if (pos.y() > table->viewport()->height() - margin) {
        bar->setValue(bar->value() + step);
    }
}

void MainWindow::copyOrMoveLocalPaths(const QStringList &paths, const QString &localDirectory, bool move)
{
    QDir targetDir(localDirectory.isEmpty() ? m_localPathEdit->text() : localDirectory);
    if (!targetDir.exists()) {
        appendLog(tr("Local target folder does not exist: %1").arg(targetDir.absolutePath()));
        return;
    }

    bool changed = false;
    for (const QString &path : paths) {
        QFileInfo info(path);
        if (!info.exists()) {
            continue;
        }

        const QString source = info.absoluteFilePath();
        const QString destination = targetDir.filePath(info.fileName());
        if (source == destination || QFileInfo(destination).exists()) {
            appendLog(tr("Skipped local file because the target already exists: %1").arg(destination));
            continue;
        }
        if (move && info.isDir() && targetDir.absolutePath().startsWith(source + QLatin1Char('/'))) {
            appendLog(tr("Skipped moving a folder into itself: %1").arg(source));
            continue;
        }

        bool ok = false;
        if (move) {
            ok = info.isDir() ? QDir().rename(source, destination) : QFile::rename(source, destination);
        } else if (info.isDir()) {
            ok = copyDirectoryRecursively(source, destination);
        } else {
            ok = QFile::copy(source, destination);
        }

        if (!ok) {
            appendLog(tr("Failed to %1 %2 to %3").arg(move ? tr("move") : tr("copy"), source, destination));
            continue;
        }
        changed = true;
    }

    if (changed) {
        loadLocalDirectory(m_localPathEdit->text());
    }
}

void MainWindow::queueRemoteMoves(const QVector<RemoteEntry> &entries, const QString &remoteDirectory)
{
    QString targetDirectory = remoteDirectory;
    if (targetDirectory.isEmpty()) {
        targetDirectory = m_remotePathEdit->text();
    }
    if (!targetDirectory.endsWith(QLatin1Char('/'))) {
        targetDirectory.append(QLatin1Char('/'));
    }

    for (const RemoteEntry &entry : entries) {
        if (entry.directory && (entry.path == targetDirectory || targetDirectory.startsWith(entry.path))) {
            appendLog(tr("Skipped moving a remote folder into itself: %1").arg(entry.path));
            continue;
        }
        QString destination = joinRemotePath(targetDirectory, entry.name);
        if (entry.directory && !destination.endsWith(QLatin1Char('/'))) {
            destination.append(QLatin1Char('/'));
        }
        if (destination == entry.path) {
            continue;
        }
        m_pendingRemoteMoves << entry;
        m_pendingRemoteMoveDestinations << destination;
    }
    startNextRemoteMove();
}

bool MainWindow::confirmUploadConflict(const QString &localPath, const QString &remotePath, qint64 remoteSize, bool *skip)
{
    *skip = false;
    const QFileInfo localInfo(localPath);
    const auto applyChoice = [&](const QString &choice) -> bool {
        if (choice == QLatin1String("overwrite")) {
            return true;
        }
        if (choice == QLatin1String("newer")) {
            *skip = true;
            appendLog(tr("Skipped %1 because exact remote modification comparison is not available yet.").arg(remotePath));
            return true;
        }
        if (choice == QLatin1String("larger")) {
            *skip = remoteSize >= localInfo.size();
            return true;
        }
        if (choice == QLatin1String("skip")) {
            *skip = true;
            return true;
        }
        return false;
    };

    if (!m_uploadConflictChoice.isEmpty()) {
        return applyChoice(m_uploadConflictChoice);
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Remote file exists"));
    box.setText(tr("%1 already exists on the remote server.").arg(remoteFileName(remotePath)));
    QPushButton *overwriteButton = box.addButton(tr("Overwrite"), QMessageBox::DestructiveRole);
    QPushButton *newerButton = box.addButton(tr("Keep Newer"), QMessageBox::ActionRole);
    QPushButton *largerButton = box.addButton(tr("Keep Larger"), QMessageBox::ActionRole);
    QPushButton *skipButton = box.addButton(tr("Skip"), QMessageBox::RejectRole);
    box.addButton(QMessageBox::Cancel);
    QCheckBox *applyCheck = new QCheckBox(tr("Apply to this upload list"), &box);
    box.setCheckBox(applyCheck);
    box.exec();

    QAbstractButton *clicked = box.clickedButton();
    QString choice;
    if (clicked == overwriteButton) {
        choice = QStringLiteral("overwrite");
    } else if (clicked == newerButton) {
        choice = QStringLiteral("newer");
    } else if (clicked == largerButton) {
        choice = QStringLiteral("larger");
    } else if (clicked == skipButton) {
        choice = QStringLiteral("skip");
    } else {
        return false;
    }
    if (applyCheck->isChecked()) {
        m_uploadConflictChoice = choice;
    }
    return applyChoice(choice);
}

bool MainWindow::uploadPath(const QString &localPath, const QString &remoteBasePath)
{
    QFileInfo info(localPath);
    if (info.isDir()) {
        QDir sourceDir(localPath);
        const QString folderRemoteBase = joinRemotePath(remoteBasePath, info.fileName());
        if (!m_pendingUploadDirectories.contains(folderRemoteBase)) {
            m_pendingUploadDirectories << folderRemoteBase;
        }

        QDirIterator dirIt(localPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (dirIt.hasNext()) {
            const QString dirPath = dirIt.next();
            const QString relative = sourceDir.relativeFilePath(dirPath);
            const QString remoteDir = joinRemotePath(folderRemoteBase, relative);
            if (!m_pendingUploadDirectories.contains(remoteDir)) {
                m_pendingUploadDirectories << remoteDir;
            }
        }

        QDirIterator it(localPath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString filePath = it.next();
            const QString relative = sourceDir.relativeFilePath(filePath);
            const QString remotePath = joinRemotePath(folderRemoteBase, relative);
            RemoteEntry remoteEntry;
            bool skip = false;
            if (remoteEntryForPath(remotePath, &remoteEntry)
                && !confirmUploadConflict(filePath, remotePath, remoteEntry.size, &skip)) {
                m_pendingUploadDirectories.clear();
                m_pendingUploadLocalPaths.clear();
                m_pendingUploadRemotePaths.clear();
                return false;
            }
            if (!skip) {
                m_pendingUploadLocalPaths << filePath;
                m_pendingUploadRemotePaths << remotePath;
            }
        }
    } else {
        const QString remotePath = joinRemotePath(remoteBasePath, info.fileName());
        RemoteEntry remoteEntry;
        bool skip = false;
        if (remoteEntryForPath(remotePath, &remoteEntry)
            && !confirmUploadConflict(localPath, remotePath, remoteEntry.size, &skip)) {
            m_pendingUploadDirectories.clear();
            m_pendingUploadLocalPaths.clear();
            m_pendingUploadRemotePaths.clear();
            return false;
        }
        if (!skip) {
            m_pendingUploadLocalPaths << localPath;
            m_pendingUploadRemotePaths << remotePath;
        }
    }

    startNextUpload();
    return true;
}

void MainWindow::startNextUpload()
{
    if (m_transferClient->isBusy()) {
        return;
    }

    if (!m_pendingUploadDirectories.isEmpty()) {
        const QString remotePath = m_pendingUploadDirectories.takeFirst();
        m_activeTransferRow = addTransferRow(tr("Create Folder"), QString(), remotePath);
        m_lastTransferWasUpload = true;
        m_transferClient->makeDirectory(currentConnection(), remotePath);
        return;
    }

    if (m_pendingUploadLocalPaths.isEmpty() || m_pendingUploadRemotePaths.isEmpty()) {
        return;
    }

    const QString localPath = m_pendingUploadLocalPaths.takeFirst();
    const QString remotePath = m_pendingUploadRemotePaths.takeFirst();
    m_activeTransferRow = addTransferRow(tr("Upload"), localPath, remotePath, QFileInfo(localPath).size());
    m_lastTransferWasUpload = true;
    m_transferClient->upload(currentConnection(), localPath, remotePath);
}

void MainWindow::startNextDownload()
{
    if (m_transferClient->isBusy() || m_pendingDownloads.isEmpty() || m_pendingDownloadLocalPaths.isEmpty()) {
        return;
    }

    const RemoteEntry entry = m_pendingDownloads.takeFirst();
    const QString localPath = m_pendingDownloadLocalPaths.takeFirst();
    bool resume = false;
    if (!confirmDownloadConflict(entry, localPath, &resume)) {
        if (m_pendingDownloads.isEmpty()) {
            m_openDownloadedAfterTransfer = false;
        }
        startNextDownload();
        return;
    }
    m_activeTransferRow = addTransferRow(tr("Download"), entry.path, localPath, entry.size);
    m_lastTransferWasUpload = false;
    m_transferClient->download(currentConnection(), entry.path, localPath, resume);
}

void MainWindow::startNextRemoteDelete()
{
    if (m_client->isBusy() || m_pendingRemoteDeletes.isEmpty()) {
        return;
    }

    const RemoteEntry entry = m_pendingRemoteDeletes.takeFirst();
    m_client->remove(currentConnection(), entry.path, entry.directory);
}

void MainWindow::startNextRemoteMove()
{
    if (m_client->isBusy() || m_pendingRemoteMoves.isEmpty() || m_pendingRemoteMoveDestinations.isEmpty()) {
        return;
    }

    const RemoteEntry entry = m_pendingRemoteMoves.takeFirst();
    const QString destination = m_pendingRemoteMoveDestinations.takeFirst();
    m_client->move(currentConnection(), entry.path, destination);
}

void MainWindow::setBrowsingBusy(bool busy)
{
    m_connectButton->setEnabled(!busy);
    m_saveSiteButton->setEnabled(!busy);
    m_remoteRefreshButton->setEnabled(!busy);
    m_remoteUpButton->setEnabled(!busy);
}

void MainWindow::setTransferBusy(bool busy)
{
    m_uploadButton->setEnabled(!busy);
    m_downloadButton->setEnabled(!busy);
}

void MainWindow::appendLog(const QString &message)
{
    m_log->appendPlainText(QStringLiteral("[%1] %2")
                           .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message));
}

void MainWindow::showCopyableWarning(const QString &title, const QString &message, const QString &details)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(title);
    box.setText(message);
    box.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    if (!details.isEmpty()) {
        box.setDetailedText(details);
    }
    box.addButton(QMessageBox::Ok);
    box.exec();
}

int MainWindow::addTransferRow(const QString &direction, const QString &source, const QString &destination, qint64 totalBytes)
{
    const int row = m_transferTable->rowCount();
    m_transferTable->insertRow(row);
    m_transferTable->setItem(row, 0, new QTableWidgetItem(direction));
    m_transferTable->setItem(row, 1, new QTableWidgetItem(source));
    m_transferTable->setItem(row, 2, new QTableWidgetItem(destination));
    m_transferTable->setItem(row, 4, new QTableWidgetItem(QStringLiteral("-")));
    QProgressBar *progress = new QProgressBar(m_transferTable);
    progress->setRange(0, 0);
    progress->setFormat(tr("Starting"));
    m_transferTable->setCellWidget(row, 3, progress);
    m_transferTable->setItem(row, 3, new QTableWidgetItem(tr("Running")));
    m_activeTransferTotalBytes = totalBytes;
    m_lastTransferBytes = 0;
    m_lastTransferSpeedBytes = -1;
    m_transferSpeedTimer.restart();
    m_transferTable->scrollToBottom();
    return row;
}

void MainWindow::updateFirstRunningTransfer(const QString &status)
{
    for (int row = 0; row < m_transferTable->rowCount(); ++row) {
        QTableWidgetItem *item = m_transferTable->item(row, 3);
        if (item && item->text() == tr("Running")) {
            if (status == tr("Done")) {
                moveTransferRow(m_transferTable, row, m_completedTransferTable, status);
                return;
            }
            if (status != tr("Running")) {
                moveTransferRow(m_transferTable, row, m_errorTransferTable, status);
                return;
            }
            item->setText(status);
            QProgressBar *bar = qobject_cast<QProgressBar *>(m_transferTable->cellWidget(row, 3));
            if (bar) {
                bar->setRange(0, 100);
                bar->setValue(status == tr("Done") ? 100 : bar->value());
                bar->setFormat(status);
            }
            QTableWidgetItem *speedItem = m_transferTable->item(row, 4);
            if (speedItem && status != tr("Running")) {
                speedItem->setText(QStringLiteral("-"));
            }
            return;
        }
    }
}

void MainWindow::removeTransferRows(QTableWidget *table, const QList<int> &rows)
{
    QList<int> sortedRows = rows;
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());

    for (const int row : sortedRows) {
        if (row < 0 || row >= table->rowCount()) {
            continue;
        }
        if (table == m_transferTable && row == m_activeTransferRow) {
            m_activeTransferRow = -1;
        } else if (table == m_transferTable && m_activeTransferRow > row) {
            --m_activeTransferRow;
        }
        table->removeRow(row);
    }
}

void MainWindow::moveTransferRow(QTableWidget *sourceTable, int sourceRow, QTableWidget *targetTable, const QString &status)
{
    if (sourceRow < 0 || sourceRow >= sourceTable->rowCount()) {
        return;
    }

    const int targetRow = targetTable->rowCount();
    targetTable->insertRow(targetRow);
    for (int column = 0; column < sourceTable->columnCount(); ++column) {
        QTableWidgetItem *sourceItem = sourceTable->item(sourceRow, column);
        QString text = sourceItem ? sourceItem->text() : QString();
        if (column == 3) {
            text = status;
        } else if (column == 4 && status != tr("Running")) {
            text = QStringLiteral("-");
        }
        targetTable->setItem(targetRow, column, new QTableWidgetItem(text));
    }

    sourceTable->removeRow(sourceRow);
    if (sourceTable == m_transferTable) {
        m_activeTransferRow = -1;
    }
    targetTable->scrollToBottom();
}

bool MainWindow::confirmDownloadConflict(const RemoteEntry &entry, const QString &localPath, bool *resume)
{
    *resume = false;
    QFileInfo localInfo(localPath);
    if (!localInfo.exists()) {
        return true;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("File exists"));
    box.setText(tr("%1 already exists in the local folder.").arg(localInfo.fileName()));
    QPushButton *resumeButton = box.addButton(tr("Resume"), QMessageBox::AcceptRole);
    QPushButton *overwriteButton = box.addButton(tr("Overwrite"), QMessageBox::DestructiveRole);
    QPushButton *newerButton = box.addButton(tr("Keep Newer"), QMessageBox::ActionRole);
    QPushButton *largerButton = box.addButton(tr("Keep Larger"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    QCheckBox *applyCheck = new QCheckBox(tr("Apply to transfers in this session"), &box);
    box.setCheckBox(applyCheck);
    box.exec();

    QAbstractButton *clicked = box.clickedButton();
    Q_UNUSED(applyCheck)
    if (clicked == resumeButton) {
        *resume = true;
        return true;
    }
    if (clicked == overwriteButton) {
        QFile::remove(localPath);
        return true;
    }
    if (clicked == newerButton) {
        appendLog(tr("Kept existing %1 because exact remote modification comparison is not available yet.").arg(entry.name));
        if (m_openDownloadedAfterTransfer) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
        }
        return false;
    }
    if (clicked == largerButton) {
        if (entry.size > localInfo.size()) {
            QFile::remove(localPath);
            return true;
        }
        appendLog(tr("Kept existing %1 because the local file is larger or equal.").arg(entry.name));
        if (m_openDownloadedAfterTransfer) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
        }
        return false;
    }
    return false;
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
    return QStringLiteral("%1://%2:%3").arg(protocolDisplayName(connection.protocol), connection.host).arg(connection.port);
}

QString MainWindow::selectedLocalPath() const
{
    const QStringList paths = selectedLocalPaths();
    if (paths.isEmpty()) {
        return QString();
    }
    return paths.first();
}

RemoteEntry MainWindow::selectedRemoteEntry() const
{
    const QVector<RemoteEntry> entries = selectedRemoteEntries();
    return entries.isEmpty() ? RemoteEntry() : entries.first();
}
