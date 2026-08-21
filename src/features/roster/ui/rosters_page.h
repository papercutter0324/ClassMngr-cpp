#pragma once

#include "domain/models/classroom.h"
#include "features/classes/models/class_tab_navigation_model.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "ui/shared/pages/basepage.h"

#include <QList>

class ApplicationServices;
class QLabel;
class NavigationTabWidget;
class QVBoxLayout;
class RosterEditorWidget;
class QWidget;

class RostersPage : public BasePage
{
    Q_OBJECT

public:
    explicit RostersPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void loadClass(
        const Classroom& classroom
        );
    void loadRosters(
        int selectedClassId = -1
        );
    void setScheduleDisplayMode(
        ScheduleDisplayMode mode
        );
    void refreshNavigationPreferences();

    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    QString unsavedChangesTitle() const override;
    QString unsavedChangesMessage() const override;
    void setSaveMode(
        SaveMode mode
        ) override;
    void clearDatabaseState() override;
    void retranslateUi() override;
    [[nodiscard]] PageOutputCapabilities
        outputCapabilities() const override;
    void printCurrentPage() override;
    void saveCurrentPageAs() override;

private:
    void buildUi();
    void rebuildRosterTabs(
        int selectedClassId
        );
    void createDayFilterControls(
        NavigationTabWidget* tabs
        );
    void setDayFilterEnabled(
        const QString& key,
        bool enabled
        );
    bool dayFilterEnabled(const QString& key) const;
    void setScheduleSource(
        ClassTabNavigation::ScheduleSource source
        );
    void setVisibilityScope(
        ClassTabNavigation::VisibilityScope visibilityScope
        );
    void setNavigationSelectionVisible(bool visible);
    bool activateRosterClass(
        int classId
        );
    void restoreRosterTabSelection();
    void syncTabWidgetToClass(
        NavigationTabWidget* tabs,
        int classId
        );
    int currentClassIdFromTabs(
        NavigationTabWidget* tabs
        ) const;
    Classroom classroomById(
        int classId
        ) const;
    int firstRosterClassId() const;
    void setRosterEditorAvailable(
        bool available
        );
    void updateHeaderText();

    ApplicationServices* m_services = nullptr;
    QList<Classroom> m_rosterClasses;
    Classroom m_currentClassroom;
    bool m_rebuildingRosterTabs = false;
    bool m_restoringRosterTabs = false;
    ClassTabNavigation::DayFilter m_dayFilter{
        {},
        ClassTabNavigation::ScheduleSource::Regular,
        ClassTabNavigation::VisibilityScope::ActiveSchedule
    };

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QWidget* m_tabsContainer = nullptr;
    QVBoxLayout* m_tabsLayout = nullptr;
    NavigationTabWidget* m_rosterTabs = nullptr;
    RosterEditorWidget* m_editor = nullptr;
};
