#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "connectiondata.h"
#include "groupdata.h"
#include "connectionpane.h"
#include <QSettings>
#include <QMessageBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QDebug>
#include <QDnsLookup>

#include "addconndialog.h"
#include "addgroupdialog.h"
#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"

enum ItemType
{
    GroupItem = QTreeWidgetItem::UserType,
    ConnectionItem = QTreeWidgetItem::UserType + 1
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->connList->setColumnCount(1);
    ui->connList->setHeaderLabel("Connections");
    ui->connList->setHeaderHidden(true);
    ui->connList->sortByColumn(0, Qt::AscendingOrder);
    ui->connList->setContextMenuPolicy(Qt::CustomContextMenu);

    connect (ui->consolePane, SIGNAL(tabCloseRequested(int)), this, SLOT(CloseTab(int)));
    connect (ui->connList, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(ConnContext(const QPoint &)));
    connect(ui->connList, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(ConnectionDoubleClicked(QTreeWidgetItem *, int)));
    connect(ui->connList, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(treeWidgetItemChanged(QTreeWidgetItem*,int)));

    QCoreApplication::setOrganizationName("OpenSimulator");
    QCoreApplication::setApplicationName("OpenSim Console Client");

    QSettings settings;

    ui->connList->blockSignals(true);

    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());

    blackOnWhite = settings.value("black_on_white", QVariant(true)).toBool();
    systemFont = settings.value("system_font", QVariant(false)).toBool();

    if (settings.value("split").isValid())
        ui->splitter->restoreState(settings.value("split").toByteArray());

    int groupCount = settings.beginReadArray(QString("Groups"));
    for (int i = 0 ; i < groupCount ; i++)
    {
        settings.setArrayIndex(i);

        GroupData *grp = new GroupData();

        grp->Name = settings.value("Name").toString();
        grp->Uuid = settings.value("Uuid").toUuid();
        QVariant dnsAddress = settings.value("Dns");
        if (dnsAddress.isValid())
            grp->Dns = QHostAddress(dnsAddress.toString());

        groups[grp->Uuid] = grp;

        insertGroup(grp);

        QTreeWidgetItem *grpItem = findItem(grp->Uuid);

        if (settings.value("Expanded").toBool())
            grpItem->setExpanded(true);
    }
    settings.endArray();

    int connCount = settings.beginReadArray(QString("Connections"));

    for (int i = 0 ; i < connCount ; i++)
    {
        settings.setArrayIndex(i);

        ConnectionData *c = new ConnectionData;

        c->Name = settings.value("Name").toString();
        c->Host = settings.value("Host").toString();
        c->Port = settings.value("Port").toInt();
        c->User = settings.value("User").toString();
        c->Pass = settings.value("Pass").toString();
        c->Uuid = settings.value("Uuid").toUuid();

        connections[c->Uuid] = c;

        QTreeWidgetItem *connItem;

        QUuid parent = settings.value("Parent").toUuid();
        if (!parent.isNull())
        {
            QTreeWidgetItem *parentItem = findItem(parent);
            if (parentItem == 0)
                continue;

            connItem = new QTreeWidgetItem(parentItem, QStringList(QString(c->Name)), ConnectionItem);
        }
        else
        {
            connItem = new QTreeWidgetItem(ui->connList, QStringList(QString(c->Name)), ConnectionItem);
        }

        connItem->setIcon(0, QIcon(":/Icons/computer.png"));
        connItem->setData(0, Qt::UserRole, c->Uuid);
    }
    settings.endArray();

    ui->connList->blockSignals(false);

    ui->connList->sortItems(0, Qt::AscendingOrder);
}

MainWindow::~MainWindow()
{
    WriteSettings();

    // No need to free anything as the OS does it for us

    delete ui;
}

void MainWindow::on_action_Exit_triggered()
{
    close();
}

