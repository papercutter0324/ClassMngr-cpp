#include "features/classes/ui/class_analytics_charts.h"

#include "core/fontmanager.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QRectF>
#include <QSize>

namespace
{

// Mirrors the dark-theme check used by the schedule widgets: the palette
// follows the application theme, so this stays correct on theme switches
// (ThemeService repaints every widget after applying a theme).
[[nodiscard]] bool isDarkTheme(
    QWidget* widget
    )
{
    return widget
        && widget->palette()
               .color(QPalette::Window)
               .lightness() < 128;
}

} // namespace

namespace AnalyticsCharts
{

QStringList gradeOrder()
{
    return { QStringLiteral("A+"),
             QStringLiteral("A"),
             QStringLiteral("B+"),
             QStringLiteral("B"),
             QStringLiteral("C") };
}

QColor gradeColor(const QString& grade)
{
    // Palette taken from the analytics mock-up.
    if (grade == QLatin1String("A+"))
        return QColor(QStringLiteral("#1E9E4A"));
    if (grade == QLatin1String("A"))
        return QColor(QStringLiteral("#2E6FD0"));
    if (grade == QLatin1String("B+"))
        return QColor(QStringLiteral("#C9A227"));
    if (grade == QLatin1String("B"))
        return QColor(QStringLiteral("#E8730C"));
    if (grade == QLatin1String("C"))
        return QColor(QStringLiteral("#BE2A2A"));
    return QColor(QStringLiteral("#8A8F98"));
}

} // namespace AnalyticsCharts

GradeHistogram::GradeHistogram(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(160);
}

void GradeHistogram::setData(const QMap<QString, int>& distribution)
{
    m_distribution = distribution;
    update();
}

QSize GradeHistogram::sizeHint() const
{
    return QSize(460, 180);
}

void GradeHistogram::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QStringList order = AnalyticsCharts::gradeOrder();
    int maxCount = 1;
    for (const QString& grade : order)
        maxCount = qMax(maxCount, m_distribution.value(grade, 0));

    const QRectF frame = QRectF(rect()).adjusted(10, 12, -10, -10);
    const int n = qMax(1, order.size());
    const double slot = frame.width() / n;
    const double barW = slot * 0.55;
    const double barOffset = (slot - barW) / 2.0;

    const double baselineY = frame.bottom() - 24;
    const double maxBarH = frame.height() - 48;

    const QFont countFont = FontManager::getUiFont(11);
    const QFont gradeFont = FontManager::getUiFont(12);

    // Pick light/dark theme colors; the light values are unchanged from the
    // original hard-coded palette so the light theme keeps its look.
    const bool dark = isDarkTheme(this);

    painter.setPen(
        QPen(dark ? QColor(255, 255, 255, 46) : QColor(0, 0, 0, 40), 1.0)
        );
    painter.drawLine(
        QPointF(frame.left(), baselineY),
        QPointF(frame.right(), baselineY)
        );

    for (int i = 0; i < order.size(); ++i)
    {
        const QString grade = order.at(i);
        const int count = m_distribution.value(grade, 0);
        const double x = frame.left() + i * slot + barOffset;
        const double h = (count > 0) ? maxBarH * (count / double(maxCount)) : 0.0;
        const QRectF bar(x, baselineY - h, barW, h);

        painter.setPen(Qt::NoPen);
        painter.setBrush((count > 0)
                             ? AnalyticsCharts::gradeColor(grade)
                             : (dark ? QColor(255, 255, 255, 30)
                                    : QColor(0, 0, 0, 20)));
        painter.drawRect(bar);

        if (count > 0)
        {
            painter.setFont(countFont);
            painter.setPen(dark ? QColor(216, 220, 226) : QColor(70, 70, 70));
            painter.drawText(
                QRectF(x - 8, baselineY - h - 20, barW + 16, 18),
                Qt::AlignHCenter | Qt::AlignBottom,
                QString::number(count)
                );
        }

        painter.setFont(gradeFont);
        painter.setPen(dark ? QColor(168, 176, 189) : QColor(95, 95, 95));
        painter.drawText(
            QRectF(x - 8, baselineY + 4, barW + 16, 18),
            Qt::AlignHCenter | Qt::AlignTop,
            grade
            );
    }
}


