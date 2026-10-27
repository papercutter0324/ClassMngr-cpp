#pragma once

#include "domain/models/teacher_import.h"
#include "features/teacher/import/teacher_import_file_validator.h"
#include "next/application/teacher_import_use_case.h"
#include "next/domain/operation_result.h"

#include <QDialog>
#include <QString>

#include <expected>
#include <functional>
#include <optional>

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QScrollArea;
class QVBoxLayout;
class QWidget;

namespace ClassMngr::Next::Ui
{

using TeacherImportApplyResult = std::expected<
    Application::TeacherImportUseCaseResult,
    Application::TeacherImportUseCaseError
    >;

struct InitialSetupTeacherImportActions final
{
    std::function<Domain::Result<std::optional<QString>>()> chooseWorkbook;
    std::function<TeacherImportFileValidation(const QString&)> validateWorkbook;
    std::function<int()> validationTimeoutMilliseconds;
    std::function<TeacherImportApplyResult(
        const Application::TeacherImportUseCaseRequest&
        )> apply;
    std::function<void()> cancelSetup;
};

class InitialSetupTeacherImportDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit InitialSetupTeacherImportDialog(
        InitialSetupTeacherImportActions actions,
        QWidget* parent = nullptr
        );

    void setFilePath(const QString& filePath);
    [[nodiscard]] QString successSummary() const;

public slots:
    void reject() override;

private:
    struct GroupControls final
    {
        QString level;
        QRadioButton* all = nullptr;
        QRadioButton* selected = nullptr;
        QRadioButton* none = nullptr;
        QWidget* checklist = nullptr;
        QList<QCheckBox*> candidateChecks;
    };

    void browseForWorkbook();
    void validateSelectedFile();
    void showValidation(const TeacherImportFileValidation& validation);
    void setLoading(bool loading);
    void clearReviewControls();
    void buildReviewControls(const TeacherImportPreview& preview);
    void applyImport();
    void updateApplyEnabled();
    [[nodiscard]] std::vector<Application::TeacherImportGroupDecision>
    groupDecisions() const;
    void setStatus(const QString& text);

    InitialSetupTeacherImportActions m_actions;
    QLineEdit* m_filePathEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_automaticLabel = nullptr;
    QLabel* m_koreanHeadingLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_retryButton = nullptr;
    QWidget* m_reviewWidget = nullptr;
    QVBoxLayout* m_reviewLayout = nullptr;
    QScrollArea* m_candidateScrollArea = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_applyButton = nullptr;
    QList<GroupControls> m_groupControls;
    std::optional<TeacherImportPreview> m_preview;
    QString m_successSummary;
    quint64 m_validationRequestId = 0;
    bool m_loading = false;
    bool m_cancelSetupCalled = false;
};

}  // namespace ClassMngr::Next::Ui
