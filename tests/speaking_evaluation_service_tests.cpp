#include "app/services/feature_services.h"
#include "data/data_service.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace
{

QStringList makeRow(
    const QString& english,
    const QString& korean,
    const QString& grade,
    bool fullyScored = true
)
{
    return {
        QStringLiteral("0"),
        english,
        korean,
        grade,
        grade,
        grade,
        grade,
        grade,
        fullyScored ? grade : QString(),
        QString(),
        QString()
    };
}

SpeakingEvalRows rows(std::initializer_list<QStringList> values)
{
    return SpeakingEvalRows(values);
}

} // namespace

class SpeakingEvaluationServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void dashboardUsesCurrentRosterAndHistoricalTrendCohorts();
};

void SpeakingEvaluationServiceTests::dashboardUsesCurrentRosterAndHistoricalTrendCohorts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService dataService;
    QVERIFY(dataService.openDatabase(
        directory.filePath(QStringLiteral("speaking-analytics.db"))).has_value());

    ClassService classes(dataService.databaseSession(), &dataService);
    RosterService rosters(dataService.databaseSession(), &dataService);
    SpeakingEvaluationService evaluations(
        dataService.databaseSession(), &dataService);
    const auto created = classes.create(QStringLiteral("Analytics Class"));
    QVERIFY(created.has_value());
    const int classId = *created;

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows.append({ QStringLiteral("Current"), QStringLiteral("현재") });
    QVERIFY(rosters.saveRoster(classId, roster).has_value());

    QVERIFY(evaluations.saveEvaluation(classId, QStringLiteral("Winter"), rows({
        makeRow(QStringLiteral("Current"), QStringLiteral("현재"),
                QStringLiteral("B")),
        makeRow(QStringLiteral("Former"), QStringLiteral("이전"),
                QStringLiteral("A+"))
    })).has_value());
    QVERIFY(evaluations.saveEvaluation(classId, QStringLiteral("Speech Contest"), rows({
        makeRow(QStringLiteral("Current"), QStringLiteral("현재"),
                QStringLiteral("A"), false)
    })).has_value());
    QVERIFY(evaluations.saveEvaluation(classId, QStringLiteral("Summer"), rows({
        makeRow(QStringLiteral("Current"), QStringLiteral("현재"),
                QStringLiteral("A"))
    })).has_value());
    QVERIFY(evaluations.saveEvaluation(classId, QStringLiteral("Fall"), rows({
        makeRow(QStringLiteral("Former"), QStringLiteral("이전"),
                QStringLiteral("A+"))
    })).has_value());

    const SpeakingEvaluationDashboard all =
        evaluations.analyticsDashboard(classId, {});
    // The current dashboard excludes Former, while the historical Winter
    // point keeps that student: (2.0 + 5.0) / 2 = 3.5.
    QCOMPARE(all.selectedSnapshot.classAverage3, 3.294);
    QCOMPARE(all.classShapeEvaluationName, QStringLiteral("Summer"));
    QCOMPARE(all.classShapeSnapshot.overallLetters,
             (QStringList{ QStringLiteral("A") }));
    QCOMPARE(all.yearToDatePoints.size(), 3);
    QCOMPARE(all.yearToDatePoints.at(0).evaluationName, QStringLiteral("Winter"));
    QCOMPARE(all.yearToDatePoints.at(0).classAverage3, 3.5);
    QCOMPARE(all.yearToDatePoints.at(0).classAverageLetter, QStringLiteral("A"));
    QCOMPARE(all.yearToDatePoints.at(1).evaluationName, QStringLiteral("Summer"));
    QCOMPARE(all.yearToDatePoints.at(2).evaluationName, QStringLiteral("Fall"));

    // A named selection always uses the selected named snapshot for both the
    // dashboard and its Class Shape distribution, while YTD remains global.
    const SpeakingEvaluationDashboard winter =
        evaluations.analyticsDashboard(classId, QStringLiteral("Winter"));
    QCOMPARE(winter.selectedSnapshot.overallLetters,
             (QStringList{ QStringLiteral("B") }));
    QCOMPARE(winter.classShapeEvaluationName, QStringLiteral("Winter"));
    QCOMPARE(winter.classShapeSnapshot.overallLetters,
             (QStringList{ QStringLiteral("B") }));
    QCOMPARE(winter.yearToDatePoints.size(), 3);
}

QTEST_MAIN(SpeakingEvaluationServiceTests)

#include "speaking_evaluation_service_tests.moc"
