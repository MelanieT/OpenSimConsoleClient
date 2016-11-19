#include "connectionpane.h"
#include "ui_connectionpane.h"

#include <QUrl>
#include <QUrlQuery>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomAttr>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QStringList>
#include <QRegExp>
#include <QList>
#include <QTimer>
#include <QDnsLookup>
#include <QEventLoop>
#include <QDebug>
#include <QFontDatabase>
#include <QSettings>

#include "connectiondata.h"

struct CommandData
{
public:
    QString Module;
    QString HelpText;
    QString LongHelp;
    QString Description;
    ConnectionPane *instance;
    void (*fn)(QString module, ConnectionPane *instance, QStringList args);
};

Q_DECLARE_METATYPE (CommandData)

ConnectionPane::ConnectionPane(ConnectionData *c, QHostAddress addr, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConnectionPane)
{
    ui->setupUi(this);

    Name = c->Name;
    Host = c->Host;
    Port = c->Port;
    User = c->User;
    Pass = c->Pass;

    loggedIn = false;

    QDnsLookup lookup;

    if (!addr.isNull())
    {
        lookup.setNameserver(addr);

        QHostAddress dest(Host);
        if (dest.isNull())
        {
            lookup.setName(Host);
            lookup.setType(QDnsLookup::A);

            QEventLoop loop;
            connect(&lookup, SIGNAL(finished()), &loop, SLOT(quit()));

            lookup.lookup();
            loop.exec();

            if (lookup.error() == QDnsLookup::NoError)
                Host = lookup.hostAddressRecords().first().value().toString();
        }
    }

    // Construct the URLs
    urlStart.setScheme(QString("http"));
    urlStart.setHost(Host);
    urlStart.setPort(Port);

    urlClose = urlStart;
    urlCommand = urlStart;
    urlPoll = urlStart;

    urlStart.setPath(QString("/StartSession/"));
    urlClose.setPath(QString("/CloseSession/"));
    urlCommand.setPath(QString("/SessionCommand/"));

    // Construct the manager
    manager = new QNetworkAccessManager(this);

    pollReply = 0;
    loginReply = 0;
    cmdReply = 0;

    int id = QFontDatabase::addApplicationFont(":/fonts/Consolas.ttf");
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);

    QSettings settings;

    QString styleSheet;

    if (!settings.value("system_font", false).toBool())
        styleSheet += "font-family: " + family + ";";
    if (settings.value("black_on_white", false).toBool())
        styleSheet += "background-color: black; color: white;";
    else
        styleSheet += "background-color: #fcfcfc; color: black;";
    styleSheet += "font-size: 18pt;";

    ui->mainPane->setStyleSheet(styleSheet);
}

ConnectionPane::~ConnectionPane()
{
    QNetworkAccessManager *m = manager;
    manager = 0;
    delete m;

    delete ui;
}

void ConnectionPane::CommandReply()
{
    QByteArray result = cmdReply->readAll();
    cmdReply->deleteLater();
    cmdReply = 0;

    ui->textEntry->setText(QString(""));
    TextChanged("");
}

void ConnectionPane::LoginReply()
{
    QByteArray result = loginReply->readAll();
    loginReply->deleteLater();

    if (result.size() == 0)
    {
        ui->mainPane->setHtml("<font color=\"#7f7f7f\">Connecting ...<br></font><font color=\"#ff0000\">Error!</font>");
        QMessageBox::critical(0, QString("Connection error"), QString("Connection to host failed"));
        return;
    }

    QDomDocument doc;
    doc.setContent(result, false);

    QDomElement root = doc.documentElement();

    // Get session ID
    QDomNodeList sessionL = root.elementsByTagName(QString("SessionID"));
    QDomNode sessionNode = sessionL.at(0);
    sessionID = sessionNode.toElement().text();

    urlPoll.setPath(QString("/ReadResponses/")+sessionID+QString("/"));

    QDomNodeList helpL = root.elementsByTagName(QString("HelpTree"));
    QDomNode helpNode = helpL.at(0);

    ui->mainPane->setHtml(ui->mainPane->toHtml() + QString("<font color=\"#7f7f7f\">Connected</font>"));
    tree.clear();

    tree = ProcessTreeLevel(helpNode, tree);

    QMap<QString, QVariant> nextLevel;
    if (tree.contains("help"))
        nextLevel = tree["help"].toMap();

    CommandData cmd;
    cmd.Module = "Local";
    cmd.HelpText = "help [<command>]";
    cmd.LongHelp = "Provide help for commands";
    cmd.Description = "Provide help for commands";
    cmd.instance = this;
    cmd.fn = Help;

    QVariant v;
    v.setValue<CommandData>(cmd);
    nextLevel[""] = v;

    tree["help"] = nextLevel;

    nextLevel.clear();
    if (tree.contains("quit:"))
        nextLevel = tree["quit"].toMap();

    cmd.Module = "Local";
    cmd.HelpText = "quit";
    cmd.LongHelp = "Quit console session";
    cmd.Description = "Quit console session";
    cmd.instance = this;
    cmd.fn = Quit;

    v.setValue<CommandData>(cmd);
    nextLevel[""] = v;

    tree["quit"] = nextLevel;

    // Construct the poll request
    QString data = "";
    QNetworkRequest request(urlPoll);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));

    // Start poll
    pollReply = manager->post(request, data.toLatin1());
    connect(pollReply, SIGNAL(finished()), this, SLOT(PollReply()));

    ui->textEntry->setFocus();
    TextChanged(QString(""));

    connect(ui->textEntry, SIGNAL(returnPressed()), this, SLOT(ReturnPressed()));
    connect(ui->textEntry, SIGNAL(textChanged(QString)), this, SLOT(TextChanged(QString)));

    loggedIn = true;
}

