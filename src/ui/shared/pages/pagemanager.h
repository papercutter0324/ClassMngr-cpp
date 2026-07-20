#ifndef PAGEMANAGER_H
#define PAGEMANAGER_H

#include <QMap>
#include <QStackedWidget>

#include "basepage.h"



// =========================================================
// Forward Declarations
// =========================================================

class ApplicationServices;

class SchedulePage;
class CalendarPage;
class PersonalDetailsPage;
class MyClassesPage;
class RostersPage;
class ClassesPage;
class TeacherInfoPage;
class StaffDirectoryPage;
class CampusDashboardPage;
class SpeakingEvalPage;
class SubPrepPage;
class PdfViewerPage;



// =========================================================
// Page Types
// =========================================================

enum class PageType
{
    PersonalDetails,
    Calendar,
    MySchedule,
    MyClasses,
    Schedule,
    Classes,
    TeacherInfo,
    NativeEnglishTeachers,
    GsTeam,
    CampusDashboard,
    Rosters,
    SpeakingEval,
    SubPrep,
    PdfViewer
};



// =========================================================
// Page Manager
// =========================================================

class PageManager : public QStackedWidget
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



signals:

    void openDatabaseRequested();

    void newDatabaseRequested();


public:

    // =====================================================
    // Accessors
    // =====================================================

    SchedulePage* schedulePage() const;

    PersonalDetailsPage* personalDetailsPage() const;

    CalendarPage* calendarPage() const;

    SchedulePage* mySchedulePage() const;

    MyClassesPage* myClassesPage() const;

    RostersPage* rostersPage() const;

    SubPrepPage* subPrepPage() const;

    ClassesPage* classesPage() const;

    TeacherInfoPage* teacherPage() const;

    StaffDirectoryPage* nativeEnglishTeachersPage() const;

    StaffDirectoryPage* gsTeamPage() const;

    CampusDashboardPage* campusDashboard() const;

    SpeakingEvalPage* speakingPage() const;

    PdfViewerPage* pdfViewerPage() const;



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

    // =====================================================
    // Services
    // =====================================================

    ApplicationServices* m_services = nullptr;



    // =====================================================
    // Setup
    // =====================================================

    void registerPages();



    // =====================================================
    // Pages
    // =====================================================

    SchedulePage* m_schedulePage = nullptr;

    PersonalDetailsPage* m_personalDetailsPage = nullptr;

    CalendarPage* m_calendarPage = nullptr;

    SchedulePage* m_mySchedulePage = nullptr;

    MyClassesPage* m_myClassesPage = nullptr;

    RostersPage* m_rostersPage = nullptr;

    SubPrepPage* m_subPrepPage = nullptr;

    ClassesPage* m_classesPage = nullptr;

    TeacherInfoPage* m_teacherPage = nullptr;

    StaffDirectoryPage* m_nativeEnglishTeachersPage = nullptr;

    StaffDirectoryPage* m_gsTeamPage = nullptr;

    CampusDashboardPage* m_campusDashboard = nullptr;

    SpeakingEvalPage* m_speakingPage = nullptr;

    PdfViewerPage* m_pdfViewerPage = nullptr;



    // =====================================================
    // Registry
    // =====================================================

    QMap<PageType, BasePage*> m_pages;
};



#endif // PAGEMANAGER_H
