#include "pagemanager.h"

#include "core/application_services.h"
#include "core/memory_usage_diagnostics.h"
#include "core/resource_paths.h"

#include "features/campus/ui/campus_dashboard_page.h"
#include "features/classes/ui/classes_page.h"
#include "features/classes/ui/testing_classes_page.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/my_info/ui/my_classes_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "ui/shared/pages/pdf_viewer_page.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QElapsedTimer>
#include <QDebug>

#include <utility>

namespace
{
QString pageTypeIdentifierForWidget(
    const QMap<PageType, BasePage*>& pages,
    const BasePage* page
    )
{
    for (auto iterator = pages.cbegin(); iterator != pages.cend(); ++iterator)
    {
        if (iterator.value() == page)
        {
            return PageManager::pageTypeIdentifier(iterator.key());
        }
    }

    return QStringLiteral("unknown-page");
}
}



// =========================================================
// Constructor
// =========================================================

PageManager::PageManager(
    QWidget* parent
    )
    : QStackedWidget(parent)
{
    MemoryUsageDiagnostics::registerMemoryBreakdownProvider(this, this);
    MemoryUsageDiagnostics::registerPageLifecycleProvider(this, this);
}

QString PageManager::pageTypeIdentifier(PageType type)
{
    switch (type)
    {
    case PageType::PersonalDetails: return QStringLiteral("personal-details");
    case PageType::Calendar: return QStringLiteral("calendar");
    case PageType::MySchedule: return QStringLiteral("my-schedule");
    case PageType::MyClasses: return QStringLiteral("my-classes");
    case PageType::Schedule: return QStringLiteral("schedule");
    case PageType::Classes: return QStringLiteral("classes");
    case PageType::TestingClasses: return QStringLiteral("testing-classes");
    case PageType::TeacherInfo: return QStringLiteral("teacher-info");
    case PageType::NativeEnglishTeachers: return QStringLiteral("native-english-teachers");
    case PageType::GsTeam: return QStringLiteral("gs-team");
    case PageType::CampusDashboard: return QStringLiteral("campus-dashboard");
    case PageType::SubPrep: return QStringLiteral("sub-prep");
    case PageType::PdfViewer: return QStringLiteral("pdf-viewer");
    }

    return QStringLiteral("unknown-page");
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

    registerPageFactories();

    // Keep the lightweight/default pages eager for now. The more expensive
    // Calendar, Classes, Campus, and PDF pages are created only when needed.
    ensurePage(PageType::PersonalDetails);
    ensurePage(PageType::MySchedule);
    ensurePage(PageType::MyClasses);
    ensurePage(PageType::Schedule);
    ensurePage(PageType::SubPrep);
    ensurePage(PageType::TestingClasses);
    ensurePage(PageType::TeacherInfo);
    ensurePage(PageType::NativeEnglishTeachers);
    ensurePage(PageType::GsTeam);

    showPage(
        PageType::PersonalDetails
        );
}



// =========================================================
// Register Page Factories
// =========================================================

