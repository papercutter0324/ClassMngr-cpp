#pragma once

#include <QObject>

class ApplicationServices;
class Sidebar;
class PageManager;
class ActionRegistry;

class Classroom;
struct Teacher;

class SidebarController : public QObject
{
    Q_OBJECT

public:
    explicit SidebarController(
        ApplicationServices* services,
        Sidebar* sidebar,
        PageManager* pages,
        QObject* parent = nullptr
        );

    void connectActions(
        ActionRegistry& actions
        );

    // =====================================================
    // Refresh
    // =====================================================

    void refreshClassSidebar();
    void refreshTeacherSidebar();
    void refreshAllSidebars();

public slots:

    void handleClassInfoSaved(
        int classId
        );

    void handleTeacherSaved(
        int teacherId
        );

private slots:

    // =====================================================
    // Class Actions
    // =====================================================

    void addClass();
    void deleteClass();

    // =====================================================
    // Teacher Actions
    // =====================================================

    void addTeacher();
    void deleteTeacher();

private:

    // =====================================================
    // Lookup Helpers
    // =====================================================

    Classroom getSelectedClass() const;

    Classroom getClassById(
        int classId
        ) const;

    Teacher getTeacherById(
        int teacherId
        ) const;

    int promptForClassToDelete() const;

    int promptForTeacherToDelete() const;

    QString classDisplayName(
        const Classroom& classroom
        ) const;

    bool confirmDeleteClass(
        const Classroom& classroom
        ) const;

    bool confirmDeleteTeacher(
        const Teacher& teacher
        ) const;

    void updateActionStates();

private:

    ApplicationServices* m_services{nullptr};
    Sidebar* m_sidebar{nullptr};
    PageManager* m_pages{nullptr};
    ActionRegistry* m_actions{nullptr};
};