void MainWindow::on_action_Connect_triggered()
{
    QTreeWidgetItem *item = selectedItem();
    if (!item)
        return;

    if (item->type() == GroupItem) // Group
    {
        QUuid groupUuid = item->data(0, Qt::UserRole).toUuid();

        if (!groups.contains(groupUuid))
            return;

        for (int i = 0 ; i < item->childCount() ; i ++)
        {
            QTreeWidgetItem *it = item->child(i);

            QUuid uuid = it->data(0, Qt::UserRole).toUuid();

            if (!connections.contains(uuid))
                continue;

            AddNewTab(groups[groupUuid], connections[uuid], groups[groupUuid]->Dns);
        }
        return;
    }

    QUuid uuid = item->data(0, Qt::UserRole).toUuid();

    if (!connections.contains(uuid))
        return;

    ConnectionData *c = connections[uuid];

    QHostAddress dns;

    if (item->parent() != 0)
    {
        QTreeWidgetItem *parentItem = item->parent();
        QUuid parentUuid = parentItem->data(0, Qt::UserRole).toUuid();

        if (groups.contains(parentUuid))
            dns = groups[parentUuid]->Dns;
    }
    AddNewTab(0, c, dns);
}

void MainWindow::on_action_Disconnect_triggered()
{
    ConnectionPane *cw = GetCurrentTab();
    if (!cw)
        return;

    cw->CloseConnection();

    QTabWidget *p = (QTabWidget *)cw->parent()->parent();

    QWidget *pp = (QWidget *)p->parent()->parent();

    if (pp && pp->inherits("QTabWidget") && p->count() == 1)
    {
        delete cw;
        delete p;
    }
    else
    {
        delete cw;
    }
}

void MainWindow::on_action_Edit_triggered()
{
    QTreeWidgetItem *item = selectedItem();
    if (item == 0)
        return;

    QUuid uuid = item->data(0, Qt::UserRole).toUuid();

    if (connections.contains(uuid))
    {
        ConnectionData *c = connections[uuid];

        AddConnDialog dlg(this);

        dlg.Name->setEnabled(false);
        dlg.Name->setText(c->Name);
        dlg.Host->setText(c->Host);
        dlg.Port->setValue(c->Port);
        dlg.User->setText(c->User);
        dlg.Pass->setText(c->Pass);

        if (dlg.exec() < 0)
            return;

        c->Host = dlg.Host->text();
        c->Port = dlg.Port->value();
        c->User = dlg.User->text();
        c->Pass = dlg.Pass->text();
    }

    if (groups.contains(uuid))
    {
        GroupData *g = groups[uuid];

        AddGroupDialog dlg(this);

        dlg.Name->setEnabled(false);
        dlg.Name->setText(g->Name);
        dlg.Dns->setText(g->Dns.toString());

        if (dlg.exec() < 0)
            return;

        if (dlg.Dns->text() != "")
            g->Dns = QHostAddress(dlg.Dns->text());
        else
            g->Dns = QHostAddress();
    }

    WriteSettings();
}

void MainWindow::on_action_Restart_triggered()
{
    ConnectionPane *c = GetCurrentTab();
    if (!c)
        return;

    c->RestartServer();
}

void MainWindow::on_action_Clear_text_triggered()
{
    ConnectionPane *cw = GetCurrentTab();
    if (!cw)
        return;

    cw->ClearScrollback();
}

// Recursively find a tab with a given UUID. This returns
// both sub-tab widgets and actual connection panes.
QWidget *MainWindow::findTab(QTabWidget *parent, QUuid uuid)
{
    int count = parent->count();

    for (int i = 0 ; i < count ; i++)
    {
        QWidget *w = parent->widget(i);

        QVariant u = w->property("UUID");
        if (!u.isValid())
            continue;

        if (u.toUuid() == uuid)
        {
            return w;
        }

        if (w->inherits("QTabWidget"))
        {
            w = findTab((QTabWidget *)w, uuid);
            if (w != 0)
                return w;
        }
    }

    return 0;
}

