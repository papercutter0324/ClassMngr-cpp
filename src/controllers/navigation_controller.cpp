#include "navigation_controller.h"

#include "models/teacher.h"

#include "services/dataservice.h"

#include "ui/pages/pagemanager.h"
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
    Q_UNUSED(data);
}

void NavigationController::handleMyInfo(
    const NavigationData& data
    )
{
    Q_UNUSED(data);
}

void NavigationController::handleCampus(
    const NavigationData& data
    )
{
    Q_UNUSED(data);
}

void NavigationController::handleClass(
    const NavigationData& data
    )
{
    Q_UNUSED(data);
}