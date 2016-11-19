#ifndef GROUPDATA_H
#define GROUPDATA_H

#include <QUuid>
#include <QString>
#include <QHostAddress>

class GroupData
{
public:
    GroupData();

    QUuid Uuid;
    QString Name;
    QHostAddress Dns;
};

#endif // GROUPDATA_H
