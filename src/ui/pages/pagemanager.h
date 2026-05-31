#ifndef PAGEMANAGER_H
#define PAGEMANAGER_H

#include <QHash>
#include <QStackedWidget>

#include "basepage.h"



// =========================================================
// Forward Declarations
// =========================================================

class ApplicationServices;

class SchedulePage;
class ClassInfoPage;
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

    SchedulePage *m_schedulePage = nullptr;

    ClassInfoPage *m_classInfoPage = nullptr;

    TeacherInfoPage *m_teacherPage = nullptr;

    CampusDashboardPage *m_campusDashboard = nullptr;

    RosterPage *m_rosterPage = nullptr;

    SpeakingEvalPage *m_speakingPage = nullptr;



    // =====================================================
    // Registry
    // =====================================================

    QHash<
        PageType,
        BasePage*
        > m_pages;
};



#endif // PAGEMANAGER_H