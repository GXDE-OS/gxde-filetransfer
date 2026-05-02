#pragma once

#include "remoteclient.h"

#include <DMainWindow>

#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>

class QComboBox;
class QSpinBox;

class MainWindow : public Dtk::Widget::DMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void connectToRemote();
    void refresh();
    void goUp();
    void openEntry(int row, int column);
    void showEntries(const QString &path, const QVector<RemoteEntry> &entries);
    void showError(const QString &message, const QString &details);
    void updateDefaultPort();

private:
    QWidget *createConnectionBar();
    QWidget *createBrowser();
    RemoteConnection currentConnection() const;
    QString parentPath(const QString &path) const;
    void setBusy(bool busy);
    void appendLog(const QString &message);

    QComboBox *m_protocolCombo = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_userEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_upButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTableWidget *m_table = nullptr;
    QPlainTextEdit *m_log = nullptr;
    RemoteClient *m_client = nullptr;
};
