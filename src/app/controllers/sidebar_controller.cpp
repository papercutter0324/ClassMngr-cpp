#include "sidebar_controller_p.h"

#include "next/platform/settings_manager_upcoming_birthday_dismissal_port.h"

#include <memory>

using namespace SidebarControllerPrivate;

SidebarController::SidebarController(
    ApplicationServices* services,
    Sidebar* sidebar,
    PageManager* pages,
    QObject* parent,
    ClassMngr::Next::Application::UpcomingBirthdayDismissalPort*
        upcomingBirthdayDismissalPort
    )
    : QObject(parent)
    , m_services(services)
    , m_sidebar(sidebar)
    , m_pages(pages)
    , m_upcomingBirthdayDismissalPort(upcomingBirthdayDismissalPort)
{
    if (!m_upcomingBirthdayDismissalPort)
    {
        m_ownedUpcomingBirthdayDismissalPort =
            std::make_unique<
                ClassMngr::Next::Platform::
                    SettingsManagerUpcomingBirthdayDismissalPort
                >();
        m_upcomingBirthdayDismissalPort =
            m_ownedUpcomingBirthdayDismissalPort.get();
    }
}

SidebarController::~SidebarController() = default;

void SidebarController::connectActions(ActionRegistry& actions)
{
    m_actions =
        &actions;

    connect(
        actions.newClass,
        &QAction::triggered,
        this,
        &SidebarController::addClass
        );

    connect(
        actions.deleteClass,
        &QAction::triggered,
        this,
        &SidebarController::deleteClass
        );

    connect(
        actions.importClasses,
        &QAction::triggered,
        this,
        &SidebarController::importClasses
        );

    connect(
        actions.exportClasses,
        &QAction::triggered,
        this,
        &SidebarController::exportClasses
        );

    connect(
        actions.newTeacher,
        &QAction::triggered,
        this,
        &SidebarController::addTeacher
        );

    connect(
        actions.deleteTeacher,
        &QAction::triggered,
        this,
        &SidebarController::deleteTeacher
        );

    connect(
        actions.upcomingBirthdays,
        &QAction::triggered,
        this,
        &SidebarController::showUpcomingBirthdays
        );

    connect(
        actions.importTeachers,
        &QAction::triggered,
        this,
        &SidebarController::importTeachers
        );

    connect(
        m_sidebar,
        &Sidebar::addClassRequested,
        this,
        &SidebarController::addClass
        );

    connect(
        m_sidebar,
        &Sidebar::addTeacherRequested,
        this,
        &SidebarController::addTeacher
        );

    connect(
        m_sidebar,
        &Sidebar::deleteTeacherRequested,
        this,
        &SidebarController::deleteTeacher
        );

    updateActionStates();
}
