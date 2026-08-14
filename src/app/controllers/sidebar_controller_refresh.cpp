#include "sidebar_controller_p.h"

#include "app/services/feature_services.h"
#include "ui/shared/dialogs/user_prompt_service.h"

using namespace SidebarControllerPrivate;

void SidebarController::refreshClassSidebar()
{
    if (!m_sidebar)
    {
        return;
    }

    m_sidebar->clearClasses();

    auto* classes =
        openClassService(m_services);
    auto* teachers =
        openTeacherService(m_services);

    if (!classes || !teachers)
    {
        updateActionStates();
        return;
    }

    QList<SidebarClassNode> classNodes;
    const Result<QList<Classroom>> loadedClasses = classes->classes();
    if (!loadedClasses)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Load Classes"),
            tr("Classes could not be loaded."),
            loadedClasses.error()
            );
        updateActionStates();
        return;
    }

    for (const Classroom& classroom : *loadedClasses)
    {
        auto classInfo =
            classes->classInfo(
                classroom.id
                );

        Teacher teacher;

        if (classInfo.teacherId > 0)
        {
            const Result<Teacher> loadedTeacher =
                teachers->teacher(classInfo.teacherId);
            if (!loadedTeacher)
            {
                DialogServices::showWarning(
                    m_sidebar,
                    tr("Load Classes"),
                    tr("A class teacher could not be loaded."),
                    loadedTeacher.error()
                    );
                updateActionStates();
                return;
            }
            teacher = *loadedTeacher;
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

    auto* classes =
        openClassService(m_services);
    auto* teacherService =
        openTeacherService(m_services);

    if (!classes || !teacherService)
    {
        updateActionStates();
        return;
    }

    const Result<QList<Teacher>> teachers =
        teacherService->teachers();
    const Result<QList<Classroom>> loadedClasses = classes->classes();
    if (!teachers || !loadedClasses)
    {
        DialogServices::showWarning(
            m_sidebar,
            tr("Load Teachers"),
            tr("Teachers and their classes could not be loaded."),
            !teachers ? teachers.error() : loadedClasses.error()
            );
        updateActionStates();
        return;
    }

    QHash<int, Teacher> teachersById;

    for (const Teacher& teacher : *teachers)
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

    for (const Classroom& classroom : *loadedClasses)
    {
        const ClassInfo classInfo =
            classes->classInfo(
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

    for (const Teacher& teacher : sortedTeachers(*teachers))
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