void ConnectionPane::PollReply()
{
    if (manager == 0)
        return;

    QByteArray result = pollReply->readAll();

    //qDebug() << QString(result);
    pollReply->deleteLater();

    QString fullText;

    if (result.size() != 0)
    {
        QDomDocument doc;
        doc.setContent(result, false);

        QDomElement root = doc.documentElement();

        QDomNodeList lineL = root.elementsByTagName(QString("Line"));

        for (int i = 0 ; i < lineL.count() ; i++)
        {
            QDomElement e = lineL.at(i).toElement();

            QString level;
            bool prompt = false;
            bool command = false;
            bool input = false;

            if (e.hasAttribute("Level"))
                level = e.attributeNode("Level").value();

            if (e.hasAttribute("Prompt"))
            {
                if (e.attributeNode("Prompt").value() == "true")
                    prompt = true;
            }

            if (e.hasAttribute("Command"))
            {
                if (e.attributeNode("Command").value() == "true")
                    command = true;
            }

            if (e.hasAttribute("Input"))
            {
                if (e.attributeNode("Input").value() == "true")
                    input = true;
            }

            QString line = e.text();

            if (!input)
                fullText += QString("<br>");
            fullText += FormatLine(line.trimmed(), level).replace("\n", "<br>");
            if (fullText.endsWith("<br>"))
                fullText = fullText.mid(fullText.length() - 4);

            if (prompt || command)
            {
                expectingInput = true;
                if (command)
                    expectingCommand = true;
            }
        }
    }

    textContent += fullText;
    if (textContent.length() > 131400)
        textContent = textContent.right(131400);

    ui->mainPane->setHtml(textContent);

    ui->mainPane->moveCursor(QTextCursor::End);
    ui->mainPane->ensureCursorVisible();

    if (!loggedIn)
        return;

    TextChanged(ui->textEntry->text());

    // Construct the poll request
    QString data = "";
    QNetworkRequest request(urlPoll);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));

    // Poll again
    pollReply = manager->post(request, data.toLatin1());
    connect(pollReply, SIGNAL(finished()), this, SLOT(PollReply()));
}

void ConnectionPane::TextChanged(QString text)
{
    if (!expectingInput)
        ui->helpArea->hide();
    else if (!expectingCommand)
        ui->helpArea->hide();
    else
        ui->helpArea->show();

    ui->mainPane->moveCursor(QTextCursor::End);
    ui->mainPane->ensureCursorVisible();

    QStringList words = Parse(text);

    bool trailingSpace = text.right(1) == " ";

    QStringList opts = FindNextOption(words, trailingSpace);

    if (opts.size() == 0)
    {
        ui->helpArea->setText(QString(""));
    }
    else
    {
        QString first = opts.at(0);
        if (first.indexOf(QString("Command help:")) == 0)
            ui->helpArea->setText(first);
        else if (text != "")
        {
            QString output;
            output.sprintf("Options: %s", opts.join(" ").toLocal8Bit().data());
            ui->helpArea->setText(output);
        }
        else
        {
            QString output;
            output.sprintf("Commands: %s", opts.join(" ").toLocal8Bit().data());
            ui->helpArea->setText(output);
        }
    }
}

