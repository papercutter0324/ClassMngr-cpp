#include "core/application_services.h"
#include "features/classes/ui/class_analytics_charts.h"
#include "features/classes/ui/class_analytics_page.h"
#include "next/application/class_analytics_dashboard_read_port.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QtTest/QtTest>

#include <array>
#include <string>
#include <string_view>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class DefaultLocaleScope final
{
public:
    DefaultLocaleScope()
        : m_previous(QLocale())
    {
        QLocale::setDefault(QLocale::c());
    }

    ~DefaultLocaleScope()
    {
        QLocale::setDefault(m_previous);
    }

private:
    QLocale m_previous;
};

class InMemoryDashboardReadPort final
    : public ClassAnalyticsDashboardReadPort
{
public:
    ClassAnalyticsRosterNames roster{
        .rowCount = 3,
        .hasEnglishColumn = true,
        .hasKoreanColumn = false,
        .englishNames = { u"Alex Example", u"Blair Example", u"No Scores" }
    };
    ClassAnalyticsEvaluationBatch evaluations;
    mutable int rosterReadCount = 0;
    mutable int evaluationBatchReadCount = 0;

    [[nodiscard]] Domain::Result<ClassAnalyticsRosterNames> readRosterNames(
        const Domain::ClassId& classId
        ) const override
    {
        (void)classId;
        ++rosterReadCount;
        return Domain::Result<ClassAnalyticsRosterNames>::success(roster);
    }

    [[nodiscard]] Domain::Result<ClassAnalyticsEvaluationBatch>
    readEvaluationBatch(
        const Domain::ClassId& classId
        ) const override
    {
        (void)classId;
        ++evaluationBatchReadCount;
        return Domain::Result<ClassAnalyticsEvaluationBatch>::success(
            evaluations);
    }
};

class TestNameSemantics final : public ClassAnalyticsNameSemanticsPort
{
public:
    [[nodiscard]] std::u16string trimmed(
        const std::u16string_view value
        ) const override
    {
        std::size_t first = 0;
        std::size_t last = value.size();
        while (first < last && (value[first] == u' ' || value[first] == u'\t'))
            ++first;
        while (last > first && (value[last - 1] == u' ' || value[last - 1] == u'\t'))
            --last;
        return std::u16string(value.substr(first, last - first));
    }

    [[nodiscard]] std::u16string normalizedEnglishIdentity(
        const std::u16string_view value
        ) const override
    {
        std::u16string normalized = trimmed(value);
        for (char16_t& character : normalized)
        {
            if (character >= u'A' && character <= u'Z')
                character = static_cast<char16_t>(character + (u'a' - u'A'));
        }
        return normalized;
    }

    [[nodiscard]] std::u16string baseKoreanName(
        const std::u16string_view value
        ) const override
    {
        return trimmed(value);
    }

    [[nodiscard]] bool equalsCaseInsensitive(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        return normalizedEnglishIdentity(left)
            == normalizedEnglishIdentity(right);
    }

    [[nodiscard]] int compareEnglishNames(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        const std::u16string normalizedLeft =
            normalizedEnglishIdentity(left);
        const std::u16string normalizedRight =
            normalizedEnglishIdentity(right);
        return normalizedLeft < normalizedRight
            ? -1
            : normalizedLeft > normalizedRight ? 1 : 0;
    }
};

ClassAnalyticsEvaluationRow row(
    const std::u16string_view englishName,
    const std::u16string_view grade
)
{
    ClassAnalyticsEvaluationRow result;
    result.englishName = englishName;
    result.scores.fill(std::u16string(grade));
    return result;
}

InMemoryDashboardReadPort populatedReadPort()
{
    InMemoryDashboardReadPort result;
    result.evaluations[static_cast<std::size_t>(
        ClassAnalyticsEvaluation::Winter)] = {
        row(u"Alex Example", u"A"),
        row(u"Blair Example", u"B")
    };
    result.evaluations[static_cast<std::size_t>(
        ClassAnalyticsEvaluation::SpeechContest)] = {
        row(u"Alex Example", u"A+"),
        row(u"Blair Example", u"B+")
    };
    result.evaluations[static_cast<std::size_t>(
        ClassAnalyticsEvaluation::Summer)] = {
        row(u"Alex Example", u"B+"),
        row(u"Blair Example", u"A")
    };
    return result;
}