void PageManager::registerPageFactories()
{
    m_pageFactories[PageType::PersonalDetails] =
        [this]() -> BasePage*
        {
            m_personalDetailsPage =
                new PersonalDetailsPage(m_services, this);
            return m_personalDetailsPage;
        };

    m_pageFactories[PageType::Calendar] =
        [this]() -> BasePage*
        {
            m_calendarPage = new CalendarPage(m_services, this);
            return m_calendarPage;
        };

    m_pageFactories[PageType::MySchedule] =
        [this]() -> BasePage*
        {
            m_mySchedulePage = new SchedulePage(m_services, this);
            return m_mySchedulePage;
        };

    m_pageFactories[PageType::MyClasses] =
        [this]() -> BasePage*
        {
            m_myClassesPage = new MyClassesPage(m_services, this);
            return m_myClassesPage;
        };

    m_pageFactories[PageType::Schedule] =
        [this]() -> BasePage*
        {
            m_schedulePage = new SchedulePage(m_services, this);
            return m_schedulePage;
        };

    m_pageFactories[PageType::SubPrep] =
        [this]() -> BasePage*
        {
            m_subPrepPage = new SubPrepPage(m_services, this);
            return m_subPrepPage;
        };

    m_pageFactories[PageType::Classes] =
        [this]() -> BasePage*
        {
            m_classesPage = new ClassesPage(m_services, this);
            return m_classesPage;
        };

    m_pageFactories[PageType::TestingClasses] =
        [this]() -> BasePage*
        {
            m_testingClassesPage = new TestingClassesPage(m_services, this);
            return m_testingClassesPage;
        };

    m_pageFactories[PageType::TeacherInfo] =
        [this]() -> BasePage*
        {
            m_teacherPage = new TeacherInfoPage(m_services, this);
            return m_teacherPage;
        };

    m_pageFactories[PageType::NativeEnglishTeachers] =
        [this]() -> BasePage*
        {
            m_nativeEnglishTeachersPage =
                new StaffDirectoryPage(
                    m_services,
                    StaffDirectoryKind::NativeEnglishTeachers,
                    this
                    );
            return m_nativeEnglishTeachersPage;
        };

    m_pageFactories[PageType::GsTeam] =
        [this]() -> BasePage*
        {
            m_gsTeamPage =
                new StaffDirectoryPage(
                    m_services,
                    StaffDirectoryKind::GsTeam,
                    this
                    );
            return m_gsTeamPage;
        };

    m_pageFactories[PageType::CampusDashboard] =
        [this]() -> BasePage*
        {
            m_campusDashboard = new CampusDashboardPage(m_adminMode, this);
            return m_campusDashboard;
        };

    m_pageFactories[PageType::PdfViewer] =
        [this]() -> BasePage*
        {
            m_pdfViewerPage = new PdfViewerPage(this);
            return m_pdfViewerPage;
        };
}

BasePage* PageManager::ensurePage(
    PageType type
    )
{
    if (!m_initialized)
    {
        return nullptr;
    }

    if (const Status status = preparePageResources(type); !status)
    {
        qWarning().noquote() << status.error();
        return nullptr;
    }

    if (const auto page = m_pages.constFind(type); page != m_pages.cend())
    {
        return page.value();
    }

    const auto factory = m_pageFactories.constFind(type);

    if (factory == m_pageFactories.cend() || !factory.value())
    {
        return nullptr;
    }

    const bool recordTiming = MemoryUsageDiagnostics::isEnabled();
    QElapsedTimer constructionTimer;
    if (recordTiming)
    {
        constructionTimer.start();
    }

    BasePage* page = factory.value()();

    if (!page)
    {
        return nullptr;
    }

    if (const Status status = page->prepareForActivation(); !status)
    {
        qWarning().noquote() << status.error();
        return nullptr;
    }

    m_pages.insert(type, page);
    m_pageCreatedAt.insert(type, QDateTime::currentDateTime());
    addWidget(page);
    connectCommonPageSignals(page);
    applyCurrentState(page);

    emit pageCreated(type, page);
    MemoryUsageDiagnostics::recordEvent(
        QStringLiteral("page-instantiated"),
        pageTypeIdentifier(type)
        );
    if (recordTiming)
    {
        MemoryUsageDiagnostics::recordTimedOperation(
            QStringLiteral("page-construction"),
            pageTypeIdentifier(type),
            constructionTimer.elapsed()
            );
    }

    return page;
}

void PageManager::applyCurrentState(
    BasePage* page
    )
{
    if (!page)
    {
        return;
    }

    page->setSaveMode(m_saveMode);

    if (m_databaseStateSet)
    {
        page->setDatabaseOpen(m_databaseOpen);

        if (page == m_classesPage)
        {
            m_classesPage->setEmbeddedDatabaseOpen(m_databaseOpen);
        }
    }

    if (page == m_pdfViewerPage)
    {
        m_pdfViewerPage->setDocumentPageSpacing(m_documentPageSpacing);
        m_pdfViewerPage->setDocumentViewerBackground(
            m_documentViewerBackground
            );
    }
}

