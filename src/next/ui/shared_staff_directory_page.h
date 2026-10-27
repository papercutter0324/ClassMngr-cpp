#pragma once

#include "next/application/gs_team_directory_read_port.h"
#include "next/application/gs_team_directory_save.h"
#include "next/application/native_english_teacher_directory_read_port.h"
#include "next/application/native_english_teacher_directory_save.h"
#include "next/application/save_mode_preferences.h"
#include "next/ui/page.h"
#include "next/ui/shared_staff_directory_table_model.h"

#include <QString>

#include <functional>
#include <vector>

class QLabel;
class QPushButton;
class QTableView;
class QTimer;
class OnScreenKeyboard;

namespace ClassMngr::Next::Ui
{

struct SharedStaffDirectoryPageActions final
{
    std::function<Application::NativeEnglishTeacherDirectoryReadResult()>
        readNativeEnglishTeachers;
    std::function<Application::NativeEnglishTeacherDirectorySaveOutcome(
        const Application::NativeEnglishTeacherDirectorySaveRequest&)>
        saveNativeEnglishTeachers;
    std::function<Application::GsTeamDirectoryReadResult()> readGsTeam;
    std::function<Application::GsTeamDirectorySaveOutcome(
        const Application::GsTeamDirectorySaveRequest&)> saveGsTeam;
    std::function<bool(int)> confirmDelete;
    std::function<void(const QString&, const QString&)> reportWarning;
    Application::SaveMode saveMode = Application::SaveMode::Automatic;
};

// One page and one compact table model serve both directory routes. The page
// keeps Native English and GS mutations in their separate typed branches.
class SharedStaffDirectoryPage final : public Page
{
public:
    explicit SharedStaffDirectoryPage(
        const PageContext& context,
        StaffDirectoryKind kind,
        SharedStaffDirectoryPageActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] StaffDirectoryKind kind() const noexcept;
    [[nodiscard]] SharedStaffDirectoryTableModel* directoryModel() const noexcept;
    [[nodiscard]] QTableView* tableView() const noexcept;
    [[nodiscard]] bool refreshFromStore(QString* error = nullptr);

    [[nodiscard]] PageOperationResult<void> onActivating() override;
    [[nodiscard]] bool hasUnsavedChanges() const noexcept override;
    [[nodiscard]] PageOperationResult<void> saveChanges() override;
    [[nodiscard]] PageOperationResult<void> discardChanges() override;
    void onSuspending() noexcept override;
    void setSaveMode(Application::SaveMode mode);
    void retranslateUi();

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildUi();
    void addRow();
    void deleteSelectedRows();
    void markDirty(bool scheduleAutosave = true);
    [[nodiscard]] PageOperationResult<void> saveDirectory(bool interactive);
    [[nodiscard]] PageOperationResult<void> reloadAfterSave();
    void updateActions();
    void reportWarning(const QString& title, const QString& message);
    void showStatus(const QString& message, bool error);
    [[nodiscard]] QString validationMessage(int issue) const;

    StaffDirectoryKind m_kind;
    SharedStaffDirectoryPageActions m_actions;
    Application::SaveMode m_saveMode = Application::SaveMode::Automatic;
    bool m_loading = false;
    bool m_dirty = false;
    bool m_reloadRequired = false;
    bool m_autosaveFailureStatusActive = false;
    std::vector<Domain::NativeEnglishTeacherId> m_deletedNativeEnglishIds;
    std::vector<Domain::GsTeamMemberId> m_deletedGsTeamIds;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_keyboardButton = nullptr;
    OnScreenKeyboard* m_onScreenKeyboard = nullptr;
    QTableView* m_table = nullptr;
    SharedStaffDirectoryTableModel* m_model = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_discardButton = nullptr;
    QTimer* m_autosaveTimer = nullptr;
};

} // namespace ClassMngr::Next::Ui
