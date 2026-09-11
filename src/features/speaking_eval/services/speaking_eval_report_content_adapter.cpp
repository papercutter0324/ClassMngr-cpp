#include "speaking_eval_report_content_adapter.h"

#include "speaking_eval_batch_report_service.h"
#include "features/speaking_eval/ui/speaking_eval_report_widget.h"

#include <cstddef>
#include <utility>

namespace SpeakingEvalReportContentAdapter
{
std::vector<ReportContent> toEngine(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports
    )
{
    std::vector<ReportContent> contents;
    contents.reserve(static_cast<std::size_t>(reports.size()));
    for (const SpeakingEvalBatchReportService::StudentReport& source : reports)
    {
        const SpeakingEvalReportData& data = source.report;
        ReportContent content;
        content.displayName = source.displayName.toStdString();
        content.englishName = data.englishName.toStdString();
        content.koreanName = data.koreanName.toStdString();
        content.classLabel = data.classLabel.toStdString();
        content.nativeTeacher = data.nativeTeacher.toStdString();
        content.koreanTeacher = data.koreanTeacher.toStdString();
        content.date = data.date.toStdString();
        content.comments = data.comments.toStdString();
        content.notes = data.notes.toStdString();
        content.grade = data.grade;
        for (std::size_t index = 0; index < content.scores.size(); ++index)
        {
            content.scores[index] = data.scores[index].toStdString();
        }
        content.reportTemplate = data.reportTemplate;
        content.sourceRow = source.sourceRow;
        contents.push_back(std::move(content));
    }
    return contents;
}

SpeakingEvalReportData toQt(
    const ReportContent& content,
    const QByteArray& signatureImage
    )
{
    SpeakingEvalReportData data;
    data.englishName = QString::fromStdString(content.englishName);
    data.koreanName = QString::fromStdString(content.koreanName);
    data.classLabel = QString::fromStdString(content.classLabel);
    data.nativeTeacher = QString::fromStdString(content.nativeTeacher);
    data.koreanTeacher = QString::fromStdString(content.koreanTeacher);
    data.date = QString::fromStdString(content.date);
    data.comments = QString::fromStdString(content.comments);
    data.notes = QString::fromStdString(content.notes);
    data.grade = content.grade;
    for (std::size_t index = 0; index < data.scores.size(); ++index)
    {
        data.scores[index] = QString::fromStdString(content.scores[index]);
    }
    data.signatureImage = signatureImage;
    data.reportTemplate = content.reportTemplate;
    return data;
}
} // namespace SpeakingEvalReportContentAdapter
