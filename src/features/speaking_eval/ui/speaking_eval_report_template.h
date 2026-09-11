#pragma once

#include "classmngr/engine/speaking_evaluation_report_template.h"

#include <QRectF>
#include <QSizeF>
#include <QString>

using SpeakingEvalReportTemplate =
    classmngr::engine::SpeakingEvaluationReportTemplate;

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
    const auto toQtLayout = [](
        const classmngr::engine::SpeakingEvaluationReportTemplatePolicy& policy
        )
    {
        return SpeakingEvalReportTemplateLayout{
            QSizeF(policy.pageWidth, policy.pageHeight),
            QRectF(
                policy.signatureBounds.left,
                policy.signatureBounds.top,
                policy.signatureBounds.width,
                policy.signatureBounds.height
                ),
            QString::fromUtf8(
                policy.powerPointResourcePath.data(),
                static_cast<qsizetype>(policy.powerPointResourcePath.size())
                ),
            policy.usesAdvancedScoreTable,
            policy.signatureAlignsBottomLeft
        };
    };

    static const SpeakingEvalReportTemplateLayout standard = toQtLayout(
        classmngr::engine::SpeakingEvaluationReportTemplateService::policy(
            classmngr::engine::SpeakingEvaluationReportTemplate::Standard
            )
        );
    static const SpeakingEvalReportTemplateLayout advanced = toQtLayout(
        classmngr::engine::SpeakingEvaluationReportTemplateService::policy(
            classmngr::engine::SpeakingEvaluationReportTemplate::Advanced
            )
        );

    switch (reportTemplate)
    {
    case SpeakingEvalReportTemplate::Advanced:
        return advanced;
    case SpeakingEvalReportTemplate::Standard:
    default:
        return standard;
    }
}
