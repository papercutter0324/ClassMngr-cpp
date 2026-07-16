#pragma once

#include "ui/shared/pages/basepage.h"

#include "domain/models/classroom.h"
#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/ui/speaking_eval_table_view.h"

#include <QList>
#include <QString>
#include <QStringList>

class ApplicationServices;
class QLabel;
class QModelIndex;
class QPushButton;
class QTabWidget;
class QTimer;
class QVBoxLayout;
class QUndoStack;
class QWidget;
class SpeakingEvalDelegate;
class SpeakingEvalModel;

class SpeakingEvalPage : public BasePage
{
    Q_OBJECT

public:
    explicit SpeakingEvalPage(
        ApplicationServices* services,
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

private slots:
    void importNames();

    void openKoreanKeyboard();

    void autosave();

    void updateActions();

    void showReports();

    void exportReports();

private:
    void buildUi();

    void rebuildClassTabs(
        int selectedClassId
        );

    bool activateEvaluation(
        int classId,
        const QString& evaluationName
        );

    void restoreEvaluationTabSelection();

    void syncTabWidgetToClass(
        QTabWidget* tabs,
        int classId
        );

    int currentClassIdFromTabs(
        QTabWidget* tabs
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
    Classroom m_classroom;
    QList<Classroom> m_evaluationClasses;
    QString m_evaluationName;
    bool m_loadingEvaluation = false;
    bool m_importingNames = false;
    bool m_resolvingDuplicateName = false;
    bool m_rebuildingClassTabs = false;
    bool m_restoringClassTabs = false;
    bool m_syncingEvaluationTabs = false;
    SaveMode m_saveMode = SaveMode::Automatic;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QWidget* m_tabsContainer = nullptr;
    QWidget* m_classTabsContainer = nullptr;
    QVBoxLayout* m_classTabsLayout = nullptr;
    QTabWidget* m_classTabs = nullptr;
    QTabWidget* m_evaluationTabs = nullptr;

    SpeakingEvalModel* m_model = nullptr;
    SpeakingEvalTableView* m_table = nullptr;
    SpeakingEvalDelegate* m_delegate = nullptr;
    QUndoStack* m_undoStack = nullptr;

    QPushButton* m_importNamesButton = nullptr;
    QList<QPushButton*> m_reportButtons;
    QPushButton* m_koreanKeyboardButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QTimer* m_autosaveTimer = nullptr;
};
