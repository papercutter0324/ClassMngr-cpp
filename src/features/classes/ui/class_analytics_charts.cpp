#include "features/classes/ui/class_analytics_charts.h"

#include "core/fontmanager.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QSizePolicy>

namespace
{

bool isDarkTheme(const QWidget* widget)
{
    return widget
        && widget->palette().color(QPalette::Window).lightness() < 128;
}

QColor mutedTextColor(const QWidget* widget)
{
    return isDarkTheme(widget)
        ? QColor(QStringLiteral("#a8b0bd"))
        : QColor(QStringLiteral("#66727a"));
}

} // namespace

namespace AnalyticsCharts
{

QStringList gradeOrder()
{
    return {
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C")
    };
}

QColor gradeColor(const QString& grade)
{
    if (grade == QLatin1String("A+"))
        return QColor(QStringLiteral("#159447"));
    if (grade == QLatin1String("A"))
        return QColor(QStringLiteral("#3f7ecb"));
    if (grade == QLatin1String("B+"))
        return QColor(QStringLiteral("#d7a316"));
    if (grade == QLatin1String("B"))
        return QColor(QStringLiteral("#ef5a13"));
    if (grade == QLatin1String("C"))
        return QColor(QStringLiteral("#bd1821"));
    return QColor(QStringLiteral("#8a8f98"));
}

} // namespace AnalyticsCharts

GradeHistogram::GradeHistogram(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void GradeHistogram::setData(const QMap<QString, int>& distribution)
{
    m_distribution = distribution;
    update();
}

QSize GradeHistogram::sizeHint() const
{
    return { 300, 260 };
}

void GradeHistogram::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QStringList grades = AnalyticsCharts::gradeOrder();
    int maximum = 1;
    for (const QString& grade : grades)
        maximum = qMax(maximum, m_distribution.value(grade));

    const QRectF area = QRectF(rect()).adjusted(18, 18, -18, -24);
    const qreal baseline = area.bottom() - 24.0;
    const qreal maxBarHeight = qMax<qreal>(1.0, area.height() - 58.0);
    const qreal slotWidth = area.width() / grades.size();
    const qreal barWidth = qMax<qreal>(18.0, slotWidth * 0.46);

    painter.setPen(QPen(
        isDarkTheme(this) ? QColor(255, 255, 255, 55) : QColor(0, 0, 0, 42),
        1.0));
    painter.drawLine(QPointF(area.left(), baseline), QPointF(area.right(), baseline));

    const QFont countFont = FontManager::getUiFont(11, QFont::DemiBold);
    const QFont labelFont = FontManager::getUiFont(12);
    for (qsizetype index = 0; index < grades.size(); ++index)
    {
        const QString& grade = grades.at(index);
        const int count = m_distribution.value(grade);
        const qreal center = area.left() + slotWidth * (index + 0.5);
        const qreal height = count == 0
            ? 0.0
            : maxBarHeight * count / maximum;
        const QRectF bar(center - barWidth / 2.0, baseline - height, barWidth, height);

        painter.setPen(Qt::NoPen);
        painter.setBrush(count > 0
            ? AnalyticsCharts::gradeColor(grade)
            : (isDarkTheme(this) ? QColor(255, 255, 255, 24)
                                  : QColor(0, 0, 0, 16)));
        painter.drawRoundedRect(bar, 4.0, 4.0);

        painter.setFont(countFont);
        painter.setPen(isDarkTheme(this)
            ? QColor(QStringLiteral("#f5f5f5"))
            : QColor(QStringLiteral("#31363b")));
        painter.drawText(
            QRectF(center - slotWidth / 2.0, baseline - height - 24.0,
                   slotWidth, 20.0),
            Qt::AlignCenter,
            QString::number(count));

        painter.setFont(labelFont);
        painter.setPen(mutedTextColor(this));
        painter.drawText(
            QRectF(center - slotWidth / 2.0, baseline + 6.0, slotWidth, 20.0),
            Qt::AlignCenter,
            grade);
    }
}

