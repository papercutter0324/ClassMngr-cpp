#include "pagemanager.h"

#include "../pages/campus/campusdashboardpage.h"
#include "../pages/classinfo/classinfopage.h"
#include "../pages/roster/rosterpage.h"
#include "../pages/schedule/schedulepage.h"
#include "../pages/speaking/speakingevalpage.h"
#include "../pages/teacher/teacherinfopage.h"



    // =========================================================
    // Constructor
    // =========================================================

    PageManager::PageManager(
        QWidget *parent
        )
    : QStackedWidget(parent)
{
    m_schedulePage =
        new SchedulePage(this);

    m_classInfoPage =
        new ClassInfoPage(this);

    m_teacherPage =
        new TeacherInfoPage(this);

    m_campusDashboard =
        new CampusDashboardPage(this);

    m_rosterPage =
        new RosterPage(this);

    m_speakingPage =
        new SpeakingEvalPage(this);

    registerPages();
}



// =========================================================
// Register Pages
// =========================================================

void PageManager::registerPages()
{
    m_pages[PageType::Schedule] =
        m_schedulePage;

    m_pages[PageType::ClassInfo] =
        m_classInfoPage;

    m_pages[PageType::TeacherInfo] =
        m_teacherPage;

    m_pages[PageType::CampusDashboard] =
        m_campusDashboard;

    m_pages[PageType::Roster] =
        m_rosterPage;

    m_pages[PageType::SpeakingEval] =
        m_speakingPage;



    // =====================================================
    // Add To Stack
    // =====================================================

    for (BasePage *page : m_pages)
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
    for (BasePage *page : m_pages)
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

ClassInfoPage* PageManager::classInfoPage() const
{
    return m_classInfoPage;
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