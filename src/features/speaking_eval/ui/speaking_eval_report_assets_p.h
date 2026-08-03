#pragma once

#include "features/speaking_eval/ui/speaking_eval_report_template.h"

#include <QFont>
#include <QHash>
#include <QImage>
#include <QMarginsF>
#include <QColor>
#include <QRect>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

struct SpeakingEvalFieldAsset
{
    // All geometry and typography values use the 540 x 780 point map where
    // one map unit is one typographic point (1/72 inch).
    QRectF rect;
    QMarginsF margins;
    QString fontRole;
    qreal fontSizePoints = 0.0;
    qreal minimumScale = 0.7;
    qreal letterSpacing = 0.0;
    qreal wordSpacing = 0.0;
    qreal lineHeight = 1.0;
    qreal baselineOffset = 0.0;
    Qt::Alignment horizontalAlignment = Qt::AlignLeft;
    Qt::Alignment verticalAlignment = Qt::AlignTop;
    bool comments = false;
};

struct SpeakingEvalSpriteAsset
{
    // source is measured in sprite-sheet pixels; destination and pointSize
    // are measured in report-map points.
    QRect source;
    QRectF destination;
    QSizeF pointSize;
};

struct SpeakingEvalTemplateAssets
{
    bool valid = false;
    QString error;
    // logicalSize is the physical page expressed in 1/72-inch points.
    QSizeF logicalSize;
    QSize backgroundPixelSize;
    QImage background;
    QImage sprites;
    QHash<QString, SpeakingEvalFieldAsset> fields;
    QVector<QHash<QString, QRectF>> scoreCells;
    // Advanced reports repeat each selected score in the grading column.
    // These body cells have individually authored heights.
    QVector<QRectF> studentGradeCells;
    QHash<QString, QImage> studentGrades;
    QVector<QHash<QString, QRectF>> studentGradeRects;
    QHash<QString, SpeakingEvalSpriteAsset> scoreLabels;
    // Highlighted grades are authored at each background's native resolution.
    // Their destinations are snapped to that same pixel grid so the yellow
    // artwork replaces only the gray cell interior.
    QHash<QString, QImage> scoreHighlights;
    QVector<QHash<QString, QRectF>> scoreHighlightRects;
    QColor scoreHighlightColor = QColor(QStringLiteral("#FFFF00"));
    qreal scoreHighlightInset = 1.0;
    QRectF overallGradeBounds;
    QHash<QString, SpeakingEvalSpriteAsset> overallGrades;
    QRectF signatureBounds;
};

[[nodiscard]] const SpeakingEvalTemplateAssets&
speakingEvalTemplateAssets(
    SpeakingEvalReportTemplate reportTemplate
    );

[[nodiscard]] const SpeakingEvalFieldAsset*
speakingEvalFieldAsset(
    SpeakingEvalReportTemplate reportTemplate,
    const QString& fieldName
    );

[[nodiscard]] QRectF speakingEvalScoreCell(
    SpeakingEvalReportTemplate reportTemplate,
    int metricIndex,
    const QString& score
    );

[[nodiscard]] QFont speakingEvalTemplateFont(
    const QString& role,
    qreal pointSize
    );
