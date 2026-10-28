#pragma once

#include "next/application/class_co_teacher_assignment_use_case.h"
#include "next/application/class_co_teacher_page_read_port.h"
#include "next/application/class_co_teacher_teacher_choices_read_port.h"
#include "next/application/class_details_page_read_port.h"
#include "next/application/class_details_save_workflow.h"
#include "next/application/save_mode_preferences.h"
#include "next/domain/domain_types.h"

#include <QString>
#include <QWidget>

#include <functional>
#include <optional>

class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;
class QTimer;
class QWidget;

namespace ClassMngr::Next::Ui
{

enum class UnsavedChangesChoice
{
    Save,
    Discard,
    Cancel
};

struct SelectedClassEditorActions final
{
    Application::SaveMode saveMode = Application::SaveMode::Automatic;
    std::function<Application::ClassDetailsPageReadResult(
        const QString&, const Domain::ClassId&)> readDetails;
    std::function<Application::ClassCoTeacherPageReadResult(
        const QString&, const Domain::ClassId&)> readCoTeacher;
    std::function<Application::ClassCoTeacherTeacherChoicesResult(
        const QString&)> readTeacherChoices;
    std::function<Domain::Result<Application::ClassCoTeacherTeacherChoice>(
        const QString&, const Domain::TeacherId&)> readTeacherDetails;
    std::function<Application::ClassDetailsSaveWorkflowResult(
        const QString&, const Application::ClassDetailsSaveWorkflowRequest&)>
        saveDetails;
    std::function<Domain::Result<void>(
        const QString&, const Application::ClassCoTeacherAssignmentRequest&)>
        saveCoTeacher;
    std::function<UnsavedChangesChoice(const QString&)> resolveUnsavedChanges;
};

// One resident editor serves the current selection; it never creates a widget
// tree per class. Class snapshots are released when the selection is cleared.
class SelectedClassEditor final : public QWidget
{
public:
    explicit SelectedClassEditor(
        SelectedClassEditorActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] bool hasUnsavedChanges() const noexcept;
    [[nodiscard]] bool isLoaded() const noexcept;
    [[nodiscard]] std::optional<Domain::ClassId> classId() const;
    [[nodiscard]] QTabWidget* sections() const noexcept;
    [[nodiscard]] QComboBox* gradeChoice() const noexcept;
    [[nodiscard]] QComboBox* levelChoice() const noexcept;
    [[nodiscard]] QComboBox* coTeacherChoice() const noexcept;
    [[nodiscard]] QTableWidget* regularSchedule() const noexcept;
    [[nodiscard]] QTableWidget* intensiveSchedule() const noexcept;

