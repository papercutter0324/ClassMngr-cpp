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
class ClassInfoPage;
class ClassNotesPage;
class TeacherInfoPage;
class CampusDashboardPage;
class RosterPage;
class SpeakingEvalPage;
class SubPrepPage;



// =========================================================
// Page Types
// =========================================================

enum class PageType
{
    MyInfo,
    Schedule,
    ClassInfo,
    TeacherInfo,
    CampusDashboard,
    Roster,
    ClassNotes,
    SpeakingEval,
    SubPrep
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
        ApplicationServices* services
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



    // =====================================================
    // Accessors
    // =====================================================

    SchedulePage* schedulePage() const;

    MyInfoPage* myInfoPage() const;

    SubPrepPage* subPrepPage() const;

    ClassInfoPage* classInfoPage() const;

    ClassNotesPage* classNotesPage() const;

    TeacherInfoPage* teacherPage() const;

    CampusDashboardPage* campusDashboard() const;

    RosterPage* rosterPage() const;

    SpeakingEvalPage* speakingPage() const;



    // =====================================================
    // Refresh
    // =====================================================

    void refreshAll();



private:

    // =====================================================
    // State
    // =====================================================

    bool m_initialized = false;

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

    SubPrepPage* m_subPrepPage = nullptr;

    ClassInfoPage* m_classInfoPage = nullptr;

    ClassNotesPage* m_classNotesPage = nullptr;

    TeacherInfoPage* m_teacherPage = nullptr;

    CampusDashboardPage* m_campusDashboard = nullptr;

    RosterPage* m_rosterPage = nullptr;

    SpeakingEvalPage* m_speakingPage = nullptr;



    // =====================================================
    // Registry
    // =====================================================

    QMap<PageType, BasePage*> m_pages;
};



#endif // PAGEMANAGER_H
