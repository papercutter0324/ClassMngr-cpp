#include "sidebar_controller_p.h"

using namespace SidebarControllerPrivate;

int SidebarController::promptForClassToDelete() const
{
    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return -1;
    }

    QList<QPair<QString, int>> records;

    for (const Classroom& classroom : ds->getClasses())
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
    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return -1;
    }

    QList<QPair<QString, int>> records;

    for (const Teacher& teacher : ds->getAllTeachers())
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
    auto* ds =
        openDataService(m_services);

    if (!ds)
    {
        return classroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(classroom.id)
            : classroom.name.trimmed();
    }

    const ClassInfo classInfo =
        ds->loadClassInfo(
            classroom.id
            );

    Teacher teacher;

    if (classInfo.teacherId > 0)
    {
        teacher =
            ds->getTeacher(
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
    const auto result =
        QMessageBox::question(
            m_sidebar,
            tr("Delete Class"),
            tr("Delete '%1'?")
                .arg(classDisplayName(classroom))
            );

    return result == QMessageBox::Yes;
}

bool SidebarController::confirmDeleteTeacher(
    const Teacher& teacher
    ) const
{
    const QString displayName =
        SidebarNodeNaming::formatTeacherDisplayName(
            teacher
            );

    const auto result =
        QMessageBox::question(
            m_sidebar,
            tr("Delete Teacher"),
            tr("Delete '%1'?")
                .arg(displayName)
            );

    return result == QMessageBox::Yes;
}

void SidebarController::updateActionStates()
{
    if (!m_actions)
    {
        return;
    }

    auto* ds =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!ds || !ds->isOpen())
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
            !ds->getClasses().isEmpty()
            );
    }

    if (m_actions->deleteClass)
    {
        m_actions->deleteClass->setEnabled(
            !ds->getClasses().isEmpty()
            );
    }

    if (m_actions->deleteTeacher)
    {
        m_actions->deleteTeacher->setEnabled(
            !ds->getAllTeachers().isEmpty()
            );
    }
}