void PageManager::connectCommonPageSignals(
    BasePage* page
    )
{
    connect(
        page,
        &BasePage::initialSetupRequested,
        this,
        &PageManager::initialSetupRequested
        );

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

    connect(
        page,
        &BasePage::outputCapabilitiesChanged,
        this,
        [this, page]()
        {
            if (page == currentWidget())
            {
                emit outputCapabilitiesChanged();
            }
        }
        );
}



// =========================================================
// Navigation
// =========================================================

void PageManager::showPage(
    PageType type
    )
{
    const bool recordTiming = MemoryUsageDiagnostics::isEnabled();
    QElapsedTimer activationTimer;
    if (recordTiming)
    {
        activationTimer.start();
    }

    const bool wasInstantiated = m_pages.contains(type);
    BasePage* page = ensurePage(type);

    if (!page)
    {
        return;
    }

    // Newly-created pages are prepared by ensurePage(). Existing pages need
    // their decoded feature assets rebuilt after a prior deactivation.
    if (wasInstantiated)
    {
        if (const Status status = page->prepareForActivation(); !status)
        {
            qWarning().noquote() << status.error();
            return;
        }
    }

    BasePage* leavingPage = qobject_cast<BasePage*>(currentWidget());

    const bool pageChanged = page != leavingPage;
    const bool firstActivation =
        !m_pageLastActivatedAt.contains(type);

    if (pageChanged)
    {
        if (leavingPage)
        {
            MemoryUsageDiagnostics::recordEvent(
                QStringLiteral("page-hidden"),
                pageTypeIdentifierForWidget(m_pages, leavingPage)
                );
        }

        releaseLeavingPageResources(leavingPage, type);
    }

    setCurrentWidget(
        page
        );

    if (pageChanged || firstActivation)
    {
        m_pageLastActivatedAt.insert(type, QDateTime::currentDateTime());
        MemoryUsageDiagnostics::recordEvent(
            QStringLiteral("page-shown"),
            pageTypeIdentifier(type)
            );
    }

    emit outputCapabilitiesChanged();

    if (recordTiming && (pageChanged || firstActivation))
    {
        MemoryUsageDiagnostics::recordTimedOperation(
            QStringLiteral("page-activation"),
            pageTypeIdentifier(type),
            activationTimer.elapsed()
            );
    }
}

QString PageManager::currentPageIdentifier() const
{
    const auto* page = qobject_cast<const BasePage*>(currentWidget());
    return page
        ? pageTypeIdentifierForWidget(m_pages, page)
        : QStringLiteral("none");
}

bool PageManager::isDatabaseOpen() const
{
    return m_databaseStateSet && m_databaseOpen;
}

QList<MemoryBreakdownEntry> PageManager::memoryBreakdown() const
{
    return {
        {
            QStringLiteral("Top-level page registry"),
            QStringLiteral("Page Manager"),
            0,
            static_cast<quint64>(m_pages.size()),
            QStringLiteral("instantiated=%1; registered=%2")
                .arg(m_pages.size())
                .arg(m_pageFactories.size()),
            true
        }
    };
}

QList<PageLifecycleEntry> PageManager::pageLifecycle() const
{
    QList<PageLifecycleEntry> entries;
    entries.reserve(m_pageFactories.size());

    for (auto iterator = m_pageFactories.cbegin(); iterator != m_pageFactories.cend(); ++iterator)
    {
        const auto page = m_pages.constFind(iterator.key());
        const PageLifecycleState state = page == m_pages.cend()
            ? PageLifecycleState::Uncreated
            : (page.value() == currentWidget()
                   ? PageLifecycleState::Current
                   : PageLifecycleState::Hidden);
        entries.append(
            {
                pageTypeIdentifier(iterator.key()),
                state,
                m_pageCreatedAt.value(iterator.key()),
                m_pageLastActivatedAt.value(iterator.key())
            }
            );
    }

    return entries;
}

Status PageManager::preparePageResources(
    PageType type
    )
{
    if (!usesCampusResources(type) || m_campusResourceLease.isValid())
    {
        return {};
    }

    auto lease = ResourcePaths::Campuses::acquire();
    if (!lease)
    {
        return std::unexpected(lease.error());
    }

    m_campusResourceLease = std::move(*lease);
    return {};
}

