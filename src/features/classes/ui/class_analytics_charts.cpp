#include "features/classes/ui/class_analytics_charts.h"

#include "core/fontmanager.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QSizePolicy>

#include <algorithm>

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
    setObjectName(QStringLiteral("analyticsGradeHistogram"));
    setMinimumHeight(165);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void GradeHistogram::setData(const QMap<QString, int>& distribution)
{
    m_distribution = distribution;
    update();
}

QSize GradeHistogram::sizeHint() const
{
    return { 300, 195 };
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

YearToDateChart::YearToDateChart(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("analyticsYearToDateChart"));
    setMinimumHeight(190);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void YearToDateChart::setData(
    const QList<SpeakingAnalytics::YearToDatePoint>& points
)
{
    m_points = points;
    update();
}

QSize YearToDateChart::sizeHint() const
{
    return { 300, 190 };
}

void YearToDateChart::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor foreground = isDarkTheme(this)
        ? QColor(QStringLiteral("#f5f5f5"))
        : QColor(QStringLiteral("#31363b"));
    const QColor grid = isDarkTheme(this)
        ? QColor(255, 255, 255, 45)
        : QColor(0, 0, 0, 34);
    const QColor lineColor = QColor(QStringLiteral("#3f7ecb"));

    // Keep each term in a stable position, even when it has no completed
    // evaluation yet.  This prevents the trend from visually re-scaling as
    // the year progresses.
    const QStringList evaluations = SpeakingAnalytics::evaluationNames();

    constexpr qreal leftMargin = 42.0;
    constexpr qreal rightMargin = 12.0;
    constexpr qreal topMargin = 28.0;
    constexpr qreal bottomMargin = 62.0;
    const QRectF plot = QRectF(rect()).adjusted(
        leftMargin, topMargin, -rightMargin, -bottomMargin);
    if (plot.width() <= 1.0 || plot.height() <= 1.0)
        return;

    const QList<QString> grades{
        QStringLiteral("C"),
        QStringLiteral("B"),
        QStringLiteral("B+"),
        QStringLiteral("A"),
        QStringLiteral("A+")
    };
    painter.setFont(FontManager::getUiFont(10));
    for (int grade = 1; grade <= 5; ++grade)
    {
        const qreal y = plot.bottom()
            - (grade - 1) * plot.height() / 4.0;
        painter.setPen(QPen(grid, 1.0));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(mutedTextColor(this));
        painter.drawText(
            QRectF(0.0, y - 9.0, leftMargin - 8.0, 18.0),
            Qt::AlignCenter,
            grades.at(grade - 1));
    }

    const int evaluationCount = evaluations.size();
    if (evaluationCount == 0)
        return;

    const qreal slotWidth = evaluationCount == 1
        ? plot.width()
        : plot.width() / (evaluationCount - 1);
    const QFont axisLabelFont = FontManager::getUiFont(9);
    const QFontMetricsF axisLabelMetrics(axisLabelFont);
    const auto slotCenter = [&plot, evaluationCount, slotWidth](int index)
    {
        return evaluationCount == 1
            ? plot.center().x()
            : plot.left() + index * slotWidth;
    };
    const auto pointPosition = [&plot,
                                &evaluations,
                                axisLabelMetrics,
                                evaluationCount,
                                slotCenter](int index, double average)
    {
        qreal x = slotCenter(index);
        if (evaluationCount > 1 && index == 0)
        {
            x = plot.left() + axisLabelMetrics.horizontalAdvance(
                evaluations.at(index)) / 2.0;
        }
        else if (evaluationCount > 1 && index == evaluationCount - 1)
        {
            x = plot.right() - axisLabelMetrics.horizontalAdvance(
                evaluations.at(index)) / 2.0;
        }
        const qreal boundedAverage = qBound(1.0, average, 5.0);
        const qreal y = plot.bottom()
            - (boundedAverage - 1.0) * plot.height() / 4.0;
        return QPointF(x, y);
    };

    QPainterPath trend;
    bool previousPointPresent = false;
    bool hasLineSegment = false;
    for (int index = 0; index < evaluationCount; ++index)
    {
        const auto pointIt = std::find_if(
            m_points.cbegin(), m_points.cend(),
            [&evaluations, index](
                const SpeakingAnalytics::YearToDatePoint& point)
            {
                return point.evaluationName == evaluations.at(index);
            });
        if (pointIt == m_points.cend())
        {
            previousPointPresent = false;
            continue;
        }

        const QPointF point = pointPosition(index, pointIt->classAverage3);
        if (!previousPointPresent)
            trend.moveTo(point);
        else
        {
            trend.lineTo(point);
            hasLineSegment = true;
        }
        previousPointPresent = true;
    }
    if (hasLineSegment)
    {
        painter.setPen(QPen(lineColor, 2.4, Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(trend);
    }

    const QFont valueFont = FontManager::getUiFont(10, QFont::DemiBold);
    const QFont labelFont = FontManager::getUiFont(10);
    for (int index = 0; index < evaluationCount; ++index)
    {
        const QString& evaluation = evaluations.at(index);
        const auto pointIt = std::find_if(
            m_points.cbegin(), m_points.cend(),
            [&evaluation](const SpeakingAnalytics::YearToDatePoint& point)
            {
                return point.evaluationName == evaluation;
            });
        const qreal x = slotCenter(index);

        if (pointIt != m_points.cend())
        {
            const QPointF position = pointPosition(index, pointIt->classAverage3);

            painter.setPen(QPen(foreground, 1.2));
            painter.setBrush(lineColor);
            painter.drawEllipse(position, 4.5, 4.5);

            // Keep the qualitative and numeric results distinct: the grade
            // sits above the marker and its one-decimal score sits below it.
            painter.setFont(valueFont);
            painter.setPen(foreground);
            painter.drawText(
                QRectF(position.x() - slotWidth / 2.0,
                       position.y() - 26.0,
                       slotWidth,
                       18.0),
                Qt::AlignCenter,
                pointIt->classAverageLetter);

            painter.setFont(labelFont);
            painter.setPen(Qt::white);
            painter.drawText(
                QRectF(position.x() - slotWidth / 2.0,
                       position.y() + 7.0,
                       slotWidth,
                       16.0),
                Qt::AlignCenter,
                SpeakingAnalytics::formatAverage(pointIt->classAverage3));
        }

        painter.setFont(axisLabelFont);
        painter.setPen(mutedTextColor(this));
        const QString label = evaluation == QLatin1String("Speech Contest")
            ? QStringLiteral("Speech\nContest")
            : evaluation;
        Qt::Alignment labelAlignment = Qt::AlignHCenter | Qt::AlignTop;
        QRectF labelArea(x - slotWidth / 2.0,
                         plot.bottom() + 18.0,
                         slotWidth,
                         30.0);
        if (index == 0)
        {
            labelArea = QRectF(plot.left(), labelArea.top(), slotWidth, 30.0);
            labelAlignment = Qt::AlignLeft | Qt::AlignTop;
        }
        else if (index == evaluationCount - 1)
        {
            labelArea = QRectF(
                plot.right() - slotWidth, labelArea.top(), slotWidth, 30.0);
            labelAlignment = Qt::AlignRight | Qt::AlignTop;
        }
        else if (evaluation == QLatin1String("Speech Contest"))
        {
            // Preserve a readable gap after Winter at the card's narrowest
            // supported width while retaining the two-line label.
            labelArea.translate(4.0, 0.0);
        }
        painter.drawText(
            labelArea,
            labelAlignment,
            label);
    }

    if (m_points.isEmpty())
    {
        painter.setFont(FontManager::getUiFont(11));
        painter.setPen(mutedTextColor(this));
        painter.drawText(
            plot,
            Qt::AlignCenter | Qt::TextWordWrap,
            tr("No completed evaluations yet."));
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
        const QFont badgeFont = FontManager::getUiFont(9);
        const qreal badgeWidth = QFontMetricsF(badgeFont).horizontalAdvance(text) + 14.0;
        const qreal averageTextWidth = QFontMetricsF(FontManager::getUiFont(10))
            .horizontalAdvance(m_averageText);
        const QRectF badge(
            averageArea.left() + averageTextWidth + 20.0,
            averageArea.center().y() - 8.5,
            badgeWidth,
            17.0);
        painter.setPen(QPen(color, 1.0));
        painter.setBrush(color.lighter(isDarkTheme(this) ? 120 : 185));
        painter.drawRoundedRect(badge, 8.5, 8.5);
        painter.setFont(badgeFont);
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
