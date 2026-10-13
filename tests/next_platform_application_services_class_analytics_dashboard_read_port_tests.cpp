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
#include <limits>
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
    void sparseRosterSizingUsesUnrelatedCellsAndFirstExactHeaders();
    void rejectsFractionalRosterColumnWithoutInflatingAnalyticsRowCount();
    void rejectsInvalidRosterColumnPositionStorage();
    void rejectsRosterRowsOutsideClassAnalyticsIntegerRange();
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
sparseRosterSizingUsesUnrelatedCellsAndFirstExactHeaders()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(
        directory.filePath(QStringLiteral("class-analytics-sparse.db"))));
    const auto classId = services.classService()->create(
        QStringLiteral("Sparse analytics"));
    QVERIFY(classId);
    const auto absentNamesClassId = services.classService()->create(
        QStringLiteral("Absent analytics names"));
    QVERIFY(absentNamesClassId);

    QSqlQuery insertColumn(services.databaseSession()->database());
    insertColumn.prepare(R"(
        INSERT INTO roster_columns (class_id, name, position, width)
        VALUES (?, ?, ?, 100)
    )");
    const auto addColumn = [&](const int ownerId, const QString& name,
                               const int position) {
        insertColumn.bindValue(0, ownerId);
        insertColumn.bindValue(1, name);
        insertColumn.bindValue(2, position);
        return insertColumn.exec();
    };
    // Insert IDs out of display order so the first exact English header is
    // selected by (position, id), not by insertion order.
    QVERIFY(addColumn(*classId, QStringLiteral("English"), 2));
    QVERIFY(addColumn(*classId, QStringLiteral("Korean"), 1));
    QVERIFY(addColumn(*classId, QStringLiteral("English"), 0));
    QVERIFY(addColumn(*classId, QStringLiteral("Notes"), 3));
    QVERIFY(addColumn(*classId, QStringLiteral("english"), 4));
    QVERIFY(addColumn(*absentNamesClassId, QStringLiteral("Notes"), 0));

    QSqlQuery insertCell(services.databaseSession()->database());
    insertCell.prepare(R"(
        INSERT INTO roster_data (class_id, row_index, col_index, value)
        VALUES (?, ?, ?, ?)
    )");
    const auto addCell = [&](const int ownerId, const int row, const int column,
                             const QString& value) {
        insertCell.bindValue(0, ownerId);
        insertCell.bindValue(1, row);
        insertCell.bindValue(2, column);
        insertCell.bindValue(3, value);
        return insertCell.exec();
    };
    QVERIFY(addCell(*classId, 0, 0, QStringLiteral("Chosen English")));
    QVERIFY(addCell(*classId, 2, 1, QStringLiteral("Chosen Korean")));
    QVERIFY(addCell(*classId, 2, 2,
                    QStringLiteral("duplicate-english-private-marker")));
    QVERIFY(addCell(*classId, 4, 3,
                    QStringLiteral("unrelated-private-marker")));
    QVERIFY(addCell(*absentNamesClassId, 3, 0,
                    QStringLiteral("absent-header-private-marker")));

    const auto typedClassId = Domain::ClassId::fromString(
        std::to_string(*classId));
    const auto typedAbsentNamesClassId = Domain::ClassId::fromString(
        std::to_string(*absentNamesClassId));
    QVERIFY(typedClassId);
    QVERIFY(typedAbsentNamesClassId);
    Platform::ApplicationServicesClassAnalyticsDashboardReadPort port(services);

    const auto roster = port.readRosterNames(*typedClassId);
    QVERIFY(roster);
    QVERIFY(roster.value().hasEnglishColumn);
    QVERIFY(roster.value().hasKoreanColumn);
    QCOMPARE(roster.value().rowCount, std::size_t{5});
    QVERIFY((roster.value().englishNames == std::vector<std::u16string>{
        u"Chosen English", u"", u"", u"", u""
    }));
    QVERIFY((roster.value().koreanNames == std::vector<std::u16string>{
        u"", u"", u"Chosen Korean", u"", u""
    }));

    const auto absentHeaders = port.readRosterNames(*typedAbsentNamesClassId);
    QVERIFY(absentHeaders);
    QVERIFY(!absentHeaders.value().hasEnglishColumn);
    QVERIFY(!absentHeaders.value().hasKoreanColumn);
    QCOMPARE(absentHeaders.value().rowCount, std::size_t{4});
    QVERIFY(absentHeaders.value().englishNames
            == std::vector<std::u16string>(4));
    QVERIFY(absentHeaders.value().koreanNames
            == std::vector<std::u16string>(4));
}