void ConnectionPane::ReturnPressed()
{
    // Construct the command
    QString cmd = ui->textEntry->text();

    if ((!expectingCommand) && expectingInput)
    {
        if (SendCommand(cmd))
        {
          ui->textEntry->setText(QString(""));
          TextChanged(ui->textEntry->text());
        }
    }

    QStringList resolved = Resolve(Parse(cmd));
    QString historyEntry = resolved.join(" ");
}

void ConnectionPane::Login()
{
    QUrlQuery queryString;

    queryString.addQueryItem("USER", User);
    queryString.addQueryItem("PASS", Pass);

    // Construct the login request
    QString data = queryString.query(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest request(urlStart);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));

    // Log in

    ui->mainPane->setHtml(ui->mainPane->toHtml() + QString("<font color=\"#7f7f7f\">Connecting ...</font>"));
    loginReply = manager->post(request, data.toLatin1());
    connect(loginReply, SIGNAL(finished()), this, SLOT(LoginReply()));
}

void ConnectionPane::CloseConnection()
{
    close();
}

void ConnectionPane::ClearScrollback()
{
    ui->mainPane->setHtml(QString(""));
}

void ConnectionPane::Copy()
{
    ui->mainPane->copy();
}

void ConnectionPane::RestartServer()
{
    if (!loggedIn)
        return;

    SendCommand("quit");

    ui->mainPane->setHtml(ui->mainPane->toHtml() + QString("<font color=\"#7f7f7f\">Disconnected</font>"));
    pollReply->abort();

    loggedIn = false;

    QTimer::singleShot(5000, this, SLOT(Login()));
}

bool ConnectionPane::IsLoggedIn()
{
    return loggedIn;
}

QStringList ConnectionPane::CollectHelp(QStringList helpParts)
{
    QString originalHelpRequest = helpParts.join(" ");

    QStringList help;

    QMap<QString, QVariant> dict = tree;
    while (helpParts.size() > 0)
    {
        QString helpPart = helpParts.at(0);

        if (!dict.contains(helpPart))
            break;

        QVariant val = dict[helpPart];
        if (QString(val.typeName()) == QString("QVariantMap"))
        {
            dict = val.toMap();
        }

        helpParts.removeFirst();

    }

    if (dict.contains(QString("")))
    {
        QVariant v = dict[QString("")];
        CommandData ci = v.value<CommandData>();

        help.append(ci.HelpText);
        help.append(ci.LongHelp);
        // help.append(ci.Description);
    }
    else
    {
        help.append(QString("No help is available for ") + originalHelpRequest);
    }

    return help;}

QStringList ConnectionPane::Parse(QString text)
{
    QStringList result;
    int index = 0;

    QStringList unquoted = text.split("\"");

    for (index = 0 ; index < unquoted.size() ; index++)
    {
        if ((index % 2) == 0)
        {
            QStringList words = unquoted.at(index).split(" ");

            for (int i = 0 ; i < words.size() ; i++)
            {
                QString w = words.at(i);
                if (w != QString(""))
                    result.append(w);
            }
        }
        else
        {
            result.append(unquoted.at(index));
        }
    }

    return result;
}

QStringList ConnectionPane::Resolve(QStringList cmd)
{
    QStringList result(cmd);
    int index = -1;

    QMap<QString, QVariant> current = tree;

    for (int i = 0 ; i < cmd.size() ; i++)
    {
        QString s = cmd.at(i);

        index++;

        QStringList found;

        for (QMap<QString, QVariant>::iterator it = current.begin() ; it != current.end() ; it++)
        {
            QString opt = it.key();

            if (opt == s)
            {
                found.clear();
                found.append(opt);
                break;
            }
            if (opt.indexOf(s) == 0)
            {
                found.append(opt);
            }
        }

        if (found.size() == 1)
        {
            result[index] = found.at(0);
            current = current[found.at(0)].toMap();
        }
        else if (found.size() > 0)
        {
            QStringList blank;
            return blank;
        }
        else
        {
            break;
        }
    }

    if (current.contains(QString("")))
    {
        QVariant v = current[QString("")];
        CommandData ci = v.value<CommandData>();

        if (!ci.fn)
            return QStringList();

        (*ci.fn)(ci.Module, ci.instance, result);

        return result;
    }

    return QStringList();
}

