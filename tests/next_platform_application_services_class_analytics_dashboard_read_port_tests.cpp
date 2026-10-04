#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/roster.h"
#include "next/application/class_analytics_dashboard_read_query.h"
#include "next/platform/application_services_class_analytics_dashboard_read_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <array>
#include <string>

using namespace ClassMngr::Next;

namespace
{

QStringList evaluationRow(
    const QString& english,
    const QString& korean,
    const QString& grammar,
    const QString& pronunciation,
    const QString& fluency,
    const QString& manner,
    const QString& content,
    const QString& effort,
    const QString& comments,
    const QString& notes
)
{
    return {
        QStringLiteral("0"), english, korean, grammar, pronunciation,
        fluency, manner, content, effort, comments, notes
    };
}

} // namespace

class NextPlatformApplicationServicesClassAnalyticsDashboardReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsOnlyRosterNamesAndExactEvaluationScoreProjection();
    void missingEvaluationIsSuccessfulEmptyInput();
    void activeSessionQueryFailureIsStructured();
};

void NextPlatformApplicationServicesClassAnalyticsDashboardReadPortTests::
readsOnlyRosterNamesAndExactEvaluationScoreProjection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(
        directory.filePath(QStringLiteral("class-analytics.db"))));
    const auto classId = services.classService()->create(
        QStringLiteral("Analytics"));
    QVERIFY(classId);
    const auto otherClassId = services.classService()->create(
        QStringLiteral("Other analytics"));
    QVERIFY(otherClassId);

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.columns.append(QStringLiteral("Notes"));
    roster.columnWidths = {100, 100, 100, 100, 100, 100, 250};
    roster.rows = {{
        QStringLiteral("J. P. Kim"), QStringLiteral("김민수(B)"),
        QStringLiteral("C"), QStringLiteral("B"), QStringLiteral("B+"),
        QStringLiteral("A"), QStringLiteral("roster-private-marker")
    }};
    QVERIFY(services.rosterService()->saveRoster(*classId, roster));

    const QStringList names{
        QStringLiteral("Winter"), QStringLiteral("Speech Contest"),
        QStringLiteral("Summer"), QStringLiteral("Fall")
    };
    const QStringList grades{
        QStringLiteral("C"), QStringLiteral("B"), QStringLiteral("B+"),
        QStringLiteral("A"), QStringLiteral("A+"), QStringLiteral("C")
    };
    for (int index = 0; index < names.size(); ++index)
    {
        QVERIFY(services.speakingEvaluationService()->saveEvaluation(
            *classId, names.at(index), {
                evaluationRow(
                    QStringLiteral("J.P.KIM"), QStringLiteral("김민수(A)"),
                    grades.at(index), grades.at((index + 1) % grades.size()),
                    grades.at((index + 2) % grades.size()),
                    grades.at((index + 3) % grades.size()),
                    grades.at((index + 4) % grades.size()),
                    grades.at((index + 5) % grades.size()),
                    QStringLiteral("evaluation-private-marker"),
                    QStringLiteral("notes-private-marker"))
            }));
    }
    QVERIFY(services.speakingEvaluationService()->saveEvaluation(
        *otherClassId, QStringLiteral("Winter"), {
            evaluationRow(
                QStringLiteral("Other class student"), QStringLiteral("김민수(A)"),
                QStringLiteral("A+"), QStringLiteral("A+"),
                QStringLiteral("A+"), QStringLiteral("A+"),
                QStringLiteral("A+"), QStringLiteral("A+"),
                QStringLiteral("other-class-comment"),
                QStringLiteral("other-class-note"))
        }));

    QSqlQuery setSecondWinterRow(services.databaseSession()->database());
    setSecondWinterRow.prepare(R"(
        UPDATE speaking_eval_data
        SET col_1=?, col_2=?, col_3=?, col_4=?, col_5=?, col_6=?, col_7=?, col_8=?
        WHERE evaluation_id=(
            SELECT id FROM speaking_evaluations
            WHERE class_id=? AND evaluation_name='Winter'
        ) AND row_index=1
    )");
    setSecondWinterRow.addBindValue(QStringLiteral("Second row student"));
    setSecondWinterRow.addBindValue(QStringLiteral("Second row Korean"));
    setSecondWinterRow.addBindValue(QStringLiteral("B"));
    setSecondWinterRow.addBindValue(QStringLiteral("B+"));
    setSecondWinterRow.addBindValue(QStringLiteral("A"));
    setSecondWinterRow.addBindValue(QStringLiteral("A+"));
    setSecondWinterRow.addBindValue(QStringLiteral("C"));
    setSecondWinterRow.addBindValue(QStringLiteral("B"));
    setSecondWinterRow.addBindValue(*classId);
    QVERIFY(setSecondWinterRow.exec());

    const auto typedClassId = Domain::ClassId::fromString(
        std::to_string(*classId));
    QVERIFY(typedClassId);
    Platform::ApplicationServicesClassAnalyticsDashboardReadPort port(services);
    QVERIFY(port.normalizedEnglishIdentity(u"mary - jane")
            == port.normalizedEnglishIdentity(u"Mary-jane"));
    QVERIFY(port.normalizedEnglishIdentity(u"j. p. kim")
            == port.normalizedEnglishIdentity(u"J.P.Kim"));
    QVERIFY(port.baseKoreanName(u"김민수(A)")
            == port.baseKoreanName(u"김민수(B)"));

    const auto rosterProjection = port.readRosterNames(*typedClassId);
    QVERIFY(rosterProjection);
    QVERIFY(rosterProjection.value().rowCount == std::size_t{1});
    QVERIFY(rosterProjection.value().hasEnglishColumn);
    QVERIFY(rosterProjection.value().hasKoreanColumn);
    QVERIFY(rosterProjection.value().englishNames
            == std::vector<std::u16string>{u"J.P.Kim"});
    QVERIFY(rosterProjection.value().koreanNames
            == std::vector<std::u16string>{u"김민수(B)"});

    const auto evaluations = port.readEvaluationBatch(*typedClassId);
    QVERIFY(evaluations);
    const auto& winter = evaluations.value()[0];
    QVERIFY(winter.size() == static_cast<std::size_t>(SpeakingEval::RowCount));
    const auto& projected = winter.front();
    QVERIFY(projected.englishName == u"J.P.Kim");
    QVERIFY(projected.koreanName == u"김민수(A)");
    QVERIFY((projected.scores == std::array<std::u16string, 6>{
        u"C", u"B", u"B+", u"A", u"A+", u"C"
    }));
    QVERIFY(winter[1].englishName == u"Second row student");
    QVERIFY((winter[1].scores == std::array<std::u16string, 6>{
        u"B", u"B+", u"A", u"A+", u"C", u"B"
    }));
    QVERIFY(winter.front().englishName != u"Other class student");
    for (std::size_t index = 0; index < evaluations.value().size(); ++index)
    {
        QVERIFY(!evaluations.value()[index].empty());
        QVERIFY(evaluations.value()[index].front().englishName == u"J.P.Kim");
        QVERIFY(evaluations.value()[index].front().scores[0]
                == grades.at(static_cast<qsizetype>(index)).toStdU16String());
    }

    const auto dashboard = Application::ClassAnalyticsDashboardReadQuery::execute(
        {
            .classId = *typedClassId,
            .evaluationSelection = u"All"
        },
        port,
        port);
    QVERIFY(dashboard);
    QVERIFY(dashboard.value().selectedSnapshot.rankings.size() == 1);
    QCOMPARE(dashboard.value().yearToDatePoints.size(), std::size_t{4});
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        QVERIFY(dashboard.value().yearToDatePoints[index].evaluationName
                == names.at(static_cast<qsizetype>(index)).toStdU16String());
    }
}

