#include "sidebar_controller_p.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/services/feature_services.h"

using namespace SidebarControllerPrivate;

int SidebarController::promptForClassToDelete() const
{
    auto* classes =
        openClassService(m_services);

    if (!classes)
    {
        return -1;
    }

    QList<QPair<QString, int>> records;

    for (const Classroom& classroom : classes->classes())
    {
        if (classroom.id <= 0)
        {
            continue;
        }

        records.append(
            {
                classDisplayName(classroom),
                classroom.id
            }
            );
    }

    if (records.isEmpty())
    {
        return -1;
    }

    return chooseRecord(
        m_sidebar,
        tr("Delete Class"),
        tr("Which class would you like to delete?"),
        records
        );
}

int SidebarController::promptForTeacherToDelete() const
{
    auto* teachers =
        openTeacherService(m_services);

    if (!teachers)
    {
        return -1;
    }

    QList<QPair<QString, int>> records;

    for (const Teacher& teacher : teachers->teachers())
    {
        if (teacher.id <= 0)
        {
            continue;
        }

        records.append(
            {
                SidebarNodeNaming::formatTeacherDisplayName(
                    teacher
                    ),
                teacher.id
            }
            );
    }

    if (records.isEmpty())
    {
        return -1;
    }

    return chooseRecord(
        m_sidebar,
        tr("Delete Teacher"),
        tr("Which teacher would you like to delete?"),
        records
        );
}

QString SidebarController::classDisplayName(
    const Classroom& classroom
    ) const
{
    auto* classes =
        openClassService(m_services);
    auto* teachers =
        openTeacherService(m_services);

    if (!classes || !teachers)
    {
        return classroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(classroom.id)
            : classroom.name.trimmed();
    }

    const ClassInfo classInfo =
        classes->classInfo(
            classroom.id
            );

    Teacher teacher;

    if (classInfo.teacherId > 0)
    {
        teacher =
            teachers->teacher(
                classInfo.teacherId
                );
    }

    QString displayName =
        SidebarNodeNaming::formatClassDisplayName(
            classInfo,
            teacher
            )
            .trimmed();

    if (displayName.isEmpty())
    {
        displayName =
            classroom.name.trimmed();
    }

    if (displayName.isEmpty())
    {
        displayName =
            tr("Class %1")
                .arg(classroom.id);
    }

    return displayName;
}

bool SidebarController::confirmDeleteClass(
    const Classroom& classroom
    ) const
{
    return DialogServices::confirm(
        m_sidebar,
        tr("Delete Class"),
        tr("Delete '%1'?")
            .arg(classDisplayName(classroom)),
        tr("Delete"),
        tr("Cancel"),
        true
        ) == PromptChoice::Destructive;
}

bool SidebarController::confirmDeleteTeacher(
    const Teacher& teacher
    ) const
{
    const QString displayName =
        SidebarNodeNaming::formatTeacherDisplayName(
            teacher
            );

    return DialogServices::confirm(
        m_sidebar,
        tr("Delete Teacher"),
        tr("Delete '%1'?")
            .arg(displayName),
        tr("Delete"),
        tr("Cancel"),
        true
        ) == PromptChoice::Destructive;
}

void SidebarController::updateActionStates()
{
    if (!m_actions)
    {
        return;
    }

    auto* classes =
        openClassService(m_services);
    auto* teachers =
        openTeacherService(m_services);

    if (!classes || !teachers)
    {
        if (m_actions->importClasses)
        {
            m_actions->importClasses->setEnabled(false);
        }

        if (m_actions->exportClasses)
        {
            m_actions->exportClasses->setEnabled(false);
        }

        if (m_actions->deleteClass)
        {
            m_actions->deleteClass->setEnabled(false);
        }

        if (m_actions->deleteTeacher)
        {
            m_actions->deleteTeacher->setEnabled(false);
        }

        if (m_actions->importTeachers)
        {
            m_actions->importTeachers->setEnabled(false);
        }

        return;
    }

    if (m_actions->importClasses)
    {
        m_actions->importClasses->setEnabled(true);
    }

    if (m_actions->importTeachers)
    {
        m_actions->importTeachers->setEnabled(true);
    }

    if (m_actions->exportClasses)
    {
        m_actions->exportClasses->setEnabled(
            !classes->classes().isEmpty()
            );
    }

    if (m_actions->deleteClass)
    {
        m_actions->deleteClass->setEnabled(
            !classes->classes().isEmpty()
            );
    }

    if (m_actions->deleteTeacher)
    {
        m_actions->deleteTeacher->setEnabled(
            !teachers->teachers().isEmpty()
            );
    }
}
