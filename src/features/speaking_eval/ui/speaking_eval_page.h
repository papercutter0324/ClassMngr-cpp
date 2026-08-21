#pragma once

#include "ui/shared/pages/basepage.h"

#include "domain/models/classroom.h"
#include "domain/models/speaking_evaluation.h"
#include "features/classes/models/class_tab_navigation_model.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "features/speaking_eval/ui/speaking_eval_table_view.h"

#include <QList>
#include <QString>
#include <QStringList>

class ApplicationServices;
class AutosaveCoordinator;
class PageHeader;
class QComboBox;
class QLabel;
class QModelIndex;
class QPushButton;
class NavigationTabWidget;
class QVBoxLayout;
class QUndoStack;
class QWidget;
class SpeakingEvalDelegate;
class SpeakingEvalModel;
class OnScreenKeyboard;

class SpeakingEvalPage : public BasePage
{
    Q_OBJECT

public:
    explicit SpeakingEvalPage(
        ApplicationServices* services,
        bool embedded = false,
        QWidget* parent = nullptr
        );

    void loadEvaluation(
        const Classroom& classroom,
        const QString& evaluationName
        );

    void loadEvaluations(
        int selectedClassId = -1,
        const QString& selectedEvaluationName = QString()
        );
    void setScheduleDisplayMode(
        ScheduleDisplayMode mode
        );
    void refreshNavigationPreferences();
    void showKoreanKeyboard();

    void saveData() override;

    bool saveChanges() override;

    bool hasUnsavedChanges() const override;

    void discardChanges() override;

    QString unsavedChangesTitle() const override;

    QString unsavedChangesMessage() const override;

    void setSaveMode(
        SaveMode mode
        ) override;

    void refresh() override;
    void clearDatabaseState() override;
    void retranslateUi() override;
    [[nodiscard]] PageOutputCapabilities
        outputCapabilities() const override;
    void printCurrentPage() override;
    void saveCurrentPageAs() override;

private slots:
    void importNames();

    void openKoreanKeyboard();

    void updateActions();

    void showReports();

    void generateClassAiComments();

private:
    void buildUi();

    void rebuildClassTabs(
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

    void syncEvaluationTabFont();

    bool activateEvaluation(
        int classId,
        const QString& evaluationName
        );

    void restoreEvaluationTabSelection();

    void syncTabWidgetToClass(
        NavigationTabWidget* tabs,
        int classId
        );

    int currentClassIdFromTabs(
        NavigationTabWidget* tabs
        ) const;

    QString currentEvaluationNameFromTabs() const;

    Classroom classroomById(
        int classId
        ) const;

    int firstEvaluationClassId() const;

    void setEvaluationEditorAvailable(
        bool available
        );

    void loadEvaluationData(
        const Classroom& classroom,
        const QString& evaluationName
        );

    void setupTable();

    void updateHeaderText();
    [[nodiscard]] bool hasReportStudents() const;
    void outputReports(
        bool print
        );

    bool saveEvaluationInternal(
        bool showValidationMessages,
        bool showSuccessMessage
        );

    void scheduleAutosave();

    QList<SpeakingEvalCellEdit> nameImportChanges(
        const QStringList& rosterColumns,
        const QList<QStringList>& rosterRows
        ) const;

    void handleNameCellChanged(
        const QModelIndex& topLeft,
        const QModelIndex& bottomRight
        );

    void resolveDuplicateName(
        int row,
        int editedColumn
        );

    QList<QStringList> unmatchedRosterNamePairs() const;

    void selectEvaluationCell(
        int row,
        SpeakingEvalColumn column
        );

private:
    ApplicationServices* m_services = nullptr;
    bool m_embedded = false;
    Classroom m_classroom;
    QList<Classroom> m_evaluationClasses;
    QString m_evaluationName;
    bool m_loadingEvaluation = false;
    bool m_importingNames = false;
    bool m_resolvingDuplicateName = false;
    bool m_rebuildingClassTabs = false;
    bool m_restoringClassTabs = false;
    bool m_syncingEvaluationTabs = false;
    AutosaveCoordinator* m_autosave = nullptr;
    ClassTabNavigation::DayFilter m_dayFilter{
        {},
        ClassTabNavigation::ScheduleSource::Regular,
        ClassTabNavigation::VisibilityScope::ActiveSchedule
    };

    PageHeader* m_pageHeader = nullptr;
    QLabel* m_embeddedHeading = nullptr;
    QLabel* m_embeddedEvaluationLabel = nullptr;
    QComboBox* m_embeddedEvaluationCombo = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QWidget* m_tabsContainer = nullptr;
    QWidget* m_classTabsContainer = nullptr;
    QVBoxLayout* m_classTabsLayout = nullptr;
    NavigationTabWidget* m_classTabs = nullptr;
    NavigationTabWidget* m_evaluationTabs = nullptr;

    SpeakingEvalModel* m_model = nullptr;
    SpeakingEvalTableView* m_table = nullptr;
    SpeakingEvalDelegate* m_delegate = nullptr;
    QUndoStack* m_undoStack = nullptr;

    QPushButton* m_importNamesButton = nullptr;
    QList<QPushButton*> m_reportButtons;
    QPushButton* m_koreanKeyboardButton = nullptr;
    OnScreenKeyboard* m_onScreenKeyboard = nullptr;
    QPushButton* m_saveButton = nullptr;
};
