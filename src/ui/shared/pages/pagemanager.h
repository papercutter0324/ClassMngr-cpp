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
class MyInfoPage;
class MyInfoRostersPage;
class ClassInfoPage;
class ClassNotesPage;
class TeacherInfoPage;
class CampusDashboardPage;
class RosterPage;
class SpeakingEvalPage;
class SubPrepPage;
class PdfViewerPage;



// =========================================================
// Page Types
// =========================================================

enum class PageType
{
    MyInfo,
    MyInfoSchedule,
    MyInfoClassInformation,
    Schedule,
    ClassInfo,
    TeacherInfo,
    CampusDashboard,
    Roster,
    MyInfoRosters,
    ClassNotes,
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



    // =====================================================
    // Accessors
    // =====================================================

    SchedulePage* schedulePage() const;

    MyInfoPage* myInfoPage() const;

    MyInfoPage* myInfoSchedulePage() const;

    MyInfoPage* myInfoClassInformationPage() const;

    MyInfoRostersPage* myInfoRostersPage() const;

    SubPrepPage* subPrepPage() const;

    ClassInfoPage* classInfoPage() const;

    ClassNotesPage* classNotesPage() const;

    TeacherInfoPage* teacherPage() const;

    CampusDashboardPage* campusDashboard() const;

    RosterPage* rosterPage() const;

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

    MyInfoPage* m_myInfoPage = nullptr;

    MyInfoPage* m_myInfoSchedulePage = nullptr;

    MyInfoPage* m_myInfoClassInformationPage = nullptr;

    MyInfoRostersPage* m_myInfoRostersPage = nullptr;

    SubPrepPage* m_subPrepPage = nullptr;

    ClassInfoPage* m_classInfoPage = nullptr;

    ClassNotesPage* m_classNotesPage = nullptr;

    TeacherInfoPage* m_teacherPage = nullptr;

    CampusDashboardPage* m_campusDashboard = nullptr;

    RosterPage* m_rosterPage = nullptr;

    SpeakingEvalPage* m_speakingPage = nullptr;

    PdfViewerPage* m_pdfViewerPage = nullptr;



    // =====================================================
    // Registry
    // =====================================================

    QMap<PageType, BasePage*> m_pages;
};



#endif // PAGEMANAGER_H
