#pragma once

#include <QRectF>
#include <QSizeF>
#include <QString>

enum class SpeakingEvalReportTemplate
{
    Standard,
    Advanced
};

struct SpeakingEvalReportTemplateLayout
{
    QSizeF pageSize;
    QRectF signatureBounds;
    QString powerPointResourcePath;
    bool usesAdvancedScoreTable = false;
    bool signatureAlignsBottomLeft = false;
};

[[nodiscard]] inline const SpeakingEvalReportTemplateLayout&
speakingEvalReportTemplateLayout(
    SpeakingEvalReportTemplate reportTemplate
    )
{
    static const SpeakingEvalReportTemplateLayout standard{
        QSizeF(540.0, 780.0),
        QRectF(384.0, 722.0, 120.0, 36.0),
        QStringLiteral(
            "Speaking Evaluations/SpeakingEvaluationTemplate-Full.pptx"
            ),
        false
    };
    static const SpeakingEvalReportTemplateLayout advanced{
        QSizeF(540.0, 780.0),
        QRectF(408.5, 746.25, 114.0, 24.75),
        QStringLiteral(
            "Speaking Evaluations/SpeakingEvaluationTemplate_Advanced-Full.pptx"
            ),
        true,
        true
    };

    switch (reportTemplate)
    {
    case SpeakingEvalReportTemplate::Advanced:
        return advanced;
    case SpeakingEvalReportTemplate::Standard:
    default:
        return standard;
    }
}