void NextPlatformApplicationServicesClassAnalyticsDashboardReadPortTests::
missingEvaluationIsSuccessfulEmptyInput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(
        directory.filePath(QStringLiteral("class-analytics-missing.db"))));
    const auto classId = services.classService()->create(
        QStringLiteral("Analytics"));
    QVERIFY(classId);
    const auto typedClassId = Domain::ClassId::fromString(
        std::to_string(*classId));
    QVERIFY(typedClassId);

    QSqlQuery insertEmptyEvaluation(services.databaseSession()->database());
    insertEmptyEvaluation.prepare(R"(
        INSERT INTO speaking_evaluations (class_id, evaluation_name)
        VALUES (?, ?)
    )");
    insertEmptyEvaluation.addBindValue(*classId);
    insertEmptyEvaluation.addBindValue(QStringLiteral("Speech Contest"));
    QVERIFY(insertEmptyEvaluation.exec());

    Platform::ApplicationServicesClassAnalyticsDashboardReadPort port(services);
    const auto batch = port.readEvaluationBatch(*typedClassId);
    QVERIFY(batch);
    QVERIFY(batch.value()[0].empty()); // No Winter evaluation record.
    QVERIFY(batch.value()[1].empty()); // Present Speech Contest, no data rows.
    QVERIFY(batch.value()[2].empty());
    QVERIFY(batch.value()[3].empty());
}

void NextPlatformApplicationServicesClassAnalyticsDashboardReadPortTests::
activeSessionQueryFailureIsStructured()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(
        directory.filePath(QStringLiteral("class-analytics-failure.db"))));
    const auto classId = services.classService()->create(
        QStringLiteral("Analytics"));
    QVERIFY(classId);
    QVERIFY(services.speakingEvaluationService()->saveEvaluation(
        *classId, QStringLiteral("Winter"), {
            evaluationRow(QStringLiteral("Current"), QStringLiteral("김민수"),
                QStringLiteral("A"), QStringLiteral("A"), QStringLiteral("A"),
                QStringLiteral("A"), QStringLiteral("A"), QStringLiteral("A"),
                {}, {})
        }));
    const auto typedClassId = Domain::ClassId::fromString(
        std::to_string(*classId));
    QVERIFY(typedClassId);

    QSqlQuery drop(services.databaseSession()->database());
    QVERIFY(drop.exec(QStringLiteral("DROP TABLE speaking_eval_data")));
    Platform::ApplicationServicesClassAnalyticsDashboardReadPort port(services);
    const auto failed = port.readEvaluationBatch(*typedClassId);
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());
}

QTEST_MAIN(NextPlatformApplicationServicesClassAnalyticsDashboardReadPortTests)

#include "next_platform_application_services_class_analytics_dashboard_read_port_tests.moc"