void MainWindow::AddNewTab(GroupData *grp, ConnectionData *conn, QHostAddress addr)
{
    // Assume that it's a root pane
    QTabWidget *parent = ui->consolePane;

    // See if there is a connection present and open. If there is one,
    // just refuse to add another. If it's not connected, close the old
    // pane and another will be created later.
    QWidget *w = findTab(ui->consolePane, conn->Uuid);
    if (w != 0 && w->inherits("ConnectionPane"))
    {
        ConnectionPane *c = (ConnectionPane *)w;

        if (c->IsLoggedIn())
            return;

        delete c;
    }

    // If this is a group pane being opened, see if there is already
    // a multipane for this group. If not, create it.
    if (grp != 0)
    {
        w = findTab(ui->consolePane, grp->Uuid);

        if (w == 0 || (!w->inherits("QTabWidget")))
        {
            parent = new QTabWidget(ui->consolePane);
            parent->setTabPosition(QTabWidget::North);
            parent->setTabShape(QTabWidget::Triangular);
            parent->setProperty("UUID", QVariant(grp->Uuid));
            ui->consolePane->addTab(parent, grp->Name);
            ui->consolePane->setCurrentWidget(parent);
        }
        else if (w->inherits("QTabWidget"))
        {
            parent = (QTabWidget *)w;
        }
    }

    ConnectionPane *tabContents = new ConnectionPane(conn, addr, parent);
    tabContents->setVisible(true);
    tabContents->setProperty("UUID", conn->Uuid);

    parent->addTab(tabContents, conn->Name);
    parent->setCurrentWidget(tabContents);

    tabContents->Login();
}

void MainWindow::WriteSettings()
{
    QSettings settings;

    settings.clear();

    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());
    settings.setValue("split", ui->splitter->saveState());

    settings.setValue("black_on_white", blackOnWhite);
    settings.setValue("system_font", systemFont);

    settings.beginWriteArray(QString("Groups"), groups.count());

    int idx = 0;
    for (QMap<QUuid, GroupData *>::iterator it = groups.begin() ; it != groups.end() ; it++, idx++)
    {
        QTreeWidgetItem *grpItem = findItem((*it)->Uuid);

        if (grpItem == 0)
            continue;

        settings.setArrayIndex(idx);
        settings.setValue("Uuid", QVariant((*it)->Uuid));
        settings.setValue("Name", QVariant((*it)->Name));
        if (!(*it)->Dns.isNull())
            settings.setValue("Dns", (*it)->Dns.toString());
        settings.setValue("Expanded", QVariant(grpItem->isExpanded()));
    }

    settings.endArray();

    settings.beginWriteArray(QString("Connections"), connections.count());

    idx = 0;
    for (QMap<QUuid, ConnectionData *>::iterator it = connections.begin() ; it != connections.end() ; it++, idx++)
    {
        QUuid parent;

        if (!(*it)->Uuid.isNull())
        {
            QTreeWidgetItem *item = findItem((*it)->Uuid);
            if (item == 0)
                continue;

            if (item->parent() != 0)
                parent = item->parent()->data(0, Qt::UserRole).toUuid();
        }

        settings.setArrayIndex(idx);
        settings.setValue("Uuid", QVariant((*it)->Uuid));
        settings.setValue("Name", QVariant((*it)->Name));
        settings.setValue("Host", QVariant((*it)->Host));
        settings.setValue("Port", QVariant((*it)->Port));
        settings.setValue("User", QVariant((*it)->User));
        settings.setValue("Pass", QVariant((*it)->Pass));
        settings.setValue("Parent", (QVariant(parent)));
    }

    settings.endArray();
}

void MainWindow::ConnectionDoubleClicked(QTreeWidgetItem *, int)
{
    ui->action_Connect->trigger();
}

ConnectionPane * MainWindow::GetCurrentTab()
{
    if (ui->consolePane->count() == 0)
        return 0;
    if (ui->consolePane->count() == 1 && ui->consolePane->tabText(0) == "Not connected")
        return 0;

    QWidget *w = ui->consolePane->currentWidget();

    if (w->inherits("QTabWidget")) // Sub tabs
    {
        if (((QTabWidget *)w)->count() == 0)
            return 0;

        w = ((QTabWidget *)w)->currentWidget();
    }
    ConnectionPane *cw = dynamic_cast<ConnectionPane *>(w);
    return cw;
}


