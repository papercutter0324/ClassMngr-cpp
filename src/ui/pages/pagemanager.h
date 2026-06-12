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
class ClassInfoPage;
class ClassNotesPage;
class TeacherInfoPage;
class CampusDashboardPage;
class RosterPage;
class SpeakingEvalPage;



// =========================================================
// Page Types
// =========================================================

enum class PageType
{
    Schedule,
    ClassInfo,
    TeacherInfo,
    CampusDashboard,
    Roster,
    ClassNotes,
    SpeakingEval
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



    // =====================================================
    // Accessors
    // =====================================================

    SchedulePage* schedulePage() const;

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
