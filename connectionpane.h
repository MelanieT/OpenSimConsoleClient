#ifndef CONNECTIONPANE_H
#define CONNECTIONPANE_H

#include <QWidget>
#include <QMap>
#include <QUrl>
#include <QVariant>
#include <QDomNode>
#include <QHostAddress>

class QNetworkAccessManager;
class QNetworkReply;
class ConnectionData;

namespace Ui {
class ConnectionPane;
}

class ConnectionPane : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionPane(ConnectionData *c, QHostAddress addr, QWidget *parent = 0);
    ~ConnectionPane();
public slots:
    void CommandReply();
    void LoginReply();
    void PollReply();
    void TextChanged(QString text);
    void ReturnPressed();
    void Login();
public:
    void CloseConnection();
    void ClearScrollback();
    void Copy();
    void RestartServer();
    bool IsLoggedIn();

protected:
    QStringList CollectHelp(QStringList helpParts);
    QStringList Parse(QString text);
    QStringList Resolve(QStringList cmd);
    QStringList FindNextOption(QStringList cmd, bool term);
    static void Help(QString module, ConnectionPane *instance, QStringList args);
    static void Quit(QString module, ConnectionPane *instance, QStringList args);
    QStringList CollectHelp(QStringList help, QMap<QString, QVariant> level);
    static void CommandHandler(QString module, ConnectionPane *instance, QStringList args);
    QString FormatLine(QString line, QString level);
    QString OutputLine(QString line, QString level);
    QString GetColor(QString text);
    //void DumpTree(QMap<QString, QVariant> level);
    bool SendCommand(QString cmd);
    QMap<QString, QVariant> ProcessTreeLevel(QDomNode node, QMap<QString, QVariant> level);
    QString Name;
    QString Host;
    int Port;
    QString User;
    QString Pass;
    QUrl urlStart;
    QUrl urlClose;
    QUrl urlCommand;
    QUrl urlPoll;
    QString sessionID;
    QNetworkAccessManager *manager;
    QMap<QString, QVariant> tree;
    QNetworkReply *loginReply;
    QNetworkReply *pollReply;
    QNetworkReply *cmdReply;
    bool loggedIn;
    QString textContent;
    bool expectingInput;
    bool expectingCommand;

private:
    Ui::ConnectionPane *ui;
};

#endif // CONNECTIONPANE_H
