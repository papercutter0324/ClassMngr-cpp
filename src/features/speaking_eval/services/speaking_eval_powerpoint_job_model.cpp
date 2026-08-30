#include "speaking_eval_powerpoint_job_model.h"

#include "speaking_eval_batch_report_service.h"

#include "classmngr/engine/speaking_evaluation_powerpoint_job_service.h"
#include "classmngr/engine/speaking_evaluation_report_template.h"

#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"

#include <QDir>
#include <QJsonArray>

#include <cstddef>
#include <cstdint>
#include <vector>

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
    SpeakingEvalReportTemplate reportTemplate,
    const QString& documentsRoot
    )
{
    const classmngr::engine::SpeakingEvaluationReportTemplatePolicy& policy =
        classmngr::engine::SpeakingEvaluationReportTemplateService::policy(
            reportTemplate
            );

    TemplateProfile profile;
    profile.reportTemplate = reportTemplate;
    profile.resourcePath = QDir(documentsRoot).filePath(
        QString::fromUtf8(
            policy.powerPointResourcePath.data(),
            static_cast<qsizetype>(policy.powerPointResourcePath.size())
            )
        );
    profile.signatureBounds = QRectF(
        policy.signatureBounds.left,
        policy.signatureBounds.top,
        policy.signatureBounds.width,
        policy.signatureBounds.height
        );
    profile.signatureAlignsBottomLeft = policy.signatureAlignsBottomLeft;
    profile.scoreTableOnMaster = policy.scoreTableOnMaster;
    profile.scoreTableName = QString::fromUtf8(
        policy.scoreTableName.data(),
        static_cast<qsizetype>(policy.scoreTableName.size())
        );
    profile.minimumTableRows = policy.minimumTableRows;
    profile.minimumTableColumns = policy.minimumTableColumns;
    profile.firstGradeColumn = policy.firstGradeColumn;
    profile.neutralFillRed = policy.neutralFillRed;
    profile.neutralFillGreen = policy.neutralFillGreen;
    profile.neutralFillBlue = policy.neutralFillBlue;

    return profile;
}

classmngr::engine::SpeakingEvaluationReportContent
portableReportContent(
    const SpeakingEvalBatchReportService::StudentReport& source
    )
{
    const SpeakingEvalReportData& data = source.report;
    classmngr::engine::SpeakingEvaluationReportContent portable;
    portable.displayName = source.displayName.toStdString();
    portable.englishName = data.englishName.toStdString();
    portable.koreanName = data.koreanName.toStdString();
    portable.classLabel = data.classLabel.toStdString();
    portable.nativeTeacher = data.nativeTeacher.toStdString();
    portable.koreanTeacher = data.koreanTeacher.toStdString();
    portable.date = data.date.toStdString();
    portable.comments = data.comments.toStdString();
    portable.notes = data.notes.toStdString();
    portable.grade = data.grade;
    for (std::size_t index = 0; index < portable.scores.size(); ++index)
    {
        portable.scores[index] = data.scores[index].toStdString();
    }
    portable.reportTemplate = data.reportTemplate;
    portable.sourceRow = source.sourceRow;
    return portable;
}

std::vector<std::uint8_t> portableSignatureImage(
    const QByteArray& signatureImage
    )
{
    if (signatureImage.isEmpty())
    {
        return {};
    }

    const auto* begin = reinterpret_cast<const std::uint8_t*>(
        signatureImage.constData()
        );
    return {
        begin,
        begin + static_cast<std::size_t>(signatureImage.size())
    };
}

BatchJob build(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
    const QStringList& pdfPaths,
    const QString& workingDirectory,
    const QString& documentsRoot
    )
{
    BatchJob batch;
    if (reports.isEmpty() || reports.size() != pdfPaths.size())
    {
        return batch;
    }

    classmngr::engine::SpeakingEvaluationPowerPointJobRequest request;
    request.reports.reserve(static_cast<std::size_t>(reports.size()));
    request.pdfPaths.reserve(static_cast<std::size_t>(pdfPaths.size()));
    request.completionPaths.reserve(static_cast<std::size_t>(reports.size()));
    for (int index = 0; index < reports.size(); ++index)
    {
        request.reports.push_back(portableReportContent(reports.at(index)));
        request.pdfPaths.push_back(pdfPaths.at(index).toStdString());
        request.completionPaths.push_back(
            QDir(workingDirectory).filePath(
                QStringLiteral("completed-%1")
                    .arg(index, 6, 10, QLatin1Char('0'))
                ).toStdString()
            );
    }
    request.signatureImage = portableSignatureImage(
        reports.constFirst().report.signatureImage
        );

    const auto portableJobResult =
        classmngr::engine::SpeakingEvaluationPowerPointJobService::build(
            request
            );
    if (!portableJobResult)
    {
        return batch;
    }

    const classmngr::engine::SpeakingEvaluationPowerPointJob& portableJob =
        *portableJobResult;
    batch.templateProfile =
        templateProfile(
            portableJob.reportTemplate,
            documentsRoot
            );
    if (!portableJob.signatureImage.empty())
    {
        batch.signatureImage = QByteArray(
            reinterpret_cast<const char*>(portableJob.signatureImage.data()),
            static_cast<qsizetype>(portableJob.signatureImage.size())
            );
    }
    batch.students.reserve(static_cast<qsizetype>(portableJob.students.size()));

    const SpeakingEvalFieldAsset* commentsField =
        speakingEvalFieldAsset(
            portableJob.reportTemplate,
            QStringLiteral("comments")
            );
    for (
        const classmngr::engine::SpeakingEvaluationPowerPointStudentJob& source :
        portableJob.students
        )
    {
        StudentJob job;
        job.displayName = normalizedText(
            QString::fromStdString(source.displayName)
            );
        job.pdfPath = QString::fromStdString(source.pdfPath);
        job.completionPath = QString::fromStdString(source.completionPath);
        job.englishName = normalizedText(
            QString::fromStdString(source.englishName)
            );
        job.koreanName = normalizedText(
            QString::fromStdString(source.koreanName)
            );
        job.classLabel = normalizedText(
            QString::fromStdString(source.classLabel)
            );
        job.nativeTeacher = normalizedText(
            QString::fromStdString(source.nativeTeacher)
            );
        job.koreanTeacher = normalizedText(
            QString::fromStdString(source.koreanTeacher)
            );
        job.date = normalizedText(QString::fromStdString(source.date));
        job.comments = normalizedText(
            QString::fromStdString(source.comments)
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
            QString::fromStdString(source.overallGrade)
            );
        for (std::size_t index = 0; index < job.scores.size(); ++index)
        {
            job.scores[index] = normalizedText(
                QString::fromStdString(source.scores[index])
                );
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

}
