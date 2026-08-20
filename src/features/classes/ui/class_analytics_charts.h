#pragma once

#include "features/classes/services/speaking_analytics.h"

#include <QColor>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

namespace AnalyticsCharts
{

enum class CriterionInsight
{
    None,
    Strongest,
    Focus
};

[[nodiscard]] QStringList gradeOrder();
[[nodiscard]] QColor gradeColor(const QString& grade);

} // namespace AnalyticsCharts

// Compact histogram for the overall grade distribution.
class GradeHistogram : public QWidget
{
    Q_OBJECT

public:
    explicit GradeHistogram(QWidget* parent = nullptr);

    void setData(const QMap<QString, int>& distribution);
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QMap<QString, int> m_distribution;
};

// Compact 1–5 trend chart for completed evaluation averages.  It has no
// student-count concept: every point is identified by its evaluation and
// grade/one-decimal class average.
class YearToDateChart : public QWidget
{
    Q_OBJECT

public:
    explicit YearToDateChart(QWidget* parent = nullptr);

    void setData(const QList<SpeakingAnalytics::YearToDatePoint>& points);
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QList<SpeakingAnalytics::YearToDatePoint> m_points;
};

// One criterion row: label, rounded grade/average, optional insight badge,
// and a grade-coloured distribution bar.
class CriterionDistributionBar : public QWidget
{
    Q_OBJECT

public:
    explicit CriterionDistributionBar(QWidget* parent = nullptr);

    void setLabel(const QString& label);
    void setAverageText(const QString& averageText);
    void setData(const QMap<QString, int>& distribution);
    void setInsight(AnalyticsCharts::CriterionInsight insight);
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_label;
    QString m_averageText;
    QMap<QString, int> m_distribution;
    AnalyticsCharts::CriterionInsight m_insight =
        AnalyticsCharts::CriterionInsight::None;
};
