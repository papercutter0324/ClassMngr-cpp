#pragma once

#include "core/result.h"
#include "next/application/schedule_import_apply_request.h"
#include "next/application/schedule_import_state_snapshot.h"
#include "next/ui/schedule_import_request_mapper.h"

#include <QDialog>
#include <QString>

#include <functional>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QGroupBox;
class QPushButton;
class QRadioButton;
class QStackedWidget;
class QTableView;
class QTextEdit;
class QIODevice;

namespace ClassMngr::Next::Ui
{

struct ScheduleImportWorkflowActions final
{
    std::function<QString(QWidget*)> chooseWorkbook;
    std::function<Result<ScheduleImportWorkbook>(QIODevice&, ScheduleImportKind)>
        parseWorkbook;
    std::function<int()> parseTimeoutMilliseconds;
    std::function<QString()> activeWorkspacePath;
    std::function<QString()> workspaceIdentity;
    std::function<Application::ScheduleImportStateSnapshotOutcome(
        const QString&)> readImportState;
    std::function<Application::ScheduleImportApplyResult(
        const QString&,
        const Application::ScheduleImportApplyRequest&)> applyImport;
    std::function<void(const Application::ScheduleImportApplySummary&)>
        importApplied;
};

class ScheduleImportTeacherReviewModel;
class ScheduleImportClassReviewModel;
class ScheduleImportCandidatePreviewModel;
class ScheduleImportChoiceDelegate;
struct ScheduleImportParseTaskResult;

class ScheduleImportWorkflowDialog final : public QDialog
{
public:
    explicit ScheduleImportWorkflowDialog(
        ScheduleImportWorkflowActions actions,
        QString expectedWorkspacePath,
        QString expectedWorkspaceIdentity,
        QWidget* parent = nullptr
        );
    ~ScheduleImportWorkflowDialog() override;

    [[nodiscard]] QLineEdit* sourcePathEdit() const noexcept;
    [[nodiscard]] QComboBox* sheetCombo() const noexcept;
    [[nodiscard]] QComboBox* userCombo() const noexcept;
    [[nodiscard]] QTableView* teacherTable() const noexcept;
    [[nodiscard]] QTableView* classTable() const noexcept;
    [[nodiscard]] QPushButton* parseButton() const noexcept;
    [[nodiscard]] QPushButton* primaryButton() const noexcept;
    [[nodiscard]] QLabel* statusLabel() const noexcept;
    [[nodiscard]] int reviewWidgetCount() const;

public:
    void reject() override;

protected:
    void changeEvent(QEvent* event) override;

private:
    struct FileMetadata final
    {
        QString canonicalPath;
        qint64 size = -1;
        qint64 lastModifiedMilliseconds = -1;
        qint64 metadataChangedMilliseconds = -1;
        qint64 birthMilliseconds = -1;

        friend bool operator==(const FileMetadata&, const FileMetadata&) = default;
    };

    void buildUi();
    void retranslateUi();
    void chooseWorkbook();
    void beginParse();
    void handleParseFinished(
        std::uint64_t requestId,
        std::shared_ptr<ScheduleImportParseTaskResult> result
        );
    void populateSheets();
    void populateUsers();
    void continueToReview();
    void updateReviewState();
    void updateReviewSummary(
        const Application::ScheduleImportReviewDecisionRequest& decisions
        );
    void applyImport();
    void showSourceError(const QString& message);
    void showReviewError(const QString& message);
    void clearReviewData();
    void releaseRetainedData();
    [[nodiscard]] bool workspaceIsCurrent(QString* error = nullptr) const;
    [[nodiscard]] bool sourceIsCurrent(QString* error = nullptr) const;
    [[nodiscard]] int selectedSheetIndex() const;
    [[nodiscard]] int selectedUserIndex() const;
    [[nodiscard]] Application::ScheduleImportPlanIntensiveMode intensiveMode() const;
    [[nodiscard]] static std::optional<FileMetadata> fileMetadata(
        const QString& path
        );

    ScheduleImportWorkflowActions m_actions;
    QString m_expectedWorkspacePath;
    QString m_expectedWorkspaceIdentity;
    QStackedWidget* m_pages = nullptr;
    QLineEdit* m_sourcePath = nullptr;
    QComboBox* m_kind = nullptr;
    QComboBox* m_sheet = nullptr;
    QComboBox* m_user = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_selectionStatus = nullptr;
    QLabel* m_reviewStatus = nullptr;
    QLabel* m_reviewSummary = nullptr;
    QLabel* m_reviewTitle = nullptr;
    QGroupBox* m_diagnosticsBox = nullptr;
    QString m_reviewUserName;
    QTextEdit* m_diagnostics = nullptr;
    QCheckBox* m_diagnosticsAcknowledged = nullptr;
    QGroupBox* m_intensiveGroup = nullptr;
    QRadioButton* m_updateIntensive = nullptr;
    QRadioButton* m_replaceIntensive = nullptr;
    QTableView* m_previewTable = nullptr;
    QTableView* m_teacherTable = nullptr;
    QTableView* m_classTable = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_primaryButton = nullptr;
    QPushButton* m_browseButton = nullptr;
    ScheduleImportCandidatePreviewModel* m_previewModel = nullptr;
    ScheduleImportTeacherReviewModel* m_teacherModel = nullptr;
    ScheduleImportClassReviewModel* m_classModel = nullptr;
    std::unique_ptr<ScheduleImportChoiceDelegate> m_choiceDelegate;
    std::unique_ptr<ScheduleImportReviewMapping> m_mapping;
    std::unique_ptr<ScheduleImportWorkbook> m_workbook;
    std::optional<Application::ScheduleImportApplyFailure> m_lastApplyFailure;
    std::shared_ptr<std::atomic_bool> m_parseCancellation;
    QObject* m_parseWatcher = nullptr;
    std::optional<FileMetadata> m_sourceMetadata;
    QString m_sourceFilePath;
    QString m_lastError;
    std::uint64_t m_loadRequestId = 0;
    bool m_releasing = false;
};

} // namespace ClassMngr::Next::Ui
