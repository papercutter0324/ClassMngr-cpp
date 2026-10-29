#pragma once

#include "next/application/roster_save_use_case.h"
#include "next/ui/roster_output_actions.h"
#include "next/application/roster_row_transfer_use_case.h"
#include "next/application/roster_transfer_target_choices.h"
#include "next/application/roster_window_read_query.h"
#include "next/application/save_mode_preferences.h"
#include "next/application/speaking_evaluation_roster_score_import_batch_use_case.h"
#include "next/domain/domain_types.h"

#include <QWidget>

#include <array>
#include <functional>
#include <optional>

class QLabel;
class QPushButton;
class QTableView;
class QTimer;
class OnScreenKeyboard;

namespace ClassMngr::Next::Ui
{

struct RosterEditorActions final
{
    RosterOutputActions output;
    Application::SaveMode saveMode = Application::SaveMode::Automatic;
    std::function<Application::RosterWindowReadResult(
        const QString&, const Domain::ClassId&)> readRoster;
    std::function<Domain::Result<void>(
        const QString&, const Application::RosterSaveRequest&)> saveRoster;
    std::function<Domain::Result<bool>(
        const QString&, const Domain::ClassId&, const std::u16string&)>
        columnHasTailData;
    std::function<Application::SpeakingEvaluationRosterScoreImportBatchResult(
        const QString&, const Domain::ClassId&,
        const std::vector<std::u16string>&)> readEvaluationScores;
    std::function<Application::RosterTransferTargetChoicesResult(
        const QString&, const Domain::ClassId&)> readTransferTargets;
    std::function<Domain::Result<void>(
        const QString&, const Application::RosterRowTransferSaveRequest&)>
        saveRosterTransfer;
};

class RosterTableModel;
class RosterCellDelegate;

// A single 25-row model/view editor shared by regular and testing classes.
// Item editors are created by the delegate only while a cell is being edited.
class RosterEditorWidget final : public QWidget
{
public:
    explicit RosterEditorWidget(
        RosterEditorActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] RosterTableModel* model() const noexcept;
    [[nodiscard]] QTableView* tableView() const noexcept;
    [[nodiscard]] bool hasUnsavedChanges() const noexcept;
    [[nodiscard]] bool isLoaded() const noexcept;
    [[nodiscard]] bool readFailed() const noexcept;

    void enterClassSection(
        const QString& databasePath,
        const Domain::ClassId& classId,
        bool testingClassMode = false
        );
    void finishEditing();
    void openRosterOutput();
    void clear();
    [[nodiscard]] bool saveChanges(bool showValidationFeedback = true);
    [[nodiscard]] bool discardChanges();
    void setDirtyChangedCallback(std::function<void(bool)> callback);

private:
    void buildUi();
    void retranslateUi();
    void updateTablePalette();
    void showKeyboard();
    void releaseKeyboard();
    [[nodiscard]] bool reload();
    void updateActions();
    void updateStatus(const QString& message, bool error, bool retryable = false);
    void markDirty();
    void retry();
    void addColumn();
    void removeColumn();
    void removeStudentRow();
    void importScores();
    void showTransferMenu();
    [[nodiscard]] bool transferStudentToClass(
        int row, const Domain::ClassId& targetClassId);
    void moveStudentRow(int offset);
    void focusFirstValidationIssue();
    [[nodiscard]] bool hasQuestionableKoreanLengthErrors() const;
    [[nodiscard]] bool confirmQuestionableKoreanLengths() const;
    [[nodiscard]] std::optional<QString> activeColumnName() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    RosterEditorActions m_actions;
    RosterTableModel* m_model = nullptr;
    RosterCellDelegate* m_delegate = nullptr;
    QTableView* m_table = nullptr;
    QLabel* m_overflowLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_addColumnButton = nullptr;
    QPushButton* m_removeColumnButton = nullptr;
    QPushButton* m_removeRowButton = nullptr;
    QPushButton* m_importScoresButton = nullptr;
    QPushButton* m_transferButton = nullptr;
    QPushButton* m_moveUpButton = nullptr;
    QPushButton* m_moveDownButton = nullptr;
    QPushButton* m_outputButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_discardButton = nullptr;
    QPushButton* m_retryButton = nullptr;
    QPushButton* m_keyboardButton = nullptr;
    OnScreenKeyboard* m_keyboard = nullptr;
    QTimer* m_autosaveTimer = nullptr;
    std::optional<QString> m_databasePath;
    std::optional<Domain::ClassId> m_classId;
    std::function<void(bool)> m_dirtyChanged;
    QString m_lastStatus;
    bool m_loading = false;
    bool m_loaded = false;
    bool m_readFailed = false;
    bool m_saveFailed = false;
    bool m_hasRowsOutsideWindow = false;
    bool m_hasSourceEnglishColumn = false;
    bool m_hasSourceKoreanColumn = false;
    std::array<bool, 4> m_hasSourceEvaluationColumns{};
    bool m_widthsDirty = false;
    bool m_testingClassMode = false;
    std::vector<std::u16string> m_removedCustomColumns;
};

} // namespace ClassMngr::Next::Ui
