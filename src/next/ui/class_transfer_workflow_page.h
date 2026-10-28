#pragma once

#include "next/application/class_transfer_apply.h"
#include "next/application/class_transfer_export_query.h"
#include "next/application/class_transfer_preview_query.h"
#include "next/application/classes_list_read_query.h"
#include "next/ui/page.h"

#include <QString>

#include <functional>
#include <optional>

class QAbstractItemView;
class QComboBox;
class QLabel;
class QListView;
class QPushButton;
class QTabWidget;
class QTableView;

namespace ClassMngr::Next::Ui
{

struct ClassTransferWorkflowPageActions final
{
    std::function<std::optional<QString>()> activeWorkspacePath;
    std::function<Domain::Result<std::optional<QString>>()> chooseImportFile;
    std::function<Domain::Result<std::optional<QString>>()> chooseExportFile;
    std::function<Application::ClassesListReadResult(const QString&)> readClasses;
    std::function<Application::ClassTransferExportResult(
        const QString&, const Application::ClassTransferExportRequest&)> exportClasses;
    std::function<Application::ClassTransferPreviewQueryResult(
        const QString&, const Application::ClassTransferPreviewRequest&)> previewImport;
    std::function<Application::ClassTransferApplyResult(
        const QString&, Application::ClassTransferApplyCommand)> applyImport;
    std::function<void(const Domain::ClassId&)> selectImportedClass;
};

class ClassTransferWorkflowPage final : public Page
{
public:
    explicit ClassTransferWorkflowPage(
        const PageContext& context,
        ClassTransferWorkflowPageActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] QListView* exportClassList() const noexcept;
    [[nodiscard]] QTabWidget* reviewTabs() const noexcept;
    [[nodiscard]] QTableView* classReviewTable() const noexcept;
    [[nodiscard]] QTableView* teacherReviewTable() const noexcept;
    [[nodiscard]] QComboBox* resolutionChoice() const noexcept;
    [[nodiscard]] QPushButton* exportButton() const noexcept;
    [[nodiscard]] QPushButton* importButton() const noexcept;
    [[nodiscard]] QPushButton* refreshPreviewButton() const noexcept;
    [[nodiscard]] QPushButton* applyButton() const noexcept;
    [[nodiscard]] QLabel* statusLabel() const noexcept;
    [[nodiscard]] int exportedClassChoiceCount() const noexcept;
    [[nodiscard]] bool hasRetainedPackage() const noexcept;
    [[nodiscard]] QString reviewedWorkspacePath() const;

    [[nodiscard]] bool refreshClasses();
    [[nodiscard]] bool exportSelectedTo(const QString& outputPath);
    [[nodiscard]] bool loadImportFile(const QString& inputPath);
    [[nodiscard]] bool applyImport();
    void cancelImportReview();
    void clearForWorkspaceChange();
    void clearForClosedWorkspace();

    [[nodiscard]] PageOperationResult<void> onActivating() override;
    void onSuspending() noexcept override;

private:
    class ClassSelectionModel;
    class ReviewModel;

    void buildUi();
    void retranslateUi();
    void chooseExportPath();
    void chooseImportPath();
    void refreshReviewChoices();
    void refreshApplyState();
    void refreshPreview();
    void clearReview();
    void setStatus(const QString& message, bool error);
    [[nodiscard]] std::optional<QString> activePath() const;
    [[nodiscard]] Application::ClassTransferPreviewRequest previewRequest() const;
    [[nodiscard]] Application::ClassTransferApplyRequest applyRequest() const;
    [[nodiscard]] Application::ClassTransferApplyCandidates applyCandidates() const;
    [[nodiscard]] bool ensureReviewPathIsCurrent();

    ClassTransferWorkflowPageActions m_actions;
    ClassSelectionModel* m_classSelectionModel = nullptr;
    ReviewModel* m_classReviewModel = nullptr;
    ReviewModel* m_teacherReviewModel = nullptr;
    QListView* m_exportClassList = nullptr;
    QTableView* m_classReviewTable = nullptr;
    QTableView* m_teacherReviewTable = nullptr;
    QTabWidget* m_reviewTabs = nullptr;
    QComboBox* m_resolutionChoice = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_reviewSummaryLabel = nullptr;
    QPushButton* m_exportButton = nullptr;
    QPushButton* m_importButton = nullptr;
    QPushButton* m_applyButton = nullptr;
    QPushButton* m_cancelReviewButton = nullptr;
    QPushButton* m_refreshPreviewButton = nullptr;
    std::optional<Application::ClassTransferPortablePackage> m_package;
    std::optional<Application::ClassTransferPreviewResult> m_preview;
    std::optional<QString> m_loadedWorkspacePath;
    std::optional<QString> m_reviewWorkspacePath;
    bool m_loading = false;
};

} // namespace ClassMngr::Next::Ui