QList<QLabel*> visibleLabelsWithText(
    const QWidget& parent,
    const QString& text
)
{
    QList<QLabel*> result;
    for (QLabel* label : parent.findChildren<QLabel*>())
    {
        if (label->isVisible() && label->text() == text)
            result.append(label);
    }
    return result;
}

QImage renderWidget(QWidget& widget)
{
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget.render(&painter);
    return image;
}

bool matchesHistogram(
    GradeHistogram& actual,
    const QMap<QString, int>& expectedDistribution
)
{
    GradeHistogram expected(actual.parentWidget());
    expected.setData(expectedDistribution);
    expected.resize(actual.size());
    expected.show();
    return renderWidget(actual) == renderWidget(expected);
}

bool matchesYearToDateChart(
    YearToDateChart& actual,
    const QList<SpeakingAnalytics::YearToDatePoint>& expectedPoints
)
{
    YearToDateChart expected(actual.parentWidget());
    expected.setData(expectedPoints);
    expected.resize(actual.size());
    expected.show();
    return renderWidget(actual) == renderWidget(expected);
}

SpeakingAnalytics::YearToDatePoint point(
    const QString& evaluationName,
    const double average,
    const QString& grade
)
{
    return {
        .evaluationName = evaluationName,
        .classAverage3 = average,
        .classAverageLetter = grade
    };
}

QJsonArray yearToDateTranscript()
{
    return {
        QJsonObject{
            { QStringLiteral("evaluation"), QStringLiteral("Winter") },
            { QStringLiteral("grade"), QStringLiteral("B+") },
            { QStringLiteral("average_3"), QStringLiteral("3.0") }
        },
        QJsonObject{
            { QStringLiteral("evaluation"), QStringLiteral("Speech Contest") },
            { QStringLiteral("grade"), QStringLiteral("A") },
            { QStringLiteral("average_3"), QStringLiteral("4.0") }
        },
        QJsonObject{
            { QStringLiteral("evaluation"), QStringLiteral("Summer") },
            { QStringLiteral("grade"), QStringLiteral("A") },
            { QStringLiteral("average_3"), QStringLiteral("3.5") }
        }
    };
}

void logTranscript()
{
    const QJsonObject all{
        { QStringLiteral("average_grade"), QStringLiteral("A") },
        { QStringLiteral("average_3"), QStringLiteral("3.5") },
        { QStringLiteral("fully_scored"), QStringLiteral("2 / 3") },
        { QStringLiteral("shape_evaluation"), QStringLiteral("Summer") },
        { QStringLiteral("shape_distribution"), QJsonObject{
            { QStringLiteral("A"), 1 },
            { QStringLiteral("B+"), 1 }
        } },
        { QStringLiteral("ytd"), yearToDateTranscript() }
    };
    const QJsonObject winter{
        { QStringLiteral("average_grade"), QStringLiteral("B+") },
        { QStringLiteral("average_3"), QStringLiteral("3.0") },
        { QStringLiteral("fully_scored"), QStringLiteral("2 / 3") },
        { QStringLiteral("shape_evaluation"), QStringLiteral("Winter") },
        { QStringLiteral("shape_distribution"), QJsonObject{
            { QStringLiteral("A"), 1 },
            { QStringLiteral("B"), 1 }
        } },
        { QStringLiteral("ytd"), yearToDateTranscript() }
    };
    const QByteArray json = QJsonDocument(QJsonObject{
        { QStringLiteral("all"), all },
        { QStringLiteral("winter"), winter }
    }).toJson(QJsonDocument::Compact);
    qInfo().noquote() << "F361_TRANSCRIPT" << QString::fromLatin1(json);
}

} // namespace

class ClassAnalyticsPageReadParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void visibleSummaryShapeAndYearToDateMatchFixture();
    void noScoredDataShowsEmptyStateAndBlankCharts();
};

