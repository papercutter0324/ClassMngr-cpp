#include "sidebar_controller_p.h"

SidebarController::SidebarController(
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
        actions.showAllKoreanTeachers,
        &QAction::toggled,
        this,
        [this](bool)
        {
            refreshTeacherSidebar();
        }
        );

    connect(
        m_sidebar,
        &Sidebar::addClassRequested,
        this,
        &SidebarController::addClass
        );

    connect(
        m_sidebar,
        &Sidebar::deleteClassRequested,
        this,
        &SidebarController::deleteClass
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
