#pragma once

#include "remoteclient.h"

#include <DMainWindow>

#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVector>

class QComboBox;
class QSpinBox;

class MainWindow : public Dtk::Widget::DMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void connectToRemote();
    void refreshRemote();
    void goRemoteUp();
    void goLocalUp();
    void openRemoteEntry(int row, int column);
    void openLocalEntry(int row, int column);
    void uploadSelected();
    void downloadSelected();
    void deleteSelectedLocal();
    void deleteSelectedRemote();
    void openLocalFile();
    void cancelSelectedTransfer();
    void showLocalContextMenu(const QPoint &pos);
    void showRemoteContextMenu(const QPoint &pos);
    void showTransferContextMenu(const QPoint &pos);
    void saveCurrentSite();
    void loadSelectedSite(int index);
    void showEntries(const QString &path, const QVector<RemoteEntry> &entries);
    void showTransferFinished(const QString &source, const QString &destination);
    void showTransferProgress(int percent);
    void showTransferCancelled();
    void showRemoteRemoved(const QString &path);
    void showError(const QString &message, const QString &details);
    void updateDefaultPort();
    void setLocalPathFromEdit();

private:
    QWidget *createConnectionBar();
    QWidget *createBrowser();
    QWidget *createLocalPane();
    QWidget *createRemotePane();
    QWidget *createTransferPane();
    bool eventFilter(QObject *watched, QEvent *event) override;
    RemoteConnection currentConnection() const;
    QString parentPath(const QString &path) const;
    QString joinRemotePath(const QString &basePath, const QString &name) const;
    void loadLocalDirectory(const QString &path);
    QStringList selectedLocalPaths() const;
    QVector<RemoteEntry> selectedRemoteEntries() const;
    bool uploadPath(const QString &localPath, const QString &remoteBasePath);
    void startNextUpload();
    void startNextDownload();
    void startNextRemoteDelete();
    void setBrowsingBusy(bool busy);
    void setTransferBusy(bool busy);
    void appendLog(const QString &message);
    int addTransferRow(const QString &direction, const QString &source, const QString &destination);
    void updateFirstRunningTransfer(const QString &status);
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
    QPushButton *m_remoteRefreshButton = nullptr;
    QPushButton *m_remoteUpButton = nullptr;
    QPushButton *m_localUpButton = nullptr;
    QLabel *m_remoteStatusLabel = nullptr;
    QLabel *m_localStatusLabel = nullptr;
    QTableWidget *m_remoteTable = nullptr;
    QTableWidget *m_transferTable = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QTableWidget *m_localView = nullptr;
    RemoteClient *m_client = nullptr;
    RemoteClient *m_transferClient = nullptr;
    QVector<RemoteConnection> m_savedSites;
    QStringList m_pendingUploadLocalPaths;
    QStringList m_pendingUploadRemotePaths;
    QVector<RemoteEntry> m_pendingDownloads;
    QVector<RemoteEntry> m_pendingRemoteDeletes;
    bool m_lastTransferWasUpload = false;
    int m_activeTransferRow = -1;
};
