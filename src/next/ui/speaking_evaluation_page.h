#pragma once

#include "next/application/evaluation_default_selection.h"
#include "next/application/classes_list_read_port.h"
#include "next/application/classes_list_read_query.h"
#include "next/application/save_mode_preferences.h"
#include "next/application/speaking_evaluation_query.h"
#include "next/application/speaking_evaluation_roster_names_read_port.h"
#include "next/application/speaking_evaluation_save_use_case.h"
#include "next/ui/page.h"
#include "next/ui/speaking_evaluation_model.h"

#include <QString>

#include <functional>
#include <cstddef>
#include <memory>
#include <optional>

class QComboBox;
class QEvent;
class QLabel;
class QListView;
class QPushButton;
class QTableView;
class QTimer;
class QUndoStack;

namespace ClassMngr::Next::Ui
{

enum class SpeakingEvaluationUnsavedChoice
{
    Save,
    Discard,
    Cancel
};

struct SpeakingEvaluationPageActions final
{
    Application::SaveMode saveMode = Application::SaveMode::Automatic;
    std::function<QString()> workspaceIdentity;
    std::function<std::optional<QString>()> activeWorkspacePath;
    std::function<std::optional<Domain::ClassId>()> preferredClassSelection;
    std::function<void(const std::optional<Domain::ClassId>&)>
        setActiveClassSelection;
    std::function<Application::ClassesListReadResult(const QString&)> readClasses;
    std::function<std::unique_ptr<Application::SpeakingEvaluationReadPort>(
        const QString&)> evaluationReadPortFactory;
    std::function<std::unique_ptr<Application::SpeakingEvaluationSavePort>(
        const QString&)> evaluationSavePortFactory;
    std::function<std::unique_ptr<
        Application::SpeakingEvaluationRosterNamesReadPort>(const QString&)>
        rosterNamesReadPortFactory;
    std::function<SpeakingEvaluationUnsavedChoice(const QString&)>
        resolveUnsavedChanges;
    std::function<bool()> confirmQuestionableKoreanNameLengths;
};

class SpeakingEvaluationPage final : public Page
{
public:
    explicit SpeakingEvaluationPage(
        const PageContext& context,
        SpeakingEvaluationPageActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] SpeakingEvaluationClassListModel* classModel() const noexcept;
    [[nodiscard]] SpeakingEvaluationTableModel* matrixModel() const noexcept;
    [[nodiscard]] QListView* classList() const noexcept;
    [[nodiscard]] QComboBox* evaluationChoice() const noexcept;
    [[nodiscard]] QTableView* tableView() const noexcept;
    [[nodiscard]] QLabel* statusLabel() const noexcept;
    [[nodiscard]] QPushButton* retryButton() const noexcept;
    [[nodiscard]] std::optional<Domain::ClassId> selectedClassId() const;
    [[nodiscard]] QString selectedEvaluationName() const;
    [[nodiscard]] std::size_t residentMatrixCount() const noexcept;
    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] bool hasUnsavedChanges() const noexcept override;

    [[nodiscard]] PageOperationResult<void> onActivating() override;
    void onSuspending() noexcept override;
    [[nodiscard]] PageOperationResult<void> saveChanges() override;
    [[nodiscard]] PageOperationResult<void> discardChanges() override;

    [[nodiscard]] bool refreshFromStore();
    void clearForWorkspaceChange();
    void clearForClosedWorkspace();

protected:
    void changeEvent(QEvent* event) override;

private:
    enum class Status
    {
        Ready,
        NoWorkspace,
        Empty,
        ClassListError,
        EvaluationReadError,
        SaveError,
        ValidationError,
        ImportError
    };

    [[nodiscard]] std::optional<QString> activePath() const;
    [[nodiscard]] QString currentWorkspaceIdentity() const;
    [[nodiscard]] bool loadClassSummaries();
    [[nodiscard]] bool loadEvaluation();
    [[nodiscard]] bool selectClass(const Domain::ClassId& classId);
    [[nodiscard]] bool selectEvaluation(const QString& evaluationName);
    [[nodiscard]] bool prepareForSelectionChange(const QString& reason);
    [[nodiscard]] bool dropDraftForSelectionChange();
    [[nodiscard]] bool saveCurrentEvaluation(bool interactive);
    [[nodiscard]] bool reloadCurrentEvaluation();
    void retryCurrentOperation();
    void importRosterNames();
    void editPrivateNotes();
    void refreshValidation();
    void updateDirtyState(bool scheduleSave);
    void updateActions();
    void setStatus(Status status, const QString& message = {});
    void syncSelectionControls();
    void retranslateUi();
    void releaseActiveDraft();
    [[nodiscard]] SpeakingEvaluationUnsavedChoice askUnsavedChanges(
        const QString& reason
        ) const;
    [[nodiscard]] bool askQuestionableKoreanNameLengths() const;

    SpeakingEvaluationPageActions m_actions;
    SpeakingEvaluationClassListModel* m_classModel = nullptr;
    SpeakingEvaluationTableModel* m_matrixModel = nullptr;
    QListView* m_classList = nullptr;
    QComboBox* m_evaluationChoice = nullptr;
    QTableView* m_table = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_classSummary = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_dirtyIndicator = nullptr;
    QPushButton* m_importNamesButton = nullptr;
    QPushButton* m_editNotesButton = nullptr;
    QPushButton* m_undoButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_retryButton = nullptr;
    QTimer* m_autoSaveTimer = nullptr;
    QUndoStack* m_undoStack = nullptr;
    std::optional<Domain::ClassId> m_selectedClassId;
    QString m_evaluationName;
    QString m_workspaceIdentity;
    Status m_statusKind = Status::NoWorkspace;
    bool m_summariesLoaded = false;
    bool m_evaluationLoaded = false;
    bool m_evaluationReadFailed = false;
    bool m_active = false;
    bool m_syncingControls = false;
    bool m_refreshingValidation = false;
};

} // namespace ClassMngr::Next::Ui
