#pragma once

#include "classmngr/engine/speaking_evaluation_report_content.h"

#include <QByteArray>
#include <QList>

#include <vector>

struct SpeakingEvalReportData;

namespace SpeakingEvalBatchReportService
{
struct StudentReport;
}

namespace SpeakingEvalReportContentAdapter
{
using ReportContent =
    classmngr::engine::SpeakingEvaluationReportContent;

[[nodiscard]] std::vector<ReportContent> toEngine(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports
    );

[[nodiscard]] SpeakingEvalReportData toQt(
    const ReportContent& content,
    const QByteArray& signatureImage
    );
}
