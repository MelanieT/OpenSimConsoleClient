#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QMap>
#include <QUuid>
#include <QHostAddress>

class ConnectionPane;
class QTreeWidget;
class QTreeWidgetItem;
class ConnectionData;
class GroupData;
class QSettings;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
protected:
    ConnectionPane * GetCurrentTab();
    void WriteSettings();

private slots:
    void on_action_Exit_triggered();

    void on_action_Connect_triggered();

    void on_action_New_triggered();

    void on_actionDelete_group_triggered();

    void on_action_Disconnect_triggered();

    void on_action_Edit_triggered();

    void on_actionDelete_triggered();

    void on_action_Restart_triggered();

    void on_action_Clear_text_triggered();

    void CloseTab(int index);
    void ConnContext(const QPoint &);
    void ConnectionDoubleClicked(QTreeWidgetItem * item, int column = 0);
    void on_actionCopy_triggered();

    void on_actionNew_group_triggered();

    void on_action_Preferences_triggered();

public:
protected:
    QMap<QUuid, ConnectionData *> connections;
    QMap<QUuid, GroupData *> groups;
    ConnectionData *createConnection();
    QTreeWidgetItem *addItem(QTreeWidget *v, ConnectionData *c);
    QTreeWidgetItem *addItem(QTreeWidgetItem *i, ConnectionData *c);
    void setupItem(QTreeWidgetItem *item, ConnectionData *c);
    GroupData *createGroup();
    QTreeWidgetItem *insertGroup(GroupData *grp);
    QTreeWidgetItem *findItem(QUuid uuid);
    QTreeWidgetItem *selectedItem();
    QWidget *findTab(QTabWidget *parent, QUuid uuid);
    void AddNewTab(GroupData *grp, ConnectionData *conn, QHostAddress addr);
    bool blackOnWhite;
    bool systemFont;

protected slots:
    void addRootConnection();
    void addChildConnection();
    void treeWidgetItemChanged(QTreeWidgetItem *item, int column);
private:
    QTreeWidgetItem *findOrAddGroup(QString groupName);
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
