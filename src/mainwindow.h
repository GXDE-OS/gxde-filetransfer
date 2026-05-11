#pragma once

#include "remoteclient.h"

#include <DMainWindow>

#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVector>

class QComboBox;
class QGroupBox;
class QSpinBox;
class QTabWidget;

class MainWindow : public Dtk::Widget::DMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void connectToRemote();
    void refreshRemote();
    void refreshLocal();
    void goRemoteUp();
    void goLocalUp();
    void openRemoteEntry(int row, int column);
    void openLocalEntry(int row, int column);
    void handleRemoteCellClicked(int row, int column);
    void uploadSelected();
    void downloadSelected();
    void deleteSelectedLocal();
    void deleteSelectedRemote();
    void createLocalFolder();
    void createRemoteFolder();
    void openLocalFile();
    void cancelSelectedTransfer();
    void showLocalContextMenu(const QPoint &pos);
    void showRemoteContextMenu(const QPoint &pos);
    void showTransferContextMenu(const QPoint &pos);
    void showSavedSitesDialog();
    void saveCurrentSite();
    void loadSelectedSite(int index);
    void showEntries(const QString &path, const QVector<RemoteEntry> &entries);
    void showTransferFinished(const QString &source, const QString &destination);
    void showTransferProgress(int percent);
    void showTransferSpeed(const QString &speed);
    void showTransferCancelled();
    void showRemoteRemoved(const QString &path);
    void showRemoteMoved(const QString &source, const QString &destination);
    void showTransferError(const QString &message, const QString &details);
    void showError(const QString &message, const QString &details);
    void updateDefaultPort();
    void setLocalPathFromEdit();

private:
    QWidget *createConnectionBar();
    QWidget *createBrowser();
    QWidget *createLocalPane();
    QWidget *createRemotePane();
    QWidget *createTransferPane();
    QTableWidget *createTransferTable(QWidget *parent);
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    RemoteConnection currentConnection() const;
    QString parentPath(const QString &path) const;
    QString joinRemotePath(const QString &basePath, const QString &name) const;
    QString remoteFileName(const QString &path) const;
    QString humanReadableSize(qint64 size) const;
    void loadLocalDirectory(const QString &path);
    void refreshRemoteIfReady();
    QStringList selectedLocalPaths() const;
    QVector<RemoteEntry> selectedRemoteEntries() const;
    bool uploadPath(const QString &localPath, const QString &remoteBasePath);
    QString remoteUrlForPath(const QString &path) const;
    bool remoteEntryForPath(const QString &path, RemoteEntry *entry) const;
    bool listRemoteDirectorySync(const QString &path, QVector<RemoteEntry> *entries) const;
    QVector<RemoteEntry> remoteEntriesForPaths(const QStringList &paths) const;
    void queueDownloads(const QVector<RemoteEntry> &entries, const QString &localDirectory);
    QString remoteDropBasePath(const QPoint &pos) const;
    QString localDropDirectory(const QPoint &pos) const;
    void updateDropHover(QTableWidget *table, int row);
    void clearDropHover(QTableWidget *table);
    void autoScrollTable(QTableWidget *table, const QPoint &pos);
    void copyOrMoveLocalPaths(const QStringList &paths, const QString &localDirectory, bool move);
    bool collectRemoteDownloads(const RemoteEntry &entry, const QString &localDirectory);
    qint64 calculateRemoteDirectorySize(const RemoteEntry &entry) const;
    void calculateRemoteDirectorySize(int row);
    void collectRemoteDeletes(const RemoteEntry &entry);
    void queueRemoteMoves(const QVector<RemoteEntry> &entries, const QString &remoteDirectory);
    bool confirmUploadConflict(const QString &localPath, const QString &remotePath, qint64 remoteSize, bool *skip);
    void startNextUpload();
    void startNextDownload();
    void startNextRemoteDelete();
    void startNextRemoteMove();
    void cancelTransferRows(const QList<int> &rows);
    void setBrowsingBusy(bool busy);
    void setTransferBusy(bool busy);
    void appendLog(const QString &message);
    void showCopyableWarning(const QString &title, const QString &message, const QString &details = QString());
    int addTransferRow(const QString &direction, const QString &source, const QString &destination, qint64 totalBytes = -1, bool active = true);
    void startTransferRow(int row, qint64 totalBytes);
    void updateFirstRunningTransfer(const QString &status);
    void removeTransferRows(QTableWidget *table, const QList<int> &rows);
    void moveTransferRow(QTableWidget *sourceTable, int sourceRow, QTableWidget *targetTable, const QString &status);
    void movePendingDownloadRowsToError(const QString &status);
    void movePendingUploadRowsToError(const QString &status);
    void adjustPendingDownloadRowsAfterRemoved(int removedRow);
    void adjustPendingUploadRowsAfterRemoved(int removedRow);
    bool removeQueuedTransferForRow(int row);
    void requeueFailedTransfers(const QList<int> &rows);
    bool confirmDownloadConflict(const RemoteEntry &entry, const QString &localPath, bool *resume);
    void loadSavedSites();
    void persistSavedSites();
    QString siteDisplayName(const RemoteConnection &connection) const;
    QString selectedLocalPath() const;
    RemoteEntry selectedRemoteEntry() const;

    QComboBox *m_siteCombo = nullptr;
    QComboBox *m_protocolCombo = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_userEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_remotePathEdit = nullptr;
    QLineEdit *m_localPathEdit = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_saveSiteButton = nullptr;
    QPushButton *m_uploadButton = nullptr;
    QPushButton *m_downloadButton = nullptr;
    QPushButton *m_localRefreshButton = nullptr;
    QPushButton *m_remoteRefreshButton = nullptr;
    QPushButton *m_remoteUpButton = nullptr;
    QPushButton *m_localUpButton = nullptr;
    QLabel *m_remoteStatusLabel = nullptr;
    QLabel *m_localStatusLabel = nullptr;
    QTableWidget *m_remoteTable = nullptr;
    QTableWidget *m_transferTable = nullptr;
    QTableWidget *m_completedTransferTable = nullptr;
    QTableWidget *m_errorTransferTable = nullptr;
    QTabWidget *m_transferTabs = nullptr;
    QGroupBox *m_logGroup = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QTableWidget *m_localView = nullptr;
    RemoteClient *m_client = nullptr;
    RemoteClient *m_transferClient = nullptr;
    QVector<RemoteConnection> m_savedSites;
    int m_remoteDropHoverRow = -1;
    int m_localDropHoverRow = -1;
    QString m_uploadConflictChoice;
    QStringList m_pendingUploadDirectories;
    QVector<int> m_pendingUploadDirectoryRows;
    QStringList m_pendingUploadLocalPaths;
    QStringList m_pendingUploadRemotePaths;
    QVector<int> m_pendingUploadRows;
    QVector<RemoteEntry> m_pendingDownloads;
    QStringList m_pendingDownloadLocalPaths;
    QVector<int> m_pendingDownloadRows;
    QVector<RemoteEntry> m_pendingRemoteDeletes;
    QVector<RemoteEntry> m_pendingRemoteMoves;
    QStringList m_pendingRemoteMoveDestinations;
    bool m_lastTransferWasUpload = false;
    bool m_nextDownloadShouldOpen = false;
    bool m_openDownloadedAfterTransfer = false;
    bool m_creatingRemoteFolder = false;
    int m_activeTransferRow = -1;
    qint64 m_activeTransferTotalBytes = -1;
    qint64 m_lastTransferBytes = 0;
    qint64 m_lastTransferSpeedBytes = -1;
    QElapsedTimer m_transferSpeedTimer;
};
