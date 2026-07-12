#pragma once

#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/ui/speaking_eval_batch_report_service.h"
#include "features/speaking_eval/ui/speaking_eval_report_widget.h"

#include <QDialog>

class QComboBox;

[[nodiscard]] QList<SpeakingEvalBatchReportService::StudentReport>
buildSpeakingEvalStudentReports(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo
    );

class SpeakingEvalReportDialog : public QDialog
{
public:
    explicit SpeakingEvalReportDialog(
        const SpeakingEvalRows& rows,
        const ClassInfo& classInfo,
        QWidget* parent = nullptr
        );

    explicit SpeakingEvalReportDialog(
        const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
        int currentStudentIndex,
        QWidget* parent = nullptr
        );

private:
    void updateReport();

    void moveToPreviousStudent();

    void moveToNextStudent();

private:
    QList<SpeakingEvalBatchReportService::StudentReport> m_reports;
    QComboBox* m_studentSelector = nullptr;
    SpeakingEvalReportWidget* m_report = nullptr;
};
