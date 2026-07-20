#include "sidebar_controller_p.h"

void SidebarController::refreshClassSidebar()
{
    if (!m_sidebar)
    {
        return;
    }

    m_sidebar->clearClasses();

    auto* ds =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!ds || !ds->isOpen())
    {
        updateActionStates();
        return;
    }

    QList<SidebarClassNode> classNodes;

    for (const Classroom& classroom : ds->getClasses())
    {
        auto classInfo =
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
                );

        SidebarClassNode node;
        node.classId =
            classroom.id;
        node.classInfo =
            classInfo;
        node.displayName =
            displayName;
        node.teacherKr =
            teacher.teacherKr.trimmed().isEmpty()
                ? classInfo.teacherKr
                : teacher.teacherKr;

        classNodes.append(
            node
            );
    }

    std::sort(
        classNodes.begin(),
        classNodes.end(),
        sidebarClassNodeLessThan
        );

    for (const SidebarClassNode& node : std::as_const(classNodes))
    {
        m_sidebar->addClassNode(
            node.displayName,
            node.classId
            );
    }

    updateActionStates();
}

void SidebarController::refreshTeacherSidebar()
{
    if (!m_sidebar)
    {
        return;
    }

    m_sidebar->clearTeachers();

    auto* ds =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!ds || !ds->isOpen())
    {
        updateActionStates();
        return;
    }

    const QList<Teacher> teachers =
        ds->getAllTeachers();

    QHash<int, Teacher> teachersById;

    for (const Teacher& teacher : teachers)
    {
        if (teacher.id > 0)
        {
            teachersById.insert(
                teacher.id,
                teacher
                );
        }
    }

    QSet<int> myTeacherIds;
    QList<Teacher> myTeachers;

    for (const Classroom& classroom : ds->getClasses())
    {
        const ClassInfo classInfo =
            ds->loadClassInfo(
                classroom.id
                );

        const int teacherId =
            classInfo.teacherId;

        if (
            teacherId <= 0
            || myTeacherIds.contains(teacherId)
            || !teachersById.contains(teacherId)
            )
        {
            continue;
        }

        myTeacherIds.insert(
            teacherId
            );
        myTeachers.append(
            teachersById.value(teacherId)
            );
    }

    for (const Teacher& teacher : sortedTeachers(myTeachers))
    {
        const QString displayName =
            SidebarNodeNaming::formatTeacherDisplayName(
                teacher
                );

        m_sidebar->addTeacherNode(
            displayName,
            teacher.id,
            true
            );
    }

    for (const Teacher& teacher : sortedTeachers(teachers))
    {
        const QString displayName =
            SidebarNodeNaming::formatTeacherDisplayName(
                teacher
                );

        m_sidebar->addTeacherNode(
            displayName,
            teacher.id,
            false
            );
    }

    updateActionStates();
}

void SidebarController::refreshAllSidebars()
{
    refreshTeacherSidebar();
    refreshClassSidebar();
}

void SidebarController::handleClassInfoSaved(
    int classId
    )
{
    const QStringList selectedKeys =
        m_sidebar->selectedKeys();
    const int selectedClassId =
        m_sidebar->getSelectedClassId();

    refreshClassSidebar();

    m_sidebar->selectByKeys(
        selectedKeys,
        selectedClassId > 0
            ? selectedClassId
            : classId
        );
}

void SidebarController::handleTeacherSaved(
    int teacherId
    )
{
    refreshTeacherSidebar();
    refreshClassSidebar();

    m_sidebar->selectTeacher(
        teacherId
        );
}
