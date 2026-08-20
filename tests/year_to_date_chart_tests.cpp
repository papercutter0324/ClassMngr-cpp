#include "features/classes/ui/class_analytics_charts.h"

#include "core/fontmanager.h"

#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QtTest/QtTest>

namespace
{

QImage render(YearToDateChart& chart, const QSize& size)
{
    chart.resize(size);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    chart.render(&painter);
    return image;
}

QImage render(QWidget& widget, const QSize& size)
{
    widget.resize(size);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget.render(&painter);
    return image;
}

bool hasPaintedPixel(const QImage& image)
{
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (qAlpha(image.pixel(x, y)) > 0)
                return true;
        }
    }
    return false;
}

bool hasTrendPixelInRange(const QImage& image, int minimumX, int maximumX)
{
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = minimumX; x <= maximumX; ++x)
        {
            const QRgb pixel = image.pixel(x, y);
            if (qAbs(qRed(pixel) - 0x3f) <= 3
                && qAbs(qGreen(pixel) - 0x7e) <= 3
                && qAbs(qBlue(pixel) - 0xcb) <= 3)
            {
                return true;
            }
        }
    }
    return false;
}

int firstGreenBarPixel(const QImage& image, int y)
{
    for (int x = 0; x < image.width(); ++x)
    {
        const QRgb pixel = image.pixel(x, y);
        if (qAbs(qRed(pixel) - 0x15) <= 3
            && qAbs(qGreen(pixel) - 0x94) <= 3
            && qAbs(qBlue(pixel) - 0x47) <= 3)
        {
            return x;
        }
    }
    return -1;
}

SpeakingAnalytics::YearToDatePoint point(
    const QString& name,
    double average,
    const QString& grade
)
{
    return { name, average, grade };
}

class FontSizeOffsetReset
{
public:
    FontSizeOffsetReset()
        : m_offset(FontManager::sizeOffset())
    {
    }

    ~FontSizeOffsetReset()
    {
        FontManager::setSizeOffset(m_offset);
    }

private:
    int m_offset;
};

} // namespace

class YearToDateChartTests : public QObject
{
    Q_OBJECT

private slots:
    void evaluationHistogramUsesReducedHeight();
    void rendersNoDataWithAllEvaluationSlotsAndSinglePoint();
    void rendersMultiplePointsAtNarrowCardWidth();
    void alignsEdgePointsWithTheirLabels();
    void expandsForExtraLargeTwoLineAxisLabel();
    void alignsCriterionBarsAfterInsightBadge();
};

void YearToDateChartTests::evaluationHistogramUsesReducedHeight()
{
    GradeHistogram histogram;
    QCOMPARE(histogram.minimumHeight(), 165);
    QCOMPARE(histogram.sizeHint(), QSize(300, 195));
}

void YearToDateChartTests::rendersNoDataWithAllEvaluationSlotsAndSinglePoint()
{
    YearToDateChart chart;
    QCOMPARE(chart.sizeHint(), QSize(300, 190));

    chart.setData({});
    const QImage empty = render(chart, QSize(300, 190));
    QVERIFY(!empty.isNull());
    QVERIFY(hasPaintedPixel(empty));

    chart.setData({ point(QStringLiteral("Winter"), 3.5, QStringLiteral("A")) });
    const QImage single = render(chart, QSize(300, 190));
    QVERIFY(!single.isNull());
    QVERIFY(hasPaintedPixel(single));
}

void YearToDateChartTests::rendersMultiplePointsAtNarrowCardWidth()
{
    YearToDateChart chart;
    chart.setData({
        point(QStringLiteral("Winter"), 2.0, QStringLiteral("B")),
        point(QStringLiteral("Speech Contest"), 3.0, QStringLiteral("B+")),
        point(QStringLiteral("Summer"), 4.0, QStringLiteral("A")),
        point(QStringLiteral("Fall"), 4.5, QStringLiteral("A"))
    });

    const QImage image = render(chart, QSize(270, 190));
    QVERIFY(!image.isNull());
    QVERIFY(hasPaintedPixel(image));
}

void YearToDateChartTests::alignsEdgePointsWithTheirLabels()
{
    const QFontMetricsF metrics(FontManager::getUiFont(9));

    YearToDateChart chart;
    chart.setData({ point(QStringLiteral("Winter"), 3.0, QStringLiteral("B+")) });
    const QImage winter = render(chart, QSize(270, 190));
    const int winterCenter = qRound(
        42.0 + metrics.horizontalAdvance(QStringLiteral("Winter")) / 2.0);
    QVERIFY(hasTrendPixelInRange(winter, winterCenter - 8, winterCenter + 8));

    chart.setData({ point(QStringLiteral("Fall"), 3.0, QStringLiteral("B+")) });
    const QImage fall = render(chart, QSize(270, 190));
    const int fallCenter = qRound(
        258.0 - metrics.horizontalAdvance(QStringLiteral("Fall")) / 2.0);
    QVERIFY(hasTrendPixelInRange(fall, fallCenter - 8, fallCenter + 8));
}

void YearToDateChartTests::expandsForExtraLargeTwoLineAxisLabel()
{
    const FontSizeOffsetReset restoreFontSize;
    FontManager::setSizeOffset(4);

    YearToDateChart chart;
    QCOMPARE(chart.minimumSizeHint().height(), chart.minimumHeight());
    QVERIFY(chart.minimumHeight() > 190);

    chart.setData({ point(QStringLiteral("Speech Contest"), 3.0,
                          QStringLiteral("B+")) });
    const QImage image = render(chart, QSize(270, chart.minimumHeight()));
    QVERIFY(!image.isNull());
    QVERIFY(hasPaintedPixel(image));
}

void YearToDateChartTests::alignsCriterionBarsAfterInsightBadge()
{
    const FontSizeOffsetReset restoreFontSize;
    FontManager::setSizeOffset(4);

    CriterionDistributionBar strongest;
    strongest.setAverageText(QStringLiteral("A+  -  5.0"));
    strongest.setData({ { QStringLiteral("A+"), 1 } });
    strongest.setInsight(AnalyticsCharts::CriterionInsight::Strongest);

    CriterionDistributionBar plain;
    plain.setAverageText(QStringLiteral("A+  -  5.0"));
    plain.setData({ { QStringLiteral("A+"), 1 } });

    const qreal sharedBarLeft = qMax(
        strongest.minimumBarLeft(), plain.minimumBarLeft());
    QVERIFY(sharedBarLeft > plain.minimumBarLeft());
    strongest.setSharedBarLeft(sharedBarLeft);
    plain.setSharedBarLeft(sharedBarLeft);

    const QImage strongestImage = render(strongest, QSize(520, 54));
    const QImage plainImage = render(plain, QSize(520, 54));
    const int strongestBarStart = firstGreenBarPixel(strongestImage, 20);
    const int plainBarStart = firstGreenBarPixel(plainImage, 20);
    QVERIFY(strongestBarStart >= 0);
    QCOMPARE(plainBarStart, strongestBarStart);
}

QTEST_MAIN(YearToDateChartTests)

#include "year_to_date_chart_tests.moc"