void ClassAnalyticsPageReadParityTests::
visibleSummaryShapeAndYearToDateMatchFixture()
{
    DefaultLocaleScope localeScope;
    ApplicationServices services;
    InMemoryDashboardReadPort readPort = populatedReadPort();
    const TestNameSemantics names;
    ClassAnalyticsPage page(
        &services,
        { .readPort = &readPort, .nameSemantics = &names });
    page.resize(1100, 1000);
    page.loadClass(Classroom(QStringLiteral("Analytics Read Parity"), 42));
    page.show();
    QCoreApplication::processEvents();

    QComboBox* const evaluation = page.findChild<QComboBox*>();
    QVERIFY(evaluation);
    QCOMPARE(evaluation->currentIndex(), 0);
    QCOMPARE(evaluation->currentData().toString(), QString());
    QCOMPARE(evaluation->currentText(), QStringLiteral("All"));

    const QList<QLabel*> allAverage = visibleLabelsWithText(
        page, QStringLiteral("A \u00B7 3.5"));
    const QList<QLabel*> allScored = visibleLabelsWithText(
        page, QStringLiteral("2 / 3"));
    QCOMPARE(allAverage.size(), 1);
    QCOMPARE(allScored.size(), 1);

    QLabel* const shapeCaption = page.findChild<QLabel*>(
        QStringLiteral("analyticsClassShapeEvaluationCaption"));
    QVERIFY(shapeCaption);
    QCOMPARE(shapeCaption->text(), QStringLiteral("Evaluation: Summer"));

    GradeHistogram* const histogram = page.findChild<GradeHistogram*>(
        QStringLiteral("analyticsGradeHistogram"));
    YearToDateChart* const yearToDate = page.findChild<YearToDateChart*>(
        QStringLiteral("analyticsYearToDateChart"));
    QVERIFY(histogram);
    QVERIFY(yearToDate);
    QVERIFY(histogram->isVisible());
    QVERIFY(yearToDate->isVisible());
    QVERIFY(matchesHistogram(*histogram, {
        { QStringLiteral("A"), 1 },
        { QStringLiteral("B+"), 1 }
    }));
    const QList<SpeakingAnalytics::YearToDatePoint> expectedYearToDate{
        point(QStringLiteral("Winter"), 3.0, QStringLiteral("B+")),
        point(QStringLiteral("Speech Contest"), 4.0, QStringLiteral("A")),
        point(QStringLiteral("Summer"), 3.5, QStringLiteral("A"))
    };
    QVERIFY(matchesYearToDateChart(*yearToDate, expectedYearToDate));

    const int winterIndex = evaluation->findData(QStringLiteral("Winter"));
    QVERIFY(winterIndex > 0);
    evaluation->setCurrentIndex(winterIndex);
    QCoreApplication::processEvents();
    QCOMPARE(evaluation->currentData().toString(), QStringLiteral("Winter"));

    const QList<QLabel*> winterAverage = visibleLabelsWithText(
        page, QStringLiteral("B+ \u00B7 3.0"));
    const QList<QLabel*> winterScored = visibleLabelsWithText(
        page, QStringLiteral("2 / 3"));
    QCOMPARE(winterAverage.size(), 1);
    QCOMPARE(winterScored.size(), 1);
    QCOMPARE(shapeCaption->text(), QStringLiteral("Evaluation: Winter"));
    QVERIFY(matchesHistogram(*histogram, {
        { QStringLiteral("A"), 1 },
        { QStringLiteral("B"), 1 }
    }));
    QVERIFY(matchesYearToDateChart(*yearToDate, expectedYearToDate));

    QCOMPARE(readPort.rosterReadCount, 2);
    QCOMPARE(readPort.evaluationBatchReadCount, 2);
    logTranscript();
}

void ClassAnalyticsPageReadParityTests::
noScoredDataShowsEmptyStateAndBlankCharts()
{
    DefaultLocaleScope localeScope;
    ApplicationServices services;
    InMemoryDashboardReadPort readPort;
    const TestNameSemantics names;
    ClassAnalyticsPage page(
        &services,
        { .readPort = &readPort, .nameSemantics = &names });
    page.resize(1100, 1000);
    page.loadClass(Classroom(QStringLiteral("Empty Analytics Read Parity"), 43));
    page.show();
    QCoreApplication::processEvents();

    QLabel* const emptyLabel = page.findChild<QLabel*>(
        QStringLiteral("classAnalyticsEmptyLabel"));
    QVERIFY(emptyLabel);
    QVERIFY(emptyLabel->isVisible());

    GradeHistogram* const histogram = page.findChild<GradeHistogram*>(
        QStringLiteral("analyticsGradeHistogram"));
    YearToDateChart* const yearToDate = page.findChild<YearToDateChart*>(
        QStringLiteral("analyticsYearToDateChart"));
    QVERIFY(histogram);
    QVERIFY(yearToDate);
    QVERIFY(!histogram->isVisible());
    QVERIFY(!yearToDate->isVisible());
    QCOMPARE(readPort.rosterReadCount, 1);
    QCOMPARE(readPort.evaluationBatchReadCount, 1);
}

QTEST_MAIN(ClassAnalyticsPageReadParityTests)

#include "class_analytics_page_read_parity_tests.moc"