void NextPlatformApplicationServicesClassAnalyticsDashboardReadPortTests::
rejectsFractionalRosterColumnWithoutInflatingAnalyticsRowCount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(
        directory.filePath(QStringLiteral("class-analytics-fractional-column.db"))));
    const auto classId = services.classService()->create(
        QStringLiteral("Fractional analytics column"));
    QVERIFY(classId);

    QSqlQuery insertColumn(services.databaseSession()->database());
    insertColumn.prepare(R"(
        INSERT INTO roster_columns (class_id, name, position, width)
        VALUES (?, ?, ?, 100)
    )");
    const auto addColumn = [&](const QString& name, const int position) {
        insertColumn.bindValue(0, *classId);
        insertColumn.bindValue(1, name);
        insertColumn.bindValue(2, position);
        return insertColumn.exec();
    };
    QVERIFY(addColumn(QStringLiteral("English"), 0));
    QVERIFY(addColumn(QStringLiteral("Korean"), 1));
    QVERIFY(addColumn(QStringLiteral("Notes"), 2));

    QSqlQuery insertCell(services.databaseSession()->database());
    insertCell.prepare(R"(
        INSERT INTO roster_data (class_id, row_index, col_index, value)
        VALUES (?, ?, ?, ?)
    )");
    const auto addCell = [&](const QVariant& row, const QVariant& column,
                             const QString& value) {
        insertCell.bindValue(0, *classId);
        insertCell.bindValue(1, row);
        insertCell.bindValue(2, column);
        insertCell.bindValue(3, value);
        return insertCell.exec();
    };
    QVERIFY(addCell(QVariant::fromValue<qlonglong>(0),
                    QVariant::fromValue<qlonglong>(0),
                    QStringLiteral("First student")));
    QVERIFY(addCell(QVariant::fromValue<qlonglong>(3),
                    QVariant::fromValue<qlonglong>(2),
                    QStringLiteral("Sparse unrelated cell")));
    QVERIFY(addCell(QVariant::fromValue<qlonglong>(7),
                    QVariant::fromValue(0.25),
                    QStringLiteral("Fractional column cell")));

    QSqlQuery fractionalStorage(services.databaseSession()->database());
    fractionalStorage.prepare(R"(
        SELECT typeof(col_index)
        FROM roster_data
        WHERE class_id=? AND row_index=7 AND value='Fractional column cell'
    )");
    fractionalStorage.addBindValue(*classId);
    QVERIFY(fractionalStorage.exec());
    QVERIFY(fractionalStorage.next());
    QCOMPARE(fractionalStorage.value(0).toString(), QStringLiteral("real"));

    const auto typedClassId = Domain::ClassId::fromString(
        std::to_string(*classId));
    QVERIFY(typedClassId);
    Platform::ApplicationServicesClassAnalyticsDashboardReadPort port(services);
    const auto roster = port.readRosterNames(*typedClassId);
    QVERIFY(!roster);
    QCOMPARE(roster.error().code, Domain::ErrorCode::Technical);
    QVERIFY(roster.error().recoverable);
    QVERIFY(roster.error().message
            == "Roster data contains a non-integer column index.");

    QSqlQuery fractionalStorageAfterRead(
        services.databaseSession()->database());
    fractionalStorageAfterRead.prepare(R"(
        SELECT typeof(col_index)
        FROM roster_data
        WHERE class_id=? AND row_index=7 AND value='Fractional column cell'
    )");
    fractionalStorageAfterRead.addBindValue(*classId);
    QVERIFY(fractionalStorageAfterRead.exec());
    QVERIFY(fractionalStorageAfterRead.next());
    QCOMPARE(fractionalStorageAfterRead.value(0).toString(),
             QStringLiteral("real"));
}

