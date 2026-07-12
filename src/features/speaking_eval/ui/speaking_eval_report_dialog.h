#pragma once

#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/ui/speaking_eval_batch_report_service.h"
#include "features/speaking_eval/ui/speaking_eval_report_widget.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QPlainTextEdit;

[[nodiscard]] QList<SpeakingEvalBatchReportService::StudentReport>
buildSpeakingEvalStudentReports(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo
    );

class SpeakingEvalReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpeakingEvalReportDialog(
        const SpeakingEvalRows& rows,
        const ClassInfo& classInfo,
        QWidget* parent = nullptr
        );

    explicit SpeakingEvalReportDialog(
        const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
        int currentStudentIndex,
        QWidget* parent = nullptr,
        bool interactive = false
        );

signals:
    void reportValueEdited(
        int sourceRow,
        SpeakingEvalColumn column,
        const QString& value
        );

private:
    void updateReport();

    void moveToPreviousStudent();

    void moveToNextStudent();

private:
    QList<SpeakingEvalBatchReportService::StudentReport> m_reports;
    QComboBox* m_studentSelector = nullptr;
    QLabel* m_notesLabel = nullptr;
    QPlainTextEdit* m_notesEdit = nullptr;
    SpeakingEvalReportWidget* m_report = nullptr;
    bool m_interactive = false;
};
