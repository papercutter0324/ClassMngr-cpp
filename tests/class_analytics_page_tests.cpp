#include "core/application_services.h"
#include "features/classes/ui/class_analytics_page.h"
#include "features/classes/ui/class_analytics_ranking_header.h"

#include <QAbstractItemModel>
#include <QLabel>
#include <QTableView>
#include <QtTest/QtTest>

#include <array>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class EmptyDashboardReadPort final : public ClassAnalyticsDashboardReadPort
{
public:
    mutable int rosterReadCount = 0;
    mutable int evaluationBatchReadCount = 0;
    bool failRoster = false;
    ClassAnalyticsRosterNames roster;
    std::array<ClassAnalyticsEvaluationRows, 4> evaluations;

    [[nodiscard]] Domain::Result<ClassAnalyticsRosterNames> readRosterNames(
        const Domain::ClassId& classId
        ) const override
    {
        (void)classId;
        ++rosterReadCount;
        if (failRoster)
        {
            return Domain::Result<ClassAnalyticsRosterNames>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "roster read failed",
                .recoverable = true
            });
        }
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
        return std::u16string(value);
    }

    [[nodiscard]] std::u16string normalizedEnglishIdentity(
        const std::u16string_view value
        ) const override
    {
        return std::u16string(value);
    }

    [[nodiscard]] std::u16string baseKoreanName(
        const std::u16string_view value
        ) const override
    {
        return std::u16string(value);
    }

    [[nodiscard]] bool equalsCaseInsensitive(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        if (left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            const char16_t a = left[index] >= u'A' && left[index] <= u'Z'
                ? static_cast<char16_t>(left[index] + (u'a' - u'A'))
                : left[index];
            const char16_t b = right[index] >= u'A' && right[index] <= u'Z'
                ? static_cast<char16_t>(right[index] + (u'a' - u'A'))
                : right[index];
            if (a != b) return false;
        }
        return true;
    }

    [[nodiscard]] int compareEnglishNames(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        return left < right ? -1 : left > right ? 1 : 0;
    }
};

} // namespace

class ClassAnalyticsPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void readFailureAndNoDataUseTheExistingEmptyState();
    void successfulDashboardMapsResultsIntoRankingModel();
};

void ClassAnalyticsPageTests::readFailureAndNoDataUseTheExistingEmptyState()
{
    ApplicationServices services;
    EmptyDashboardReadPort readPort;
    const TestNameSemantics names;
    ClassAnalyticsPage page(
        &services,
        { .readPort = &readPort, .nameSemantics = &names });
    page.loadClass(Classroom(QStringLiteral("Analytics"), 42));
    page.show();
    QCoreApplication::processEvents();

    QLabel* const emptyLabel = page.findChild<QLabel*>(
        QStringLiteral("classAnalyticsEmptyLabel"));
    QVERIFY(emptyLabel);
    QVERIFY(emptyLabel->isVisible());
    QCOMPARE(readPort.rosterReadCount, 1);
    QCOMPARE(readPort.evaluationBatchReadCount, 1);

    readPort.failRoster = true;
    page.refresh();
    QCoreApplication::processEvents();
    QVERIFY(emptyLabel->isVisible());
    QCOMPARE(readPort.rosterReadCount, 2);
    QCOMPARE(readPort.evaluationBatchReadCount, 1);
}

void ClassAnalyticsPageTests::successfulDashboardMapsResultsIntoRankingModel()
{
    ApplicationServices services;
    EmptyDashboardReadPort readPort;
    readPort.roster.rowCount = 1;
    readPort.roster.hasEnglishColumn = true;
    readPort.roster.hasKoreanColumn = true;
    readPort.roster.englishNames = { u"Alex Example" };
    readPort.roster.koreanNames = { u"\uAE40\uBBFC\uC218" };

    ClassAnalyticsEvaluationRow winterRow;
    winterRow.englishName = u"Alex Example";
    winterRow.koreanName = u"\uAE40\uBBFC\uC218";
    winterRow.scores.fill(u"A");
    readPort.evaluations[static_cast<std::size_t>(
        ClassAnalyticsEvaluation::Winter)].push_back(winterRow);

    const TestNameSemantics names;
    ClassAnalyticsPage page(
        &services,
        { .readPort = &readPort, .nameSemantics = &names });
    page.loadClass(Classroom(QStringLiteral("Analytics"), 42));
    page.show();
    QCoreApplication::processEvents();

    QLabel* const emptyLabel = page.findChild<QLabel*>(
        QStringLiteral("classAnalyticsEmptyLabel"));
    QVERIFY(emptyLabel);
    QVERIFY(!emptyLabel->isVisible());

    QTableView* const rankingTable = page.findChild<QTableView*>(
        QStringLiteral("classAnalyticsRankingTable"));
    QVERIFY(rankingTable);
    const QAbstractItemModel* const model = rankingTable->model();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->index(0, 1).data(Qt::DisplayRole).toString(),
        QStringLiteral("Alex Example"));
    QCOMPARE(model->index(0, 2).data(Qt::DisplayRole).toString(),
        QString::fromStdU16String(u"\uAE40\uBBFC\uC218"));
    QCOMPARE(model->index(0, 3).data(Qt::DisplayRole).toString(),
        SpeakingAnalytics::formatAverage(4.0));
    QCOMPARE(model->index(0, 3).data(AnalyticsRankingRoles::Grade).toString(),
        QStringLiteral("A"));
    QCOMPARE(model->index(0, 4).data(AnalyticsRankingRoles::Grade).toString(),
        QStringLiteral("A"));
}

QTEST_MAIN(ClassAnalyticsPageTests)

#include "class_analytics_page_tests.moc"