QStringList ConnectionPane::FindNextOption(QStringList cmd, bool term)
{
    QMap<QString, QVariant> current = tree;

    int remaining = cmd.size();

    for (int i = 0 ; i < cmd.size() ; i++)
    {
        QString s = cmd.at(i);

        remaining--;

        QStringList found;

        for (QMap<QString, QVariant>::iterator it = current.begin() ; it != current.end() ; it++)
        {
            QString opt = it.key();

            if (remaining > 0 && opt == s)
            {
                found.clear();
                found.append(opt);
                break;
            }
            if (opt.indexOf(s) == 0)
            {
                found.append(opt);
            }
        }

        if (found.size() == 1 && ((remaining != 0) || term))
        {
            current = current[found.at(0)].toMap();
        }
        else if (found.size() > 0)
        {
            return found;
        }
        else
        {
            break;
        }
    }

    if (current.size() > 1)
    {
        QStringList choices;

        bool addcr = false;
        for (QMap<QString, QVariant>::iterator it = current.begin() ; it != current.end() ; it++)
        {
            QString s = it.key();

            if (s == QString(""))
            {
                QVariant v = current[""];
                CommandData ci = v.value<CommandData>();
                if (ci.fn)
                    addcr = true;

            }
            else
                choices.append(s);
        }
        if (addcr)
            choices.append("<cr>");

        return choices;
    }

    if (current.contains(QString("")))
    {
        QVariant v = current[""];
        CommandData ci = v.value<CommandData>();

        QStringList ch;
        ch.append(QString("Command help: ") + ci.HelpText);
        return ch;
    }

    QList<QString> kl = current.keys();
    QStringList ksl(kl);

    return ksl;
}

void ConnectionPane::Help(QString, ConnectionPane *instance, QStringList args)
{
    instance->ui->mainPane->setHtml(instance->ui->mainPane->toHtml()+instance->OutputLine(QString("# ") + args.join(" "), "command"));

    instance->ui->mainPane->moveCursor(QTextCursor::End);
    instance->ui->mainPane->ensureCursorVisible();

    QStringList help;
    QStringList helpParts(args);

    helpParts.removeFirst();

    instance->ui->textEntry->setText(QString(""));
    instance->ui->helpArea->setText(QString(""));

    if (helpParts.size() == 0)
        help = instance->CollectHelp(help, instance->tree);
    else
        help = help + instance->CollectHelp(helpParts);

    for (int i = 0 ; i < help.size() ; i++)
    {
        instance->ui->mainPane->setHtml(instance->ui->mainPane->toHtml()+instance->OutputLine(help.at(i), "normal"));
    }
    instance->ui->mainPane->moveCursor(QTextCursor::End);
    instance->ui->mainPane->ensureCursorVisible();
}

void ConnectionPane::Quit(QString, ConnectionPane *instance, QStringList)
{
    instance->ui->mainPane->setHtml(instance->ui->mainPane->toHtml()+instance->OutputLine("# quit", "command"));
    instance->ui->mainPane->setHtml(instance->ui->mainPane->toHtml()+instance->OutputLine("Use the toolbar button to close session", "error"));
    instance->ui->mainPane->moveCursor(QTextCursor::End);
    instance->ui->mainPane->ensureCursorVisible();

    instance->ui->textEntry->setText(QString(""));
    instance->ui->helpArea->setText(QString(""));
}

QStringList ConnectionPane::CollectHelp(QStringList help, QMap<QString, QVariant> level)
{
    for (QMap<QString, QVariant>::iterator it = level.begin() ; it != level.end() ; it++)
    {
        QString key = it.key();
        QVariant val = it.value();

        if (QString(val.typeName()) == QString("QVariantMap"))
        {
            QMap<QString, QVariant> nextLevel = val.toMap();
            help = CollectHelp(help, nextLevel);
        }
        else
        {
            CommandData cmd = val.value<CommandData>();
            QString item = cmd.HelpText + QString(" - ") + cmd.LongHelp;
            help.append(item);
        }
    }

    return help;
}

void ConnectionPane::CommandHandler(QString, ConnectionPane *instance, QStringList args)
{
    QString cmd = args.join(" ");
    if(instance->SendCommand(cmd))
    {
        //instance->ui->mainPane->setHtml(instance->ui->mainPane->toHtml()+instance->OutputLine(QString("# ") + args.join(" "), "command"));

        //instance->ui->mainPane->moveCursor(QTextCursor::End);
        //instance->ui->mainPane->ensureCursorVisible();

        //instance->ui->textEntry->setText(QString(""));
        //instance->ui->helpArea->setText(QString(""));

    }
}