void MainWindow::ConnContext(const QPoint &pos)
{
    QTreeWidgetItem *item = ui->connList->itemAt(pos);
    ui->connList->clearSelection();
    if (item)
        item->setSelected(true);

    QMenu ctx("Connection actions", this);
    if (item != 0)
    {
        if (item->type() == GroupItem)
        {
            if (item->childCount() > 0)
            {
                connect (ctx.addAction("Connect Group"), SIGNAL(triggered()), ui->action_Connect, SLOT(trigger()));
                ctx.addSeparator();
            }
        }
        else
        {
            connect (ctx.addAction("Connect"), SIGNAL(triggered()), ui->action_Connect, SLOT(trigger()));
            ctx.addSeparator();
        }
        connect (ctx.addAction("Add ..."), SIGNAL(triggered()), ui->action_New, SLOT(trigger()));
    }
    else
    {
        connect (ctx.addAction("Add ..."), SIGNAL(triggered()), this, SLOT(addRootConnection()));
    }
    if (item != 0)
        connect (ctx.addAction("Edit ..."), SIGNAL(triggered()), ui->action_Edit, SLOT(trigger()));
    if (item != 0 && item->type() != GroupItem)
        connect (ctx.addAction("Delete"), SIGNAL(triggered()), ui->actionDelete, SLOT(trigger()));
    else if (item != 0 && item->childCount() == 0)
        connect (ctx.addAction("Delete"), SIGNAL(triggered()), ui->actionDelete_group, SLOT(trigger()));
    ctx.addSeparator();
    connect (ctx.addAction("Add Group..."), SIGNAL(triggered()), ui->actionNew_group, SLOT(trigger()));

    ctx.exec(ui->connList->mapToGlobal(pos));
}


void MainWindow::CloseTab(int index)
{
    QWidget *w = ui->consolePane->widget(index);

    if (w->inherits("QTabWidget"))
    {
        QTabWidget *tabs = (QTabWidget *)w;

        while (tabs->count())
        {
            ConnectionPane *c = (ConnectionPane *)tabs->widget(0);

            c->CloseConnection();

            delete c;
        }

        delete w;
    }
    else
    {
        if (w->inherits("ConnectionPane"))
        {
            ConnectionPane *c = (ConnectionPane *)w;

            c->CloseConnection();
        }

        delete w;
    }
}

void MainWindow::on_actionCopy_triggered()
{
    ConnectionPane *cw = GetCurrentTab();
    if (!cw)
        return;

    cw->Copy();
}

// Helper to get the selected item from the list.
QTreeWidgetItem *MainWindow::selectedItem()
{
    QList<QTreeWidgetItem *> items = ui->connList->selectedItems();
    if (items.count() != 1)
        return 0;

    return items.at(0);
}

// User requests a new group to be created.
void MainWindow::on_actionNew_group_triggered()
{
    GroupData *grp= createGroup();

    QTreeWidgetItem *item = insertGroup(grp);

    ui->connList->editItem(item, 0);
}

// Insert a group into the tree view
QTreeWidgetItem *MainWindow::insertGroup(GroupData *grp)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(ui->connList, QStringList(grp->Name), GroupItem);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    item->setData(0, Qt::UserRole, grp->Uuid);
    item->setIcon(0, QIcon(":/Icons/folder.png"));

    ui->connList->sortItems(0, Qt::AscendingOrder);

    return item;
}

// Create a new group record and insert it into the groups list.
// Doesn't show or edit it.
GroupData *MainWindow::createGroup()
{
    GroupData *grp = new GroupData();

    grp->Uuid = QUuid::createUuid();
    grp->Name = "New group";

    groups[grp->Uuid] = grp;

    return grp;
}

// Find any item in the list. Works for both groups and connections.
QTreeWidgetItem *MainWindow::findItem(QUuid uuid)
{
    QTreeWidgetItemIterator it(ui->connList);

    while (*it)
    {
        if ((*it)->data(0, Qt::UserRole).toUuid() == uuid)
            return (*it);
        it++;
    }

    return 0;
}