void NextPlatformApplicationServicesClassAnalyticsDashboardReadPortTests::
rejectsInvalidRosterColumnPositionStorage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(
        directory.filePath(QStringLiteral("class-analytics-position.db"))));
    const auto classId = services.classService()->create(
        QStringLiteral("Malformed analytics position"));
    QVERIFY(classId);

    QSqlQuery insertColumn(services.databaseSession()->database());
    insertColumn.prepare(R"(
        INSERT INTO roster_columns (class_id, name, position, width)
        VALUES (?, ?, ?, 100)
    )");
    const auto addColumn = [&](const QString& name, const int position) {
        insertColumn.bindValue(0, *classId);
        insertColumn.bindValue(1, name);
        insertColumn.bindValue(2, position);
        return insertColumn.exec();
    };
    QVERIFY(addColumn(QStringLiteral("English"), 0));
    QVERIFY(addColumn(QStringLiteral("Korean"), 1));
    QVERIFY(addColumn(QStringLiteral("Notes"), 2));

    QSqlQuery insertCell(services.databaseSession()->database());
    insertCell.prepare(R"(
        INSERT INTO roster_data (class_id, row_index, col_index, value)
        VALUES (?, 0, ?, ?)
    )");
    const auto addCell = [&](const int column, const QString& value) {
        insertCell.bindValue(0, *classId);
        insertCell.bindValue(1, column);
        insertCell.bindValue(2, value);
        return insertCell.exec();
    };
    QVERIFY(addCell(0, QStringLiteral("Distinct English name")));
    QVERIFY(addCell(1, QStringLiteral("Distinct Korean name")));

    QSqlQuery makePositionMalformed(services.databaseSession()->database());
    makePositionMalformed.prepare(R"(
        UPDATE roster_columns
        SET position=?
        WHERE class_id=? AND name='English'
    )");
    makePositionMalformed.addBindValue(QStringLiteral("not-a-number"));
    makePositionMalformed.addBindValue(*classId);
    QVERIFY(makePositionMalformed.exec());

    QSqlQuery positionType(services.databaseSession()->database());
    positionType.prepare(R"(
        SELECT typeof(position)
        FROM roster_columns
        WHERE class_id=? AND name='English'
    )");
    positionType.addBindValue(*classId);
    QVERIFY(positionType.exec());
    QVERIFY(positionType.next());
    QCOMPARE(positionType.value(0).toString(), QStringLiteral("text"));

    const auto typedClassId = Domain::ClassId::fromString(
        std::to_string(*classId));
    QVERIFY(typedClassId);
    Platform::ApplicationServicesClassAnalyticsDashboardReadPort port(services);
    const auto result = port.readRosterNames(*typedClassId);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().recoverable);
    QVERIFY(!result.error().message.empty());
}

