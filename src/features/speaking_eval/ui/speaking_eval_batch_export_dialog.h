#pragma once

#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include "ui/shared/dialogs/dialog_shell.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QCoreApplication>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

namespace SpeakingEvalBatchExportDialogPresentation
{

enum class NoticeKind
{
    None,
    Information,
    Warning
};

struct Notice
{
    NoticeKind kind = NoticeKind::None;
    QString title;
    QString message;
};

[[nodiscard]] inline QString translated(const char* sourceText)
{
    return QCoreApplication::translate(
        "SpeakingEvalBatchExportDialog",
        sourceText
        );
}

[[nodiscard]] inline QString savedOutputDetails(
    const DocumentOutputResult& result
    )
{
    QStringList savedPaths = result.savedPdfPaths;
    if (!result.savedArchivePath.isEmpty())
    {
        savedPaths.append(result.savedArchivePath);
    }
    if (savedPaths.isEmpty())
    {
        return {};
    }

    return translated("Saved output:")
        + QLatin1Char('\n')
        + savedPaths.join(QLatin1Char('\n'));
}

[[nodiscard]] inline Notice outcomeNotice(
    const DocumentOutputResult& result
    )
{
    const QString savedDetails = savedOutputDetails(result);
    if (result.status == DocumentOutputStatus::Canceled)
    {
        if (savedDetails.isEmpty())
        {
            return {};
        }

        return {
            NoticeKind::Information,
            translated("Reports Saved, Printing Canceled"),
            translated("%1\n\n%2")
                .arg(result.message, savedDetails)
        };
    }

    if (result.status == DocumentOutputStatus::Failed
        || result.status == DocumentOutputStatus::InternalRendererFailed)
    {
        if (savedDetails.isEmpty())
        {
            return {
                NoticeKind::Warning,
                translated("Report Export Failed"),
                result.message
            };
        }

        return {
            NoticeKind::Warning,
            translated("Printing Failed"),
            translated("%1\n\n%2")
                .arg(result.message, savedDetails)
        };
    }

    return {};
}

inline void showOutcomeNotice(
    QWidget* parent,
    const DocumentOutputResult& result
    )
{
    const Notice notice = outcomeNotice(result);
    switch (notice.kind)
    {
    case NoticeKind::Information:
        DialogServices::showInformation(
            parent,
            notice.title,
            notice.message
            );
        return;
    case NoticeKind::Warning:
        DialogServices::showWarning(
            parent,
            notice.title,
            notice.message
            );
        return;
    case NoticeKind::None:
        return;
    }
}

} // namespace SpeakingEvalBatchExportDialogPresentation

class SpeakingEvalBatchExportDialog : public DialogShell
{
    Q_OBJECT

public:
    enum class Mode
    {
        Print,
        SaveAs
    };

    explicit SpeakingEvalBatchExportDialog(
        const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
        int currentStudentIndex,
        const QString& defaultOutputDirectory,
        Mode mode,
        QWidget* parent = nullptr
        );

private:
    void updateControls();

    void chooseOutputDirectory();

    void previewReports();

    void exportReports();

    [[nodiscard]] bool confirmPowerPointDataAccess();

    [[nodiscard]] QList<SpeakingEvalBatchReportService::StudentReport>
    selectedReports() const;

    [[nodiscard]] SpeakingEvalBatchReportService::Renderer selectedRenderer() const;

private:
    QList<SpeakingEvalBatchReportService::StudentReport> m_reports;
    int m_currentStudentIndex = -1;
    Mode m_mode = Mode::SaveAs;
    QComboBox* m_scopeSelector = nullptr;
    QComboBox* m_rendererSelector = nullptr;
    QCheckBox* m_savePdfCheck = nullptr;
    QCheckBox* m_printReportsCheck = nullptr;
    QCheckBox* m_keepIndividualPdfsCheck = nullptr;
    QCheckBox* m_openOutputFolderCheck = nullptr;
    QLineEdit* m_outputDirectoryEdit = nullptr;
    QPushButton* m_chooseDirectoryButton = nullptr;
    QLabel* m_rendererNote = nullptr;
    QPushButton* m_previewButton = nullptr;
    QPushButton* m_exportButton = nullptr;
};