// Delete a group from the list
void MainWindow::on_actionDelete_group_triggered()
{
    QTreeWidgetItem *item = selectedItem();
    if (item == 0)
        return;

    if (item->childCount() > 0 || item->type() != GroupItem)
        return;

    if (QMessageBox::question(0, QString("Delete group"),
            QString("Really delete this group?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes) != QMessageBox::Yes)
        return;

    QUuid uuid = item->data(0, Qt::UserRole).toUuid();

    if (groups.contains(uuid))
    {
        GroupData *grp = groups[uuid];
        groups.remove(uuid);
        delete grp;
    }

    delete item;

    WriteSettings();
}

void MainWindow::on_action_New_triggered()
{
    QList<QTreeWidgetItem *>items = ui->connList->selectedItems();

    if (items.count() == 0)
        addRootConnection();
    else
        addChildConnection();

    WriteSettings();
}

ConnectionData *MainWindow::createConnection()
{
    AddConnDialog dlg(this);
    if (dlg.exec() < 0)
        return 0;

    ConnectionData *c = new ConnectionData;
    c->Name = dlg.Name->text();
    c->Host = dlg.Host->text();
    c->Port = dlg.Port->value();
    c->User = dlg.User->text();
    c->Pass = dlg.Pass->text();
    c->Uuid = QUuid::createUuid();

    connections[c->Uuid.toString()] = c;

    return c;
}

void MainWindow::addRootConnection()
{
    ConnectionData *c = createConnection();

    if (c == 0)
        return;

    QTreeWidgetItem *item = addItem(ui->connList, c);
    if (!item)
        connections.remove(c->Uuid);
}

void MainWindow::addChildConnection()
{
    QTreeWidgetItem *parent = selectedItem();

    if (parent == 0)
        return;

    ConnectionData *c = createConnection();

    if (c == 0)
        return;

    QTreeWidgetItem *item = addItem(parent, c);
    if (!item)
        connections.remove(c->Uuid);
}

void MainWindow::setupItem(QTreeWidgetItem *item, ConnectionData *c)
{
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    item->setData(0, Qt::UserRole, c->Uuid);
    item->setIcon(0, QIcon(":/Icons/computer.png"));

    ui->connList->clearSelection();
    item->setSelected(true);
}

QTreeWidgetItem *MainWindow::addItem(QTreeWidget *v, ConnectionData *c)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(v, QStringList(QString(c->Name)), ConnectionItem);
    ui->connList->sortItems(0, Qt::AscendingOrder);

    setupItem(item, c);

    return item;
}

QTreeWidgetItem *MainWindow::addItem(QTreeWidgetItem *i, ConnectionData *c)
{
    i->setExpanded(true);
    QTreeWidgetItem *item = new QTreeWidgetItem(i, QStringList(QString(c->Name)), ConnectionItem);
    ui->connList->sortItems(0, Qt::AscendingOrder);

    setupItem(item, c);

    return item;
}

void MainWindow::on_actionDelete_triggered()
{
    QTreeWidgetItem *item = selectedItem();

    if (item == 0 || item->type() != ConnectionItem)
        return;

    QUuid id = item->data(0, Qt::UserRole).toUuid();
    if (QMessageBox::question(0, QString("Delete connection"),
            QString("Really delete this connection?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes) != QMessageBox::Yes)
        return;

    if (connections.contains(id))
    {
        ConnectionData *c = connections[id];
        connections.remove(c->Uuid);
        delete c;
    }
    delete item;

    WriteSettings();
}

void MainWindow::treeWidgetItemChanged(QTreeWidgetItem *item, int)
{
    QUuid uuid = item->data(0, Qt::UserRole).toUuid();

    if (groups.contains(uuid))
        groups[uuid]->Name = item->text(0);
    if (connections.contains(uuid))
        connections[uuid]->Name = item->text(0);

    WriteSettings();
}

void MainWindow::on_action_Preferences_triggered()
{
    PreferencesDialog dlg;

    dlg.ui->blackOnWhite->setChecked(blackOnWhite);
    dlg.ui->useSystemFonts->setChecked(systemFont);

    if (dlg.exec() < 0)
        return;

    blackOnWhite = dlg.ui->blackOnWhite->isChecked();
    systemFont = dlg.ui->useSystemFonts->isChecked();

    WriteSettings();
}
