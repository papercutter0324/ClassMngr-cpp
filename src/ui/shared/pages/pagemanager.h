#ifndef PAGEMANAGER_H
#define PAGEMANAGER_H

#include "core/memory_usage_diagnostics.h"
#include "core/resource_packs/resource_pack_manager.h"

#include <QDateTime>
#include <QMap>
#include <QStackedWidget>

#include <functional>

#include "basepage.h"



// =========================================================
// Forward Declarations
// =========================================================

class ApplicationServices;

class SchedulePage;
class CalendarPage;
class MyWorkspacePage;
class PersonalDetailsPage;
class MyClassesPage;
class ClassesPage;
class TestingClassesPage;
class TeacherInfoPage;
class StaffDirectoryPage;
class CampusDashboardPage;
class SubPrepPage;
class PdfViewerPage;



// =========================================================
// Page Types
// =========================================================

enum class PageType
{
    MyWorkspace,
    MyClasses,
    Schedule,
    Classes,
    TestingClasses,
    TeacherInfo,
    NativeEnglishTeachers,
    GsTeam,
    CampusDashboard,
    SubPrep,
    PdfViewer
};



// =========================================================
// Page Manager
// =========================================================

class PageManager : public QStackedWidget,
                    public MemoryBreakdownProvider,
                    public PageLifecycleProvider
{
    Q_OBJECT

public:

    explicit PageManager(
        QWidget* parent = nullptr
    );



    // =====================================================
    // Initialization
    // =====================================================

    void initialize(
        ApplicationServices* services,
        bool adminMode
        );



    // =====================================================
    // Navigation
    // =====================================================

    void showPage(
        PageType type
        );

    [[nodiscard]] bool isPageInstantiated(
        PageType type
        ) const;

    [[nodiscard]] bool isCurrentPage(
        PageType type
        ) const;
    [[nodiscard]] int instantiatedPageCount() const;
    [[nodiscard]] int registeredPageCount() const;
    [[nodiscard]] QString currentPageIdentifier() const;
    [[nodiscard]] bool isDatabaseOpen() const;
    [[nodiscard]] static QString pageTypeIdentifier(
        PageType type
        );
    [[nodiscard]] QList<MemoryBreakdownEntry>
        memoryBreakdown() const override;
    [[nodiscard]] QList<PageLifecycleEntry>
        pageLifecycle() const override;

    bool confirmCurrentPageCanLeave(
        bool exiting = false
        );

    void setSaveMode(
        SaveMode mode
        );

    void setDocumentPageSpacing(
        DocumentPageSpacing spacing
        );

    void setDocumentViewerBackground(
        DocumentViewerBackground background
        );

    void setDatabaseOpen(
        bool databaseOpen
        );

    void clearDatabaseState();

    [[nodiscard]] PageOutputCapabilities
        outputCapabilities() const;

    void printCurrentPage();

    void saveCurrentPageAs();

    void refreshSchedulePreferences();

    void refreshNavigationPreferences();



signals:

    void pageCreated(
        PageType type,
        BasePage* page
        );

    void initialSetupRequested();

    void openDatabaseRequested();

    void newDatabaseRequested();

    void outputCapabilitiesChanged();


public:

    // =====================================================
    // Accessors
    // =====================================================

    SchedulePage* schedulePage() const;

    MyWorkspacePage* myWorkspacePage() const;
    MyWorkspacePage* ensureMyWorkspacePage();

    PersonalDetailsPage* personalDetailsPage() const;

    CalendarPage* calendarPage() const;
    CalendarPage* ensureCalendarPage();

    SchedulePage* mySchedulePage() const;

    MyClassesPage* myClassesPage() const;

    SubPrepPage* subPrepPage() const;

    ClassesPage* classesPage() const;
    ClassesPage* ensureClassesPage();
    TestingClassesPage* testingClassesPage() const;
    TestingClassesPage* ensureTestingClassesPage();

    TeacherInfoPage* teacherPage() const;
    TeacherInfoPage* ensureTeacherPage();

    StaffDirectoryPage* nativeEnglishTeachersPage() const;
    StaffDirectoryPage* ensureNativeEnglishTeachersPage();

    StaffDirectoryPage* gsTeamPage() const;
    StaffDirectoryPage* ensureGsTeamPage();

    CampusDashboardPage* campusDashboard() const;
    CampusDashboardPage* ensureCampusDashboard();

    PdfViewerPage* pdfViewerPage() const;
    PdfViewerPage* ensurePdfViewerPage();



    // =====================================================
    // Refresh
    // =====================================================

    void refreshAll();
    void retranslatePages();



private:

    // =====================================================
    // State
    // =====================================================

    bool m_initialized = false;
    bool m_adminMode = false;
    bool m_databaseStateSet = false;
    bool m_databaseOpen = false;

    SaveMode m_saveMode = SaveMode::Automatic;
    DocumentPageSpacing m_documentPageSpacing =
        DocumentPageSpacing::Small;
    DocumentViewerBackground m_documentViewerBackground =
        DocumentViewerBackground::Default;
    ResourcePackLease m_campusResourceLease;

    // =====================================================
    // Services
    // =====================================================

    ApplicationServices* m_services = nullptr;



    // =====================================================
    // Setup
    // =====================================================

    void registerPageFactories();

    BasePage* ensurePage(
        PageType type
        );

    void applyCurrentState(
        BasePage* page
        );

    void connectCommonPageSignals(
        BasePage* page
        );

    [[nodiscard]] Status preparePageResources(PageType type);
    [[nodiscard]] static bool usesCampusResources(PageType type);
    void releaseLeavingPageResources(
        BasePage* page,
        PageType nextType
        );



    // =====================================================
    // Pages
    // =====================================================

    SchedulePage* m_schedulePage = nullptr;

    MyWorkspacePage* m_myWorkspacePage = nullptr;

    MyClassesPage* m_myClassesPage = nullptr;

    SubPrepPage* m_subPrepPage = nullptr;

    ClassesPage* m_classesPage = nullptr;
    TestingClassesPage* m_testingClassesPage = nullptr;

    TeacherInfoPage* m_teacherPage = nullptr;

    StaffDirectoryPage* m_nativeEnglishTeachersPage = nullptr;

    StaffDirectoryPage* m_gsTeamPage = nullptr;

    CampusDashboardPage* m_campusDashboard = nullptr;

    PdfViewerPage* m_pdfViewerPage = nullptr;



    // =====================================================
    // Registry
    // =====================================================

    QMap<PageType, BasePage*> m_pages;
    QMap<PageType, std::function<BasePage*()>> m_pageFactories;
    QMap<PageType, QDateTime> m_pageCreatedAt;
    QMap<PageType, QDateTime> m_pageLastActivatedAt;
};



#endif // PAGEMANAGER_H
