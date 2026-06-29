#include "pagemanager.h"

#include "core/application_services.h"

#include "features/campus/ui/campus_dashboard_page.h"
#include "features/classes/ui/class_info_page.h"
#include "features/classes/ui/class_notes_page.h"
#include "features/my_info/ui/my_info_page.h"
#include "features/roster/ui/roster_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "ui/shared/pages/pdf_viewer_page.h"
#include "ui/shared/utils/unsaved_changes_dialog.h"



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
    ApplicationServices* services,
    bool adminMode
    )
{
    if (m_initialized)
    {
        return;
    }

    m_initialized = true;
    m_services = services;
    m_adminMode = adminMode;

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
        new CampusDashboardPage(
            m_adminMode,
            this
            );

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

    m_pdfViewerPage =
        new PdfViewerPage(
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

    m_pages[PageType::PdfViewer] =
        m_pdfViewerPage;

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

void PageManager::setDocumentPageSpacing(
    DocumentPageSpacing spacing
    )
{
    if (m_pdfViewerPage)
    {
        m_pdfViewerPage->setDocumentPageSpacing(
            spacing
            );
    }
}

void PageManager::setDocumentViewerBackground(
    DocumentViewerBackground background
    )
{
    if (m_pdfViewerPage)
    {
        m_pdfViewerPage->setDocumentViewerBackground(
            background
            );
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

void PageManager::retranslatePages()
{
    for (BasePage* page : m_pages)
    {
        if (page)
        {
            page->retranslateUi();
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

PdfViewerPage* PageManager::pdfViewerPage() const
{
    return m_pdfViewerPage;
}
