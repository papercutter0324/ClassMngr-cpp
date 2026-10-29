#pragma once
#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include <QDate>
QString speakingEvalReportDate(const QDate&, SpeakingEvalReportTemplate);
QList<SpeakingEvalBatchReportService::StudentReport> buildSpeakingEvalStudentReports(
    const SpeakingEvalRows&, const ClassInfo&, const QByteArray& signatureImage = {});
int speakingEvalReportIndexForSourceRow(
    const QList<SpeakingEvalBatchReportService::StudentReport>&, int sourceRow);
