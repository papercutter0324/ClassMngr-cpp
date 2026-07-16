#pragma once

#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class SpeakingEvalBatchExportDialog : public QDialog
{
public:
    explicit SpeakingEvalBatchExportDialog(
        const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
        int currentStudentIndex,
        const QString& defaultOutputDirectory,
        QWidget* parent = nullptr
        );

private:
    void updateControls();

    void chooseOutputDirectory();

    void previewReports();

    void exportReports();

    [[nodiscard]] QList<SpeakingEvalBatchReportService::StudentReport>
    selectedReports() const;

    [[nodiscard]] SpeakingEvalBatchReportService::Renderer selectedRenderer() const;

private:
    QList<SpeakingEvalBatchReportService::StudentReport> m_reports;
    int m_currentStudentIndex = -1;
    QComboBox* m_scopeSelector = nullptr;
    QComboBox* m_rendererSelector = nullptr;
    QCheckBox* m_savePdfCheck = nullptr;
    QCheckBox* m_printReportsCheck = nullptr;
    QCheckBox* m_openOutputFolderCheck = nullptr;
    QLineEdit* m_outputDirectoryEdit = nullptr;
    QPushButton* m_chooseDirectoryButton = nullptr;
    QLabel* m_rendererNote = nullptr;
    QPushButton* m_previewButton = nullptr;
    QPushButton* m_exportButton = nullptr;
};
