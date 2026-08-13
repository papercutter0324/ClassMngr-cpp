#pragma once

#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include "ui/shared/dialogs/dialog_shell.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

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
