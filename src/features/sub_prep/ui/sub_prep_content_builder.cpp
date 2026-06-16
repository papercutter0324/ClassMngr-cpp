#include "sub_prep_content_builder.h"

#include "data/data_service.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "core/utils/sidebar_node_naming.h"

#include <QObject>

namespace SubPrepContentBuilder
{
QString timeFillerActivitiesHtml(
    DataService* dataService
    )
{
    if (
        !dataService
        || !dataService->isOpen()
        )
    {
        return QString();
    }

    QString html;

    const QList<Classroom> classes =
        dataService->getClasses();

    for (const Classroom& classroom : classes)
    {
        const ClassInfo info =
            dataService->loadClassInfo(
                classroom.id
                );

        const QString activities =
            info.timeFillerActivities.trimmed();

        if (activities.isEmpty())
        {
            continue;
        }

        Teacher teacher;

        if (info.teacherId > 0)
        {
            teacher =
                dataService->getTeacher(
                    info.teacherId
                    );
        }

        QString displayName =
            SidebarNodeNaming::formatClassDisplayName(
                info,
                teacher
                )
                .trimmed();

        if (displayName.isEmpty())
        {
            displayName =
                classroom.name.trimmed().isEmpty()
                    ? QObject::tr("Class %1").arg(classroom.id)
                    : classroom.name.trimmed();
        }

        if (!html.isEmpty())
        {
            html.append(QStringLiteral("<br><br>"));
        }

        html.append(
            QStringLiteral(
                "<div><b><u>%1</u></b></div>"
                "<div>%2</div>"
                )
                .arg(
                    displayName.toHtmlEscaped(),
                    activities
                        .toHtmlEscaped()
                        .replace(
                            QLatin1Char('\n'),
                            QStringLiteral("<br>")
                            )
                    )
            );
    }

    return html;
}
}
