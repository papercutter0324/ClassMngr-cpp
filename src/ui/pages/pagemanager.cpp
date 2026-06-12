#include "pagemanager.h"

#include "core/application_services.h"

#include "ui/pages/campus/campus_dashboard_page.h"
#include "ui/pages/class/class_info_page.h"
#include "ui/pages/class/class_notes_page.h"
#include "ui/pages/roster/roster_page.h"
#include "ui/pages/schedule/schedule_page.h"
#include "ui/pages/speakingeval/speaking_eval_page.h"
#include "ui/pages/subprep/sub_prep_page.h"
#include "ui/pages/teacher/teacher_info_page.h"



// =========================================================
// Constructor
// =========================================================

PageManager::PageManager(
    QWidget* parent
    )
    : QStackedWidget(parent)
{
}



// =========================================================
// Initialization
// =========================================================

void PageManager::initialize(
    ApplicationServices* services
    )
{
    if (m_initialized)
    {
        return;
    }

    m_initialized = true;
    m_services = services;

    m_schedulePage =
        new SchedulePage(
            m_services,
            this
            );

    m_subPrepPage =
        new SubPrepPage(
            this
            );

    m_classInfoPage =
        new ClassInfoPage(
            m_services,
            this
            );

    m_teacherPage =
        new TeacherInfoPage(
            m_services,
            this
            );

    m_campusDashboard =
        new CampusDashboardPage(this);

    m_rosterPage =
        new RosterPage(
            m_services,
            this
            );

    m_classNotesPage =
        new ClassNotesPage(
            m_services,
            this
            );

    m_speakingPage =
        new SpeakingEvalPage(
            m_services,
            this
            );

    registerPages();

    showPage(
        PageType::Schedule
        );
}



// =========================================================
// Register Pages
// =========================================================

void PageManager::registerPages()
{
    m_pages[PageType::Schedule] =
        m_schedulePage;

    m_pages[PageType::SubPrep] =
        m_subPrepPage;

    m_pages[PageType::ClassInfo] =
        m_classInfoPage;

    m_pages[PageType::TeacherInfo] =
        m_teacherPage;

    m_pages[PageType::CampusDashboard] =
        m_campusDashboard;

    m_pages[PageType::Roster] =
        m_rosterPage;

    m_pages[PageType::ClassNotes] =
        m_classNotesPage;

    m_pages[PageType::SpeakingEval] =
        m_speakingPage;

    for (BasePage* page : m_pages)
    {
        addWidget(page);
    }
}



// =========================================================
// Navigation
// =========================================================

void PageManager::showPage(
    PageType type
    )
{
    if (!m_pages.contains(type))
    {
        return;
    }

    setCurrentWidget(
        m_pages[type]
        );
}



// =========================================================
// Refresh All
// =========================================================

void PageManager::refreshAll()
{
    for (BasePage* page : m_pages)
    {
        if (page)
        {
            page->refresh();
        }
    }
}



// =========================================================
// Accessors
// =========================================================

SchedulePage* PageManager::schedulePage() const
{
    return m_schedulePage;
}

SubPrepPage* PageManager::subPrepPage() const
{
    return m_subPrepPage;
}

ClassInfoPage* PageManager::classInfoPage() const
{
    return m_classInfoPage;
}

ClassNotesPage* PageManager::classNotesPage() const
{
    return m_classNotesPage;
}

TeacherInfoPage* PageManager::teacherPage() const
{
    return m_teacherPage;
}

CampusDashboardPage* PageManager::campusDashboard() const
{
    return m_campusDashboard;
}

RosterPage* PageManager::rosterPage() const
{
    return m_rosterPage;
}

SpeakingEvalPage* PageManager::speakingPage() const
{
    return m_speakingPage;
}
