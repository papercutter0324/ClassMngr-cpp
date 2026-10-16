#pragma once

#include "features/speaking_eval/services/speaking_eval_report_data.h"

#include <QImage>
#include <QRectF>

class QPainter;

// Synchronous painting with the caller's QGuiApplication/font setup and template
// asset lifetime. The per-instance score cache and shared assets are not thread safe.
class SpeakingEvalReportPainter final
{
public:
    void paint(
        QPainter* painter,
        const QRectF& targetRect,
        const SpeakingEvalReportData& data
        ) const;

private:
    mutable QImage m_compositedScoreBackground;
    mutable std::array<QString, 6> m_compositedScoreBackgroundScores;
    mutable SpeakingEvalReportTemplate m_compositedScoreBackgroundTemplate =
        SpeakingEvalReportTemplate::Standard;
};
