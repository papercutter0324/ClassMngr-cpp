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
class ClassesPage;
class TeacherInfoPage;
class CampusDashboardPage;
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
    Classes,
    TeacherInfo,
    CampusDashboard,
    MyInfoRosters,
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



signals:

    void openDatabaseRequested();

    void newDatabaseRequested();


public:

    // =====================================================
    // Accessors
    // =====================================================

    SchedulePage* schedulePage() const;

    MyInfoPage* myInfoPage() const;

    MyInfoPage* myInfoSchedulePage() const;

    MyInfoPage* myInfoClassInformationPage() const;

    MyInfoRostersPage* myInfoRostersPage() const;

    SubPrepPage* subPrepPage() const;

    ClassesPage* classesPage() const;

    TeacherInfoPage* teacherPage() const;

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

    MyInfoPage* m_myInfoPage = nullptr;

    MyInfoPage* m_myInfoSchedulePage = nullptr;

    MyInfoPage* m_myInfoClassInformationPage = nullptr;

    MyInfoRostersPage* m_myInfoRostersPage = nullptr;

    SubPrepPage* m_subPrepPage = nullptr;

    ClassesPage* m_classesPage = nullptr;

    TeacherInfoPage* m_teacherPage = nullptr;

    CampusDashboardPage* m_campusDashboard = nullptr;

    SpeakingEvalPage* m_speakingPage = nullptr;

    PdfViewerPage* m_pdfViewerPage = nullptr;



    // =====================================================
    // Registry
    // =====================================================

    QMap<PageType, BasePage*> m_pages;
};



#endif // PAGEMANAGER_H
