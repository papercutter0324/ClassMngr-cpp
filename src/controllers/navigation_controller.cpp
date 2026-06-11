#include "navigation_controller.h"

#include "models/classroom.h"
#include "models/teacher.h"

#include "services/dataservice.h"

#include "ui/pages/class/class_info_page.h"
#include "ui/pages/pagemanager.h"
#include "ui/pages/roster/roster_page.h"
#include "ui/pages/teacher/teacher_info_page.h"

NavigationController::NavigationController(
    ApplicationServices* services,
    Sidebar* sidebar,
    PageManager* pages,
    QObject* parent
    )
    : QObject(parent)
    , m_services(services)
    , m_sidebar(sidebar)
    , m_pages(pages)
{
}

void NavigationController::handleTeacher(
    const NavigationData& data
    )
{
    Teacher teacher =
        m_services
            ->dataService()
            ->getTeacher(
                data.teacherId
                );

    if (teacher.id < 0)
    {
        return;
    }

    m_pages->teacherPage()
        ->loadTeacher(
            teacher
            );

    m_pages->showPage(
        PageType::TeacherInfo
        );
}

void NavigationController::handleNavigation(
    const NavigationData& data
    )
{
    switch (data.type)
    {
    case NodeType::Teacher:
        handleTeacher(data);
        return;

    case NodeType::Class:
        handleClass(data);
        return;

    case NodeType::Page:
        if (data.path.isEmpty())
        {
            return;
        }

        if (data.path.first() == tr("My Info"))
        {
            handleMyInfo(data);
            return;
        }

        if (data.path.first() == tr("Campus Info"))
        {
            handleCampus(data);
            return;
        }

        if (data.classId > 0)
        {
            const QString pageName =
                data.path.last();

            if (pageName == tr("Class Info"))
            {
                handleClass(data);
                return;
            }

            if (pageName == tr("Class Roster"))
            {
                Classroom classroom =
                    m_services
                        ->dataService()
                        ->getClassById(data.classId);

                if (classroom.id < 0)
                {
                    return;
                }

                m_pages->rosterPage()
                    ->loadClass(classroom);

                m_pages->showPage(PageType::Roster);
                return;
            }

            m_pages->showPage(PageType::SpeakingEval);
            return;
        }

        return;

    default:
        return;
    }
}

void NavigationController::handleMyInfo(
    const NavigationData& data
    )
{
    Q_UNUSED(data);

    m_pages->showPage(PageType::Schedule);
}

void NavigationController::handleCampus(
    const NavigationData& data
    )
{
    Q_UNUSED(data);

    m_pages->showPage(PageType::CampusDashboard);
}

void NavigationController::handleClass(
    const NavigationData& data
    )
{
    if (data.classId <= 0)
    {
        return;
    }

    Classroom classroom =
        m_services
            ->dataService()
            ->getClassById(data.classId);

    if (classroom.id < 0)
    {
        return;
    }

    m_pages->classInfoPage()
        ->loadClass(classroom);

    m_pages->showPage(PageType::ClassInfo);
}
