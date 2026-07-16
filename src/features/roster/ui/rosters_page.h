#pragma once

#include "domain/models/classroom.h"
#include "ui/shared/pages/basepage.h"

#include <QList>

class ApplicationServices;
class QLabel;
class QTabWidget;
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

private:
    void buildUi();
    void rebuildRosterTabs(
        int selectedClassId
        );
    bool activateRosterClass(
        int classId
        );
    void restoreRosterTabSelection();
    void syncTabWidgetToClass(
        QTabWidget* tabs,
        int classId
        );
    int currentClassIdFromTabs(
        QTabWidget* tabs
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

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QWidget* m_tabsContainer = nullptr;
    QVBoxLayout* m_tabsLayout = nullptr;
    QTabWidget* m_rosterTabs = nullptr;
    RosterEditorWidget* m_editor = nullptr;
};