CriterionDistributionBar::CriterionDistributionBar(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(34);
}

void CriterionDistributionBar::setLabel(const QString& label)
{
    m_label = label;
    update();
}

void CriterionDistributionBar::setAverageText(const QString& averageText)
{
    m_averageText = averageText;
    update();
}

void CriterionDistributionBar::setData(const QMap<QString, int>& distribution)
{
    m_distribution = distribution;
    update();
}

QSize CriterionDistributionBar::sizeHint() const
{
    return QSize(420, 40);
}

void CriterionDistributionBar::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Pick light/dark theme colors; the light values are unchanged from the
    // original hard-coded palette so the light theme keeps its look.
    const bool dark = isDarkTheme(this);

    const int h = height();
    const int labelW = 120;
    const int avgW = 60;

    // label (left)
    painter.setPen(dark ? QColor(229, 233, 238) : QColor(55, 55, 55));
    QFont labelFont = FontManager::getUiFont(11);
    painter.setFont(labelFont);
    const QFontMetricsF lf(labelFont);
    const QString trimmedLabel = lf.elidedText(m_label, Qt::ElideRight, labelW);
    painter.drawText(
        QRectF(0, 0, labelW, h),
        Qt::AlignVCenter | Qt::AlignLeft,
        trimmedLabel
        );

    // average (right)
    QFont avgFont = FontManager::getUiFont(12);
    avgFont.setBold(true);
    painter.setFont(avgFont);
    painter.setPen(dark ? QColor(245, 245, 245) : QColor(30, 30, 30));
    painter.drawText(
        QRectF(width() - avgW, 0, avgW, h),
        Qt::AlignVCenter | Qt::AlignRight,
        m_averageText
        );

    const int barX = labelW + 12;
    const int barW = qMax(0, width() - barX - avgW - 12);
    // Bar is 4px taller than the 12px average font used by this row.
    const int barH = 16;
    const int barY = (h - barH) / 2;
    const QRectF bar(barX, barY, barW, barH);

    int total = 0;
    for (const int v : m_distribution.values())
        total += v;

    // track background
    painter.setPen(Qt::NoPen);
    painter.setBrush(dark ? QColor(255, 255, 255, 34) : QColor(0, 0, 0, 22));
    painter.drawRoundedRect(bar, barH / 2.0, barH / 2.0);

    if (total > 0 && barW > 0)
    {
        // Clip to the rounded track so segments inherit the rounded ends.
        QPainterPath clipPath;
        clipPath.addRoundedRect(bar, barH / 2.0, barH / 2.0);
        painter.save();
        painter.setClipPath(clipPath);

        QFont countFont = FontManager::getUiFont(11);
        const QFontMetricsF countMetrics(countFont);
        double x = bar.left();
        for (const QString& grade : AnalyticsCharts::gradeOrder())
        {
            const int count = m_distribution.value(grade, 0);
            if (count <= 0)
                continue;
            const double segW = barW * (count / double(total));
            painter.setPen(Qt::NoPen);
            painter.setBrush(AnalyticsCharts::gradeColor(grade));
            painter.drawRect(QRectF(x, barY, segW, barH));

            // Student count, centered in the segment, white in every theme.
            const QString countText = QString::number(count);
            if (countMetrics.horizontalAdvance(countText) + 4.0 <= segW)
            {
                painter.setFont(countFont);
                painter.setPen(Qt::white);
                painter.drawText(
                    QRectF(x, barY, segW, barH),
                    Qt::AlignCenter,
                    countText
                    );
            }
            x += segW;
        }
        painter.restore();
    }
}