QString ConnectionPane::FormatLine(QString line, QString level)
{
    if (level == "")
        return line;

    QRegExp re(QString("^(.*)\\[(.*)\\](.*)$"));
    QString ret;

    if (re.exactMatch(line))
    {
        QString color = GetColor(re.cap(2));
        ret = re.cap(1)+QString("[<font color=\"")+color+QString("\">")+re.cap(2)+QString("</font>]");

        line = re.cap(3);
    }

    if (level == QString("error"))
    {
        ret += QString("<font color=\"#ff0000\">") + QString(line.toHtmlEscaped()) + QString("</font>");
    }
    else if (level == QString("warn"))
    {
        ret += QString("<font color=\"#cfcf00\">") + QString(line.toHtmlEscaped()) + QString("</font>");
    }
    else if (level == QString("command"))
    {
        ret += QString("<font color=\"#0000ff\">") + QString(line.toHtmlEscaped()) + QString("</font>");
    }
    else
    {
        ret += QString(line.toHtmlEscaped());
    }

    return ret;
}

QString ConnectionPane::OutputLine(QString line, QString level)
{
    QString ret;

    if (level == QString("error"))
    {
        ret += QString("<font color=\"#ff0000\">") + QString(line.toHtmlEscaped()) + QString("</font>");
    }
    else if (level == QString("warn"))
    {
        ret += QString("<font color=\"#cfcf00\">") + QString(line.toHtmlEscaped()) + QString("</font>");
    }
    else if (level == QString("command"))
    {
        ret += QString("<font color=\"#0000ff\">") + QString(line.toHtmlEscaped()) + QString("</font>");
    }
    else
    {
        ret += QString(line.toHtmlEscaped());
    }

    return ret;
}

QString ConnectionPane::GetColor(QString text)
{
    QList<QString> colors;

    colors.append(QString("#7f7f7f"));
    colors.append(QString("#0000ff"));
    colors.append(QString("#00cf00"));
    colors.append(QString("#00cfcf"));
    colors.append(QString("#cf00cf"));
    colors.append(QString("#cfcf00"));

    return colors.at(qHash(text.toUpper()) % colors.size());
}

/*
void ConnectionPane::DumpTree(QMap<QString, QVariant> level)
{
    QMap<QString, QVariant>::iterator it;

    for (it = level.begin() ; it != level.end() ; it++)
    {
        QVariant val = it.value();
        QString key = it.key();

        if (QString(val.typeName()) == QString("QVariantMap"))
        {
            printf("Level: %s\n", key.toAscii().data());
            QMap<QString, QVariant> nextLevel = val.toMap();
            DumpTree(nextLevel);
        }
        else
        {
            CommandData cmd = val.value<CommandData>();
            printf("%s\n", cmd.HelpText.toAscii().data());
        }
    }
}
*/

bool ConnectionPane::SendCommand(QString cmd)
{
    if (cmdReply)
        return false;

    QUrlQuery queryString;

    queryString.addQueryItem("ID", sessionID);
    queryString.addQueryItem("COMMAND", cmd);

    QString data = queryString.query(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest request(urlCommand);

    // Send it
    cmdReply = manager->post(request, data.toLatin1());
    connect(cmdReply, SIGNAL(finished()), this, SLOT(CommandReply()));

    return true;
}

QMap<QString, QVariant> ConnectionPane::ProcessTreeLevel(QDomNode node, QMap<QString, QVariant> level)
{
    QDomNodeList levelL = node.childNodes();
    for (int i = 0 ; i < levelL.count() ; i++)
    {
        QDomNode n = levelL.at(i);
        if (n.isElement())
        {
            QDomElement e = n.toElement();
            if (e.tagName() == QString("Level"))
            {
                QDomNode a = e.attributeNode(QString("Name"));
                QDomAttr att = a.toAttr();
                QString name = att.value();

                QMap<QString, QVariant> nextLevel;
                level[name] = ProcessTreeLevel(n, nextLevel);
            }
            else if (e.tagName() == QString("Command"))
            {
                CommandData cmd;

                QDomNodeList cmdL = n.childNodes();
                for (int j = 0 ; j < cmdL.count() ; j++)
                {
                    QDomNode cmdN = cmdL.at(j);
                    QDomElement cmdE = cmdN.toElement();

                    if (cmdE.tagName() == QString("Module"))
                        cmd.Module = cmdE.text();
                    else if(cmdE.tagName() == QString("HelpText"))
                        cmd.HelpText = cmdE.text();
                    else if(cmdE.tagName() == QString("LongHelp"))
                        cmd.LongHelp = cmdE.text();
                    else if(cmdE.tagName() == QString("Description"))
                        cmd.Description = cmdE.text();
                }
                cmd.fn = CommandHandler;
                cmd.instance = this;

                level[QString("")].setValue(cmd);
            }
        }
    }

    return level;
}