CriterionDistributionBar::CriterionDistributionBar(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(54);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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

void CriterionDistributionBar::setInsight(AnalyticsCharts::CriterionInsight insight)
{
    m_insight = insight;
    update();
}

QSize CriterionDistributionBar::sizeHint() const
{
    return { 520, 54 };
}

void CriterionDistributionBar::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    constexpr qreal labelWidth = 164.0;
    constexpr qreal rightPadding = 8.0;
    const QRectF labelArea(0.0, 1.0, labelWidth, 24.0);
    const QRectF averageArea(0.0, 27.0, labelWidth, 20.0);
    const QRectF bar(labelWidth + 8.0, 13.0,
                     qMax<qreal>(0.0, width() - labelWidth - rightPadding - 8.0),
                     26.0);

    QFont labelFont = FontManager::getUiFont(12, QFont::DemiBold);
    painter.setFont(labelFont);
    const QFontMetricsF labelMetrics(labelFont);
    painter.setPen(isDarkTheme(this)
        ? QColor(QStringLiteral("#f5f5f5"))
        : QColor(QStringLiteral("#24303a")));
    painter.drawText(
        labelArea,
        Qt::AlignLeft | Qt::AlignVCenter,
        labelMetrics.elidedText(m_label, Qt::ElideRight, labelArea.width()));

    if (m_insight != AnalyticsCharts::CriterionInsight::None)
    {
        const QString text = m_insight == AnalyticsCharts::CriterionInsight::Strongest
            ? tr("Strongest")
            : tr("Focus");
        const QColor color = m_insight == AnalyticsCharts::CriterionInsight::Strongest
            ? QColor(QStringLiteral("#159447"))
            : QColor(QStringLiteral("#c07b08"));
        const qreal badgeWidth = QFontMetricsF(FontManager::getUiFont(9))
            .horizontalAdvance(text) + 14.0;
        const qreal labelTextWidth = labelMetrics.horizontalAdvance(m_label);
        const QRectF badge(
            qMin(labelArea.right() - badgeWidth, labelTextWidth + 7.0),
            4.0,
            badgeWidth,
            17.0);
        painter.setPen(QPen(color, 1.0));
        painter.setBrush(color.lighter(isDarkTheme(this) ? 120 : 185));
        painter.drawRoundedRect(badge, 8.5, 8.5);
        painter.setFont(FontManager::getUiFont(9));
        painter.setPen(Qt::white);
        painter.drawText(badge, Qt::AlignCenter, text);
    }

    painter.setFont(FontManager::getUiFont(10));
    painter.setPen(mutedTextColor(this));
    painter.drawText(averageArea, Qt::AlignLeft | Qt::AlignVCenter, m_averageText);

    int total = 0;
    for (const int count : m_distribution)
        total += count;

    painter.setPen(Qt::NoPen);
    painter.setBrush(isDarkTheme(this) ? QColor(255, 255, 255, 34)
                                       : QColor(0, 0, 0, 20));
    painter.drawRoundedRect(bar, 5.0, 5.0);
    if (total == 0 || bar.width() <= 0.0)
        return;

    QPainterPath clip;
    clip.addRoundedRect(bar, 5.0, 5.0);
    painter.save();
    painter.setClipPath(clip);

    const QFont countFont = FontManager::getUiFont(11, QFont::DemiBold);
    const QFontMetricsF countMetrics(countFont);
    qreal x = bar.left();
    for (const QString& grade : AnalyticsCharts::gradeOrder())
    {
        const int count = m_distribution.value(grade);
        if (count <= 0)
            continue;

        const qreal width = bar.width() * count / total;
        const QRectF segment(x, bar.top(), width, bar.height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(AnalyticsCharts::gradeColor(grade));
        painter.drawRect(segment);

        const QString text = QString::number(count);
        if (countMetrics.horizontalAdvance(text) + 8.0 <= width)
        {
            painter.setFont(countFont);
            painter.setPen(Qt::white);
            painter.drawText(segment, Qt::AlignCenter, text);
        }
        x += width;
    }
    painter.restore();
}