void NextPlatformApplicationServicesClassAnalyticsDashboardReadPortTests::
rejectsRosterRowsOutsideClassAnalyticsIntegerRange()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(
        directory.filePath(QStringLiteral("class-analytics-row-range.db"))));

    const auto intMaxClassId = services.classService()->create(
        QStringLiteral("INT_MAX roster row"));
    const auto aboveIntMaxClassId = services.classService()->create(
        QStringLiteral("Above INT_MAX roster row"));
    const auto fractionalClassId = services.classService()->create(
        QStringLiteral("Fractional roster row"));
    QVERIFY(intMaxClassId);
    QVERIFY(aboveIntMaxClassId);
    QVERIFY(fractionalClassId);

    QSqlQuery insertColumn(services.databaseSession()->database());
    insertColumn.prepare(R"(
        INSERT INTO roster_columns (class_id, name, position, width)
        VALUES (?, 'English', 0, 100)
    )");
    const auto addEnglishColumn = [&](const int ownerId) {
        insertColumn.bindValue(0, ownerId);
        return insertColumn.exec();
    };
    QVERIFY(addEnglishColumn(*intMaxClassId));
    QVERIFY(addEnglishColumn(*aboveIntMaxClassId));
    QVERIFY(addEnglishColumn(*fractionalClassId));

    QSqlQuery insertName(services.databaseSession()->database());
    insertName.prepare(R"(
        INSERT INTO roster_data (class_id, row_index, col_index, value)
        VALUES (?, ?, 0, ?)
    )");
    const auto addName = [&](const int ownerId, const QVariant& rowIndex,
                             const QString& name) {
        insertName.bindValue(0, ownerId);
        insertName.bindValue(1, rowIndex);
        insertName.bindValue(2, name);
        return insertName.exec();
    };
    const qlonglong intMax = std::numeric_limits<int>::max();
    QVERIFY(addName(*intMaxClassId, QVariant::fromValue<qlonglong>(intMax),
                    QStringLiteral("INT_MAX student")));
    QVERIFY(addName(*aboveIntMaxClassId,
                    QVariant::fromValue<qlonglong>(intMax + 1),
                    QStringLiteral("Above INT_MAX student")));
    QVERIFY(addName(*fractionalClassId, QVariant::fromValue(1.5),
                    QStringLiteral("Fractional student")));
    QVERIFY(addName(*fractionalClassId, QVariant::fromValue<qlonglong>(2),
                    QStringLiteral("Valid maximum student")));

    const auto intMaxTypedClassId = Domain::ClassId::fromString(
        std::to_string(*intMaxClassId));
    const auto aboveIntMaxTypedClassId = Domain::ClassId::fromString(
        std::to_string(*aboveIntMaxClassId));
    const auto fractionalTypedClassId = Domain::ClassId::fromString(
        std::to_string(*fractionalClassId));
    QVERIFY(intMaxTypedClassId);
    QVERIFY(aboveIntMaxTypedClassId);
    QVERIFY(fractionalTypedClassId);

    Platform::ApplicationServicesClassAnalyticsDashboardReadPort port(services);
    const std::string expectedMessage =
        "Roster data contains a row index outside the Class Analytics range.";

    const auto intMaxResult = port.readRosterNames(*intMaxTypedClassId);
    QVERIFY(!intMaxResult);
    QCOMPARE(intMaxResult.error().code, Domain::ErrorCode::Technical);
    QVERIFY(intMaxResult.error().recoverable);
    QVERIFY(intMaxResult.error().message == expectedMessage);

    const auto aboveIntMaxResult = port.readRosterNames(*aboveIntMaxTypedClassId);
    QVERIFY(!aboveIntMaxResult);
    QCOMPARE(aboveIntMaxResult.error().code, Domain::ErrorCode::Technical);
    QVERIFY(aboveIntMaxResult.error().recoverable);
    QVERIFY(aboveIntMaxResult.error().message == expectedMessage);

    const auto fractionalResult = port.readRosterNames(*fractionalTypedClassId);
    QVERIFY(!fractionalResult);
    QCOMPARE(fractionalResult.error().code, Domain::ErrorCode::Technical);
    QVERIFY(fractionalResult.error().recoverable);
    QVERIFY(fractionalResult.error().message == expectedMessage);
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

    QSqlQuery dropRosterData(services.databaseSession()->database());
    QVERIFY(dropRosterData.exec(QStringLiteral("DROP TABLE roster_data")));
    Platform::ApplicationServicesClassAnalyticsDashboardReadPort port(services);

    QSqlQuery removeRosterColumns(services.databaseSession()->database());
    removeRosterColumns.prepare(
        QStringLiteral("DELETE FROM roster_columns WHERE class_id=?"));
    removeRosterColumns.addBindValue(*classId);
    QVERIFY(removeRosterColumns.exec());
    const auto emptyRoster = port.readRosterNames(*typedClassId);
    QVERIFY(emptyRoster);
    QCOMPARE(emptyRoster.value().rowCount, std::size_t{0});

    QSqlQuery insertRosterColumn(services.databaseSession()->database());
    insertRosterColumn.prepare(R"(
        INSERT INTO roster_columns (class_id, name, position, width)
        VALUES (?, 'English', 0, 100)
    )");
    insertRosterColumn.addBindValue(*classId);
    QVERIFY(insertRosterColumn.exec());
    const auto rosterFailure = port.readRosterNames(*typedClassId);
    QVERIFY(!rosterFailure);
    QCOMPARE(rosterFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!rosterFailure.error().message.empty());

    QSqlQuery drop(services.databaseSession()->database());
    QVERIFY(drop.exec(QStringLiteral("DROP TABLE speaking_eval_data")));
    const auto failed = port.readEvaluationBatch(*typedClassId);
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());
}

QTEST_MAIN(NextPlatformApplicationServicesClassAnalyticsDashboardReadPortTests)

#include "next_platform_application_services_class_analytics_dashboard_read_port_tests.moc"
