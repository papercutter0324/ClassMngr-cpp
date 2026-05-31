#include "sidebar_node_naming.h"

#include "models/teacher.h"

#include <QMap>
#include <QStringList>



namespace
{
const QMap<QString, QString> DayAbbreviations =
    {
        {"Monday",    "Mon"},
        {"Tuesday",   "Tues"},
        {"Wednesday", "Wed"},
        {"Thursday",  "Thurs"},
        {"Friday",    "Fri"},
        {"Saturday",  "Sat"},
        {"Sunday",    "Sun"}
};
}



QString SidebarNodeNaming::formatTeacherDisplayName(
    const Teacher& teacher
    )
{
    const QString teacherEn =
        teacher.teacherEn.trimmed();

    const QString teacherKr =
        teacher.teacherKr.trimmed();

    if (!teacherEn.isEmpty() &&
        !teacherKr.isEmpty())
    {
        return QString("%1 (%2)")
            .arg(teacherEn)
            .arg(teacherKr);
    }

    if (!teacherKr.isEmpty())
    {
        return teacherKr;
    }

    if (!teacherEn.isEmpty())
    {
        return teacherEn;
    }

    return "New Teacher";
}