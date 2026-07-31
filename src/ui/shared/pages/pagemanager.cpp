#include "pagemanager.h"

#include "core/application_services.h"

#include "features/campus/ui/campus_dashboard_page.h"
#include "features/classes/ui/classes_page.h"
#include "features/classes/ui/testing_classes_page.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/my_info/ui/my_classes_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/roster/ui/rosters_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "features/teacher/ui/staff_directory_page.h"
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

    m_personalDetailsPage =
        new PersonalDetailsPage(
            m_services,
            this
            );

    m_calendarPage =
        new CalendarPage(
            m_services,
            this
            );

    m_mySchedulePage =
        new SchedulePage(
            m_services,
            this
            );

    m_myClassesPage =
        new MyClassesPage(
            m_services,
            this
            );

    m_rostersPage =
        new RostersPage(
            m_services,
            this
            );

    m_subPrepPage =
        new SubPrepPage(
            m_services,
            this
            );

    m_classesPage =
        new ClassesPage(
            m_services,
            this
            );

    m_testingClassesPage =
        new TestingClassesPage(
            m_services,
            this
            );

    m_teacherPage =
        new TeacherInfoPage(
            m_services,
            this
            );

    m_nativeEnglishTeachersPage =
        new StaffDirectoryPage(
            m_services,
            StaffDirectoryKind::NativeEnglishTeachers,
            this
            );

    m_gsTeamPage =
        new StaffDirectoryPage(
            m_services,
            StaffDirectoryKind::GsTeam,
            this
            );

    m_campusDashboard =
        new CampusDashboardPage(
            m_adminMode,
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
        PageType::PersonalDetails
        );
}



// =========================================================
// Register Pages
// =========================================================

void PageManager::registerPages()
{
    m_pages[PageType::PersonalDetails] =
        m_personalDetailsPage;

    m_pages[PageType::Calendar] =
        m_calendarPage;

    m_pages[PageType::MySchedule] =
        m_mySchedulePage;

    m_pages[PageType::MyClasses] =
        m_myClassesPage;

    m_pages[PageType::Schedule] =
        m_schedulePage;

    m_pages[PageType::SubPrep] =
        m_subPrepPage;

    m_pages[PageType::Classes] =
        m_classesPage;

    m_pages[PageType::TestingClasses] =
        m_testingClassesPage;

    m_pages[PageType::TeacherInfo] =
        m_teacherPage;

    m_pages[PageType::NativeEnglishTeachers] =
        m_nativeEnglishTeachersPage;

    m_pages[PageType::GsTeam] =
        m_gsTeamPage;

    m_pages[PageType::CampusDashboard] =
        m_campusDashboard;

    m_pages[PageType::Rosters] =
        m_rostersPage;

    m_pages[PageType::SpeakingEval] =
        m_speakingPage;

    m_pages[PageType::PdfViewer] =
        m_pdfViewerPage;

    for (BasePage* page : m_pages)
    {
        addWidget(page);

        connect(
            page,
            &BasePage::openDatabaseRequested,
            this,
            &PageManager::openDatabaseRequested
            );

        connect(
            page,
            &BasePage::newDatabaseRequested,
            this,
            &PageManager::newDatabaseRequested
            );
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

void PageManager::setDatabaseOpen(
    bool databaseOpen
    )
{
    for (BasePage* page : m_pages)
    {
        if (page)
        {
            page->setDatabaseOpen(databaseOpen);
        }
    }
}

void PageManager::clearDatabaseState()
{
    for (BasePage* page : m_pages)
    {
        if (page)
        {
            page->clearDatabaseState();
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

void PageManager::retranslatePages()
{
    for (BasePage* page : m_pages)
    {
        if (page)
        {
            page->BasePage::retranslateUi();
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

PersonalDetailsPage* PageManager::personalDetailsPage() const
{
    return m_personalDetailsPage;
}

CalendarPage* PageManager::calendarPage() const
{
    return m_calendarPage;
}

SchedulePage* PageManager::mySchedulePage() const
{
    return m_mySchedulePage;
}

MyClassesPage* PageManager::myClassesPage() const
{
    return m_myClassesPage;
}

RostersPage* PageManager::rostersPage() const
{
    return m_rostersPage;
}

SubPrepPage* PageManager::subPrepPage() const
{
    return m_subPrepPage;
}

ClassesPage* PageManager::classesPage() const
{
    return m_classesPage;
}

TestingClassesPage* PageManager::testingClassesPage() const
{
    return m_testingClassesPage;
}

TeacherInfoPage* PageManager::teacherPage() const
{
    return m_teacherPage;
}

StaffDirectoryPage* PageManager::nativeEnglishTeachersPage() const
{
    return m_nativeEnglishTeachersPage;
}

StaffDirectoryPage* PageManager::gsTeamPage() const
{
    return m_gsTeamPage;
}

CampusDashboardPage* PageManager::campusDashboard() const
{
    return m_campusDashboard;
}

SpeakingEvalPage* PageManager::speakingPage() const
{
    return m_speakingPage;
}

PdfViewerPage* PageManager::pdfViewerPage() const
{
    return m_pdfViewerPage;
}
