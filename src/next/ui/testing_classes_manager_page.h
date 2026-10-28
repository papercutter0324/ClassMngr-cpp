#pragma once

#include "next/application/save_mode_preferences.h"
#include "next/application/schedule_testing_class_choices_query.h"
#include "next/application/testing_class_create.h"
#include "next/application/testing_class_delete.h"
#include "next/application/testing_class_details_read_query.h"
#include "next/application/testing_class_details_update.h"
#include "next/application/testing_teacher_choices_read_query.h"
#include "next/ui/page.h"
#include "next/ui/testing_classes_model.h"

#include <QColor>
#include <QString>
#include <QWidget>

#include <functional>
#include <optional>

class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTimer;

namespace ClassMngr::Next::Ui
{

enum class TestingClassesUnsavedChoice
{
    Save,
    Discard,
    Cancel
};

struct TestingClassesManagerPageActions final
{
    Application::SaveMode saveMode = Application::SaveMode::Automatic;
    std::function<QString()> workspaceIdentity;
    std::function<std::optional<QString>()> activeWorkspacePath;
    std::function<Application::ScheduleTestingClassChoicesReadResult(
        const QString&)> readClasses;
    std::function<Application::TestingClassDetailsReadResult(
        const QString&, const Domain::ClassId&)> readDetails;
    std::function<Application::TestingTeacherChoicesReadResult(
        const QString&)> readTeacherChoices;
    std::function<Application::TestingClassDetailsUpdateResult(
        const QString&, const Application::TestingClassDetailsUpdateRequest&)>
        updateDetails;
    std::function<Application::TestingClassCreateResult(
        const QString&, const Application::TestingClassCreateRequest&)>
        createClass;
    std::function<Application::TestingClassDeleteResult(
        const QString&, const Application::TestingClassDeleteRequest&)>
        deleteClass;
    std::function<TestingClassesUnsavedChoice(const QString&)>
        resolveUnsavedChanges;
    std::function<bool(const QString&)> confirmDelete;
};

// A small, model-backed testing-class manager with one reusable editor.
class TestingClassesManagerPage final : public Page
{
public:
    explicit TestingClassesManagerPage(
        const PageContext& context,
        TestingClassesManagerPageActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] TestingClassesModel* model() const noexcept;
    [[nodiscard]] QListView* classList() const noexcept;
    [[nodiscard]] QTabWidget* sections() const noexcept;
    [[nodiscard]] QWidget* editorWidget() const noexcept;
    [[nodiscard]] QLineEdit* nameEdit() const noexcept;
    [[nodiscard]] QComboBox* gradeChoice() const noexcept;
    [[nodiscard]] QComboBox* levelChoice() const noexcept;
    [[nodiscard]] QLineEdit* roomEdit() const noexcept;
    [[nodiscard]] QComboBox* teacherChoice() const noexcept;
    [[nodiscard]] QLineEdit* classColorEdit() const noexcept;
    [[nodiscard]] QLineEdit* fontColorEdit() const noexcept;
    [[nodiscard]] QPlainTextEdit* notesEdit() const noexcept;
    [[nodiscard]] QPushButton* addButton() const noexcept;
    [[nodiscard]] QPushButton* deleteButton() const noexcept;
    [[nodiscard]] QPushButton* saveButton() const noexcept;
    [[nodiscard]] QPushButton* retryButton() const noexcept;
    [[nodiscard]] QLabel* statusLabel() const noexcept;
    [[nodiscard]] QLabel* pendingSlotLabel() const noexcept;
    [[nodiscard]] const std::optional<Domain::ClassId>& selectedClassId() const noexcept;
    [[nodiscard]] bool isNewDraft() const noexcept;
    [[nodiscard]] bool hasLoadedDetails() const noexcept;
    [[nodiscard]] bool refreshFromStore();

    // Schedule pages can pass the typed slot context when opening this manager.
    void openNewClassForPendingSlot(
        std::optional<Application::TestingClassPendingScheduleSlot> slot
        );
    void clearForWorkspaceChange();
    void clearForClosedWorkspace();

    [[nodiscard]] PageOperationResult<void> onActivating() override;
    void onSuspending() noexcept override;
    [[nodiscard]] bool hasUnsavedChanges() const noexcept override;
    [[nodiscard]] PageOperationResult<void> saveChanges() override;
    [[nodiscard]] PageOperationResult<void> discardChanges() override;

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildUi();
    void retranslateUi();
    void clearEditor();
    void beginNewClass(
        std::optional<Application::TestingClassPendingScheduleSlot> slot = {}
        );
    [[nodiscard]] bool loadClass(const Domain::ClassId& classId);
    void selectRow(const std::optional<Domain::ClassId>& classId);
    void handleSelectionChange(const QModelIndex& current);
    [[nodiscard]] bool resolveDraft(const QString& reason);
    void markDirty();
    void scheduleAutomaticSave();
    void updateLevelChoices(const QString& selected = {});
    void updateColors();
    void loadTeacherChoices();
    void setStatus(const QString& message, bool isError);
    void updateActions();
    [[nodiscard]] std::optional<QString> activePath() const;
    [[nodiscard]] Application::TestingClassDetailsUpdateRequest
        detailsRequest() const;
    [[nodiscard]] Application::TestingClassCreateRequest createRequest() const;
    [[nodiscard]] bool validateEditor(QString* error) const;
    [[nodiscard]] TestingClassesUnsavedChoice askUnsavedChanges(
        const QString& reason
        ) const;
    [[nodiscard]] bool askDeleteConfirmation(const QString& className) const;
    void addClass();
    void deleteSelectedClass();

    TestingClassesManagerPageActions m_actions;
    TestingClassesModel* m_model = nullptr;
    QListView* m_classList = nullptr;
    QWidget* m_editorWidget = nullptr;
    QTabWidget* m_sections = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_gradeChoice = nullptr;
    QComboBox* m_levelChoice = nullptr;
    QLineEdit* m_roomEdit = nullptr;
    QComboBox* m_teacherChoice = nullptr;
    QLineEdit* m_classColorEdit = nullptr;
    QLineEdit* m_fontColorEdit = nullptr;
    QPlainTextEdit* m_notesEdit = nullptr;
    QLabel* m_classColorPreview = nullptr;
    QLabel* m_fontColorPreview = nullptr;
    QLabel* m_pageTitle = nullptr;
    QLabel* m_pageSubtitle = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_gradeLabel = nullptr;
    QLabel* m_levelLabel = nullptr;
    QLabel* m_roomLabel = nullptr;
    QLabel* m_teacherLabel = nullptr;
    QLabel* m_classColorLabel = nullptr;
    QLabel* m_fontColorLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_pendingSlotLabel = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_retryButton = nullptr;
    QTimer* m_autosaveTimer = nullptr;
    Application::SaveMode m_saveMode = Application::SaveMode::Automatic;
    std::optional<Domain::ClassId> m_selectedClassId;
    std::optional<Application::TestingClassDetailsSnapshot> m_savedDetails;
    std::optional<Application::TestingClassPendingScheduleSlot> m_pendingSlot;
    std::optional<Application::TestingClassPendingScheduleSlot>
        m_requestedPendingSlot;
    std::optional<Domain::ClassId> m_previousClassIdForDraft;
    std::optional<QString> m_workspaceIdentity;
    QString m_lastError;
    bool m_isNewDraft = false;
    bool m_dirty = false;
    bool m_loading = false;
    bool m_refreshing = false;
    bool m_teacherChoicesUnavailable = false;
    bool m_requestedNewDraft = false;
    bool m_pageActive = false;
};

} // namespace ClassMngr::Next::Ui
