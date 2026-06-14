#include "pagemanager.h"

#include "core/application_services.h"

#include "ui/pages/campus/campus_dashboard_page.h"
#include "ui/pages/class/class_info_page.h"
#include "ui/pages/class/class_notes_page.h"
#include "ui/pages/myinfo/my_info_page.h"
#include "ui/pages/roster/roster_page.h"
#include "ui/pages/schedule/schedule_page.h"
#include "ui/pages/speakingeval/speaking_eval_page.h"
#include "ui/pages/subprep/sub_prep_page.h"
#include "ui/pages/teacher/teacher_info_page.h"
#include "ui/utils/unsaved_changes_dialog.h"



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

    m_myInfoPage =
        new MyInfoPage(
            m_services,
            this
            );

    m_subPrepPage =
        new SubPrepPage(
            m_services,
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
        PageType::MyInfo
        );
}



// =========================================================
// Register Pages
// =========================================================

void PageManager::registerPages()
{
    m_pages[PageType::MyInfo] =
        m_myInfoPage;

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

bool PageManager::confirmCurrentPageCanLeave(
    bool exiting
    )
{
    Q_UNUSED(exiting);

    auto* page =
        qobject_cast<BasePage*>(
            currentWidget()
            );

    if (
        !page
        || !page->hasUnsavedChanges()
        )
    {
        return true;
    }

    const UnsavedChangesChoice choice =
        showUnsavedChangesDialog(
            this,
            page->unsavedChangesTitle(),
            page->unsavedChangesMessage()
            );

    switch (choice)
    {
    case UnsavedChangesChoice::Save:
        return page->saveChanges()
            && !page->hasUnsavedChanges();

    case UnsavedChangesChoice::Discard:
        page->discardChanges();
        return true;

    case UnsavedChangesChoice::Cancel:
        return false;
    }

    return false;
}

void PageManager::setSaveMode(
    SaveMode mode
    )
{
    for (BasePage* page : m_pages)
    {
        if (page)
        {
            page->setSaveMode(mode);
        }
    }
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

MyInfoPage* PageManager::myInfoPage() const
{
    return m_myInfoPage;
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
