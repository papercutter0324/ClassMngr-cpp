#ifndef SIDEBAR_TYPES_H
#define SIDEBAR_TYPES_H

#include <QString>
#include <QStringList>
#include <QMetaType>

enum class NodeType
{
    Root,
    Page,
    Url,
    Teacher
};

struct NavigationData
{
    QStringList path;
    QStringList keys;
    QString routeKey;

    NodeType type;

    QString url;

    int classId = -1;

    int teacherId = -1;
};

Q_DECLARE_METATYPE(NavigationData)

#endif
