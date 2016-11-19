#ifndef CONNECTIONDATA_H
#define CONNECTIONDATA_H

#include <QString>
#include <QUuid>

class ConnectionData
{
public:
    ConnectionData();

    QUuid Uuid;
    QString Name;

    QUuid Group;

    QString Host;
    int Port;
    QString User;
    QString Pass;
};

#endif // CONNECTIONDATA_H
