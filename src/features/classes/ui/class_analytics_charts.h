#pragma once

#include <QColor>
#include <QList>
#include <QMap>
#include <QString>
#include <QWidget>

// QPainter-based, read-only chart widgets for the Class > Analytics tab.
//
// The grade palette follows the analytics mock-up:
//   A+ #1E9E4A   A #2E6FD0   B+ #C9A227   B #E8730C   C #BE2A2A
namespace AnalyticsCharts
{

// Grades in best-first order.  Every widget here renders in this order so
// that segments and bars always line up with the same meaning.
[[nodiscard]] QStringList gradeOrder();

[[nodiscard]] QColor gradeColor(const QString& grade);

} // namespace AnalyticsCharts

// "Class shape" histogram: one vertical bar per grade, best-first left to
// right, with the student count drawn above each bar.
class GradeHistogram : public QWidget
{
    Q_OBJECT

public:
    explicit GradeHistogram(QWidget* parent = nullptr);

    // distribution: grade letter -> student count.
    void setData(const QMap<QString, int>& distribution);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QMap<QString, int> m_distribution;
};

// A single horizontal, stacked bar showing one criterion's grade distribution
// coloured by grade, with the criterion label on the left and its average on
// the right.
class CriterionDistributionBar : public QWidget
{
    Q_OBJECT

public:
    explicit CriterionDistributionBar(QWidget* parent = nullptr);

    void setLabel(const QString& label);
    void setAverageText(const QString& averageText);
    void setData(const QMap<QString, int>& distribution);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_label;
    QString m_averageText;
    QMap<QString, int> m_distribution;
};