bool PageManager::usesCampusResources(
    PageType type
    )
{
    return type == PageType::PersonalDetails
        || type == PageType::Calendar
        || type == PageType::CampusDashboard
        || type == PageType::SubPrep;
}

void PageManager::releaseLeavingPageResources(
    BasePage* page,
    PageType nextType
    )
{
    if (page)
    {
        page->releaseFeatureResources();
    }

    const auto pdfPage =
        m_pages.constFind(PageType::PdfViewer);

    if (
        pdfPage == m_pages.cend()
        || pdfPage.value() != page
        )
    {
        if (!usesCampusResources(nextType))
        {
            m_campusResourceLease.reset();
        }
        return;
    }

    if (auto* viewer = qobject_cast<PdfViewerPage*>(page))
    {
        viewer->releaseDocument();
    }

    if (!usesCampusResources(nextType))
    {
        m_campusResourceLease.reset();
    }
}

bool PageManager::isPageInstantiated(
    PageType type
    ) const
{
    return m_pages.contains(type);
}

bool PageManager::isCurrentPage(
    PageType type
    ) const
{
    const auto page = m_pages.constFind(type);

    return page != m_pages.cend()
        && page.value() == currentWidget();
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
        DialogServices::prompts().confirmUnsavedChanges(
            UnsavedChangesRequest{
                .parent = this,
                .title = page->unsavedChangesTitle(),
                .message = page->unsavedChangesMessage()
            }
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
    m_saveMode = mode;

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
    m_documentPageSpacing = spacing;

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
    m_documentViewerBackground = background;

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
    m_databaseStateSet = true;
    m_databaseOpen = databaseOpen;

    for (BasePage* page : m_pages)
    {
        if (page)
        {
            page->setDatabaseOpen(databaseOpen);
        }
    }

    if (m_classesPage)
    {
        m_classesPage->setEmbeddedDatabaseOpen(databaseOpen);
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

PageOutputCapabilities PageManager::outputCapabilities() const
{
    auto* page =
        qobject_cast<BasePage*>(currentWidget());

    return page
        ? page->outputCapabilities()
        : PageOutputCapabilities{};
}

void PageManager::printCurrentPage()
{
    auto* page =
        qobject_cast<BasePage*>(currentWidget());

    if (page && page->outputCapabilities().printEnabled)
    {
        page->printCurrentPage();
    }
}

void PageManager::saveCurrentPageAs()
{
    auto* page =
        qobject_cast<BasePage*>(currentWidget());

    if (page && page->outputCapabilities().saveAsEnabled)
    {
        page->saveCurrentPageAs();
    }
}

void PageManager::refreshSchedulePreferences()
{
    if (m_schedulePage)
    {
        m_schedulePage->refreshSchedulePreferences();
    }

    if (m_mySchedulePage)
    {
        m_mySchedulePage->refreshSchedulePreferences();
    }
}

void PageManager::refreshNavigationPreferences()
{
    if (m_classesPage)
    {
        m_classesPage->refreshNavigationPreferences();
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

CalendarPage* PageManager::ensureCalendarPage()
{
    return qobject_cast<CalendarPage*>(
        ensurePage(PageType::Calendar)
        );
}

SchedulePage* PageManager::mySchedulePage() const
{
    return m_mySchedulePage;
}

MyClassesPage* PageManager::myClassesPage() const
{
    return m_myClassesPage;
}

SubPrepPage* PageManager::subPrepPage() const
{
    return m_subPrepPage;
}

ClassesPage* PageManager::classesPage() const
{
    return m_classesPage;
}

ClassesPage* PageManager::ensureClassesPage()
{
    return qobject_cast<ClassesPage*>(
        ensurePage(PageType::Classes)
        );
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

CampusDashboardPage* PageManager::ensureCampusDashboard()
{
    return qobject_cast<CampusDashboardPage*>(
        ensurePage(PageType::CampusDashboard)
        );
}

PdfViewerPage* PageManager::pdfViewerPage() const
{
    return m_pdfViewerPage;
}

PdfViewerPage* PageManager::ensurePdfViewerPage()
{
    return qobject_cast<PdfViewerPage*>(
        ensurePage(PageType::PdfViewer)
        );
}
