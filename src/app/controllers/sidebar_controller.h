#pragma once

#include "features/teacher/upcoming_birthday_schedule.h"

#include <QDate>
#include <QList>
#include <QObject>
#include <QString>

#include <optional>

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
    void importClasses();
    void exportClasses();

    // =====================================================
    // Teacher Actions
    // =====================================================

    void addTeacher();
    void deleteTeacher();
    void showUpcomingBirthdays();
    void importTeachers();

private:

    // =====================================================
    // Lookup Helpers
    // =====================================================

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

    [[nodiscard]] std::optional<UpcomingBirthdaySchedule>
        loadUpcomingBirthdaySchedule(const QDate& referenceDate) const;

    void updateActionStates();
    void updateActionStates(
        bool hasClasses,
        bool hasTeachers
        );

    void saveClassExport(
        const QList<int>& classIds,
        const QString& suggestedBaseName,
        const QString& dialogTitle
        );

private:

    ApplicationServices* m_services{nullptr};
    Sidebar* m_sidebar{nullptr};
    PageManager* m_pages{nullptr};
    ActionRegistry* m_actions{nullptr};
};
