#include "speaking_eval_powerpoint_job_model.h"

#include "speaking_eval_batch_report_service.h"
#include "speaking_eval_report_data_assembler.h"

#include "core/resource_paths.h"
#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"

#include <QDir>
#include <QJsonArray>

#include <algorithm>

namespace SpeakingEvalPowerPointJobModel
{
QString normalizedText(
    const QString& value
    )
{
    // macOS input methods can supply Hangul as decomposed Jamo. PowerPoint
    // preserves that representation when text arrives through AppleScript,
    // causing names such as 박지혜 to be displayed as separate characters.
    return value.normalized(QString::NormalizationForm_C);
}

TemplateProfile templateProfile(
    SpeakingEvalReportTemplate reportTemplate
    )
{
    const SpeakingEvalReportTemplateLayout& layout =
        speakingEvalReportTemplateLayout(reportTemplate);

    TemplateProfile profile;
    profile.reportTemplate = reportTemplate;
    profile.resourcePath =
        ResourcePaths::Documents::filePath(
            layout.powerPointResourcePath
            );
    profile.signatureBounds = layout.signatureBounds;
    profile.signatureAlignsBottomLeft =
        layout.signatureAlignsBottomLeft;

    if (layout.usesAdvancedScoreTable)
    {
        profile.scoreTableOnMaster = false;
        profile.scoreTableName = QStringLiteral("Report_Table");
        profile.minimumTableColumns = 7;
        profile.firstGradeColumn = 3;
        profile.neutralFillRed = 229;
        profile.neutralFillGreen = 229;
        profile.neutralFillBlue = 231;
    }
    else
    {
        profile.scoreTableName = QStringLiteral("Grades & Scores");
    }

    return profile;
}

BatchJob build(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
    const QStringList& pdfPaths,
    const QString& workingDirectory
    )
{
    BatchJob batch;
    if (reports.isEmpty() || reports.size() != pdfPaths.size())
    {
        return batch;
    }

    batch.templateProfile =
        templateProfile(
            reports.constFirst().report.reportTemplate
            );
    batch.signatureImage =
        reports.constFirst().report.signatureImage;
    batch.students.reserve(reports.size());

    for (int index = 0; index < reports.size(); ++index)
    {
        const auto& student = reports.at(index);
        const SpeakingEvalReportData& data = student.report;

        StudentJob job;
        job.displayName = normalizedText(student.displayName);
        job.pdfPath = pdfPaths.at(index);
        job.completionPath =
            QDir(workingDirectory).filePath(
                QStringLiteral("completed-%1")
                    .arg(index, 6, 10, QLatin1Char('0'))
                );
        job.englishName = normalizedText(data.englishName);
        job.koreanName = normalizedText(data.koreanName);
        job.classLabel = normalizedText(data.classLabel);
        job.nativeTeacher = normalizedText(data.nativeTeacher);
        job.koreanTeacher = normalizedText(data.koreanTeacher);
        job.date = normalizedText(data.date);
        job.comments = normalizedText(data.comments);
        const SpeakingEvalFieldAsset* commentsField =
            speakingEvalFieldAsset(
                data.reportTemplate,
                QStringLiteral("comments")
                );
        if (commentsField)
        {
            job.commentsFontSizePoints =
                speakingEvalFittedFieldFontSize(
                    *commentsField,
                    job.comments,
                    1.0
                    );
        }
        job.overallGrade = normalizedText(
            SpeakingEvalReportDataAssembler::overallGrade(data.scores)
            );
        for (
            std::size_t scoreIndex = 0;
            scoreIndex < job.scores.size();
            ++scoreIndex
            )
        {
            job.scores[scoreIndex] =
                normalizedText(data.scores[scoreIndex]);
        }
        batch.students.append(job);
    }

    return batch;
}

QJsonObject toJson(
    const BatchJob& batch,
    const QString& pptxPath,
    const QString& signaturePath,
    const QString& cancelPath
    )
{
    QJsonArray students;
    for (const StudentJob& job : batch.students)
    {
        QJsonArray scores;
        for (const QString& score : job.scores)
        {
            scores.append(score);
        }

        students.append(
            QJsonObject{
                { QStringLiteral("displayName"), job.displayName },
                {
                    QStringLiteral("pdfPath"),
                    QDir::toNativeSeparators(job.pdfPath)
                },
                {
                    QStringLiteral("completionPath"),
                    QDir::toNativeSeparators(job.completionPath)
                },
                { QStringLiteral("englishName"), job.englishName },
                { QStringLiteral("koreanName"), job.koreanName },
                { QStringLiteral("classLabel"), job.classLabel },
                { QStringLiteral("nativeTeacher"), job.nativeTeacher },
                { QStringLiteral("koreanTeacher"), job.koreanTeacher },
                { QStringLiteral("date"), job.date },
                { QStringLiteral("comments"), job.comments },
                {
                    QStringLiteral("commentsFontName"),
                    QString::fromLatin1(CommentFontName)
                },
                {
                    QStringLiteral("commentsFontSizePoints"),
                    job.commentsFontSizePoints
                },
                { QStringLiteral("overallGrade"), job.overallGrade },
                { QStringLiteral("scores"), scores }
            }
            );
    }

    const TemplateProfile& profile = batch.templateProfile;
    return {
        {
            QStringLiteral("pptxPath"),
            QDir::toNativeSeparators(pptxPath)
        },
        {
            QStringLiteral("cancelPath"),
            QDir::toNativeSeparators(cancelPath)
        },
        { QStringLiteral("scoreTableOnMaster"), profile.scoreTableOnMaster },
        { QStringLiteral("scoreTableName"), profile.scoreTableName },
        { QStringLiteral("minimumTableRows"), profile.minimumTableRows },
        {
            QStringLiteral("minimumTableColumns"),
            profile.minimumTableColumns
        },
        { QStringLiteral("firstGradeColumn"), profile.firstGradeColumn },
        {
            QStringLiteral("neutralFill"),
            profile.neutralFillRed
                + (profile.neutralFillGreen << 8)
                + (profile.neutralFillBlue << 16)
        },
        {
            QStringLiteral("signaturePath"),
            QDir::toNativeSeparators(signaturePath)
        },
        { QStringLiteral("signatureLeft"), profile.signatureBounds.left() },
        { QStringLiteral("signatureTop"), profile.signatureBounds.top() },
        { QStringLiteral("signatureWidth"), profile.signatureBounds.width() },
        { QStringLiteral("signatureHeight"), profile.signatureBounds.height() },
        {
            QStringLiteral("signatureAlignsBottomLeft"),
            profile.signatureAlignsBottomLeft
        },
        { QStringLiteral("students"), students }
    };
}

bool usesSingleTemplate(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports
    )
{
    if (reports.isEmpty())
    {
        return true;
    }

    const SpeakingEvalReportTemplate reportTemplate =
        reports.constFirst().report.reportTemplate;
    return std::all_of(
        reports.cbegin(),
        reports.cend(),
        [reportTemplate](const auto& report)
        {
            return report.report.reportTemplate == reportTemplate;
        }
        );
}
}