    void loadClass(const QString& databasePath, const Domain::ClassId& classId);
    void clear();
    [[nodiscard]] bool resolveUnsavedChanges(const QString& reason);
    [[nodiscard]] bool saveChanges(QString* error = nullptr);
    [[nodiscard]] bool discardChanges(QString* error = nullptr);

private:
    void ensureUi();
    QWidget* buildDetailsTab();
    QWidget* buildCoTeacherTab();
    void setStatus(const QString& message, bool error);
    void setCoTeacherStatus(const QString& message, bool error);
    void clearDetailsDisplay();
    void readClassDetails();
    void readCoTeacher();
    void readTeacherChoices();
    void readSelectedTeacherDetails();
    void updateGradeChoices(bool resetDependentValues);
    void updateBookChoices(bool resetSelectedValues);
    void updateTeacherDisplay();
    void updateCoTeacherDisplay();
    void updateDirtyState();
    void scheduleDetailsSave();
    [[nodiscard]] Application::ClassDetailsValidationInput detailsDraft() const;
    [[nodiscard]] bool saveDetails(QString* error);
    [[nodiscard]] bool saveCoTeacher(QString* error);
    [[nodiscard]] bool loadCoTeacherIfNeeded();
    [[nodiscard]] bool applyDetailsSnapshot(
        const Application::ClassDetailsPageReadSnapshot& snapshot,
        QString* error
        );
    void populateSchedule(
        QTableWidget* table,
        const std::vector<Application::ClassDetailsPageScheduleRow>& rows
        );
    [[nodiscard]] std::vector<Application::ClassDetailsScheduleValidationRow>
    scheduleRows(const QTableWidget* table) const;
    void appendScheduleRow(QTableWidget* table);
    void removeSelectedScheduleRows(QTableWidget* table);
    [[nodiscard]] UnsavedChangesChoice promptUnsavedChanges(
        const QString& reason
        ) const;
    void retranslateUi();

protected:
    void changeEvent(QEvent* event) override;

private:
    SelectedClassEditorActions m_actions;
    Application::SaveMode m_saveMode = Application::SaveMode::Automatic;
    QTabWidget* m_sections = nullptr;
    QWidget* m_detailsTab = nullptr;
    QWidget* m_coTeacherTab = nullptr;
    QLabel* m_emptyState = nullptr;
    QLabel* m_detailsStatus = nullptr;
    QLabel* m_coTeacherStatus = nullptr;
    QLabel* m_teacherLabel = nullptr;
    QLabel* m_studentCountLabel = nullptr;
    QLabel* m_coTeacherNameLabel = nullptr;
    QLabel* m_coTeacherRoomLabel = nullptr;
    QLabel* m_coTeacherInternetLabel = nullptr;
    QLabel* m_coTeacherWifiNameLabel = nullptr;
    QLabel* m_coTeacherWifiPasswordLabel = nullptr;
    QLabel* m_coTeacherProjectionLabel = nullptr;
    QLabel* m_coTeacherZoomIdLabel = nullptr;
    QLabel* m_coTeacherZoomPasswordLabel = nullptr;
    QComboBox* m_grade = nullptr;
    QComboBox* m_level = nullptr;
    QComboBox* m_readingBook = nullptr;
    QComboBox* m_essayBook = nullptr;
    QLineEdit* m_classColor = nullptr;
    QLineEdit* m_fontColor = nullptr;
    QPushButton* m_chooseColor = nullptr;
    QPushButton* m_regularScheduleAdd = nullptr;
    QPushButton* m_intensiveScheduleAdd = nullptr;
    QPushButton* m_saveDetailsButton = nullptr;
    QPushButton* m_retryDetailsButton = nullptr;
    QTableWidget* m_regularSchedule = nullptr;
    QTableWidget* m_intensiveSchedule = nullptr;
    QComboBox* m_coTeacherLocale = nullptr;
    QComboBox* m_coTeacher = nullptr;
    QPushButton* m_saveCoTeacherButton = nullptr;
    QPushButton* m_retryCoTeacherButton = nullptr;
    QTimer* m_detailsAutosave = nullptr;
    std::optional<Domain::ClassId> m_classId;
    std::optional<QString> m_databasePath;
    std::optional<Application::ClassDetailsPageReadSnapshot> m_detailsSnapshot;
    std::optional<Application::ClassCoTeacherPageReadSnapshot> m_coTeacherSnapshot;
    std::optional<Application::ClassCoTeacherTeacherChoicesSnapshot> m_teacherChoices;
    std::optional<Application::ClassCoTeacherTeacherChoice> m_selectedTeacherDetails;
    std::optional<Domain::TeacherId> m_savedCoTeacherId;
    std::optional<Domain::TeacherId> m_selectedTeacherDetailsId;
    bool m_detailsLoaded = false;
    bool m_coTeacherReadAttempted = false;
    bool m_coTeacherChoicesReadAttempted = false;
    bool m_detailsDirty = false;
    bool m_coTeacherDirty = false;
    bool m_loading = false;
    bool m_rebuildingChoices = false;
    int m_previousSectionIndex = 0;
};

} // namespace ClassMngr::Next::Ui
