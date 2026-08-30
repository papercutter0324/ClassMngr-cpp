#pragma once

#include "features/speaking_eval/ui/speaking_eval_report_template.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <array>

namespace SpeakingEvalBatchReportService
{
struct StudentReport;
}

namespace SpeakingEvalPowerPointJobModel
{
inline constexpr auto CommentFontName = "Segoe UI Semibold";

struct TemplateProfile
{
    SpeakingEvalReportTemplate reportTemplate =
        SpeakingEvalReportTemplate::Standard;
    QString resourcePath;
    QRectF signatureBounds;
    bool signatureAlignsBottomLeft = false;
    bool scoreTableOnMaster = true;
    QString scoreTableName;
    // The path is resolved while the caller owns a documents-pack lease.
    // It must be consumed before that lease is released.
    int minimumTableRows = 12;
    int minimumTableColumns = 6;
    int firstGradeColumn = 2;
    int neutralFillRed = 217;
    int neutralFillGreen = 217;
    int neutralFillBlue = 217;
};

struct StudentJob
{
    QString displayName;
    QString pdfPath;
    QString completionPath;
    QString englishName;
    QString koreanName;
    QString classLabel;
    QString nativeTeacher;
    QString koreanTeacher;
    QString date;
    QString comments;
    qreal commentsFontSizePoints = 0.0;
    QString overallGrade;
    std::array<QString, 6> scores;
};

struct BatchJob
{
    TemplateProfile templateProfile;
    QByteArray signatureImage;
    QList<StudentJob> students;
};

[[nodiscard]] QString normalizedText(
    const QString& value
    );

[[nodiscard]] BatchJob build(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
    const QStringList& pdfPaths,
    const QString& workingDirectory,
    const QString& documentsRoot
    );

[[nodiscard]] QJsonObject toJson(
    const BatchJob& batch,
    const QString& pptxPath,
    const QString& signaturePath,
    const QString& cancelPath
    );

}
