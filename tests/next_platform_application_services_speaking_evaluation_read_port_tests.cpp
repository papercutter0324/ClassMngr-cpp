#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/speaking_eval_repository.h"
#include "next/application/speaking_evaluation_query.h"
#include "next/application/speaking_evaluation_batch_read_query.h"
#include "next/application/speaking_evaluation_roster_score_import_batch_use_case.h"
#include "next/platform/application_services_speaking_evaluation_batch_read_port.h"
#include "next/platform/application_services_speaking_evaluation_read_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("speaking-evaluation-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Application::SpeakingEvaluationReadQuery query(
    const int classId,
    std::u16string evaluationName
    )
{
    const auto typedId = Domain::ClassId::fromString(std::to_string(classId));
    if (!typedId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    return {
        .classId = *typedId,
        .evaluationName = std::move(evaluationName)
    };
}

Application::SpeakingEvaluationReadBatchQuery batchQuery(
    const int classId,
    std::vector<std::u16string> names
    )
{
    const auto typedId = Domain::ClassId::fromString(std::to_string(classId));
    if (!typedId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    return {
        .classId = *typedId,
        .evaluationNames = std::move(names)
    };
}

SpeakingEvalRows makeRows()
{
    SpeakingEvalRows rows = SpeakingEval::emptyRows();
    for (int row = 0; row < rows.size(); ++row)
    {
        rows[row][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
            QStringLiteral("Student %1").arg(QChar(u'A' + row));
        rows[row][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
            QStringLiteral("\uAE40\uBBFC\uC9C0");
    }
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("\uAE40\uBBFC\uC9C0");
    rows[7][SpeakingEval::toInt(SpeakingEvalColumn::Comments)] =
        QStringLiteral("Review \U0001F4DA");
    rows[24][SpeakingEval::toInt(SpeakingEvalColumn::Notes)] =
        QStringLiteral("\uC218\uC5C5 \U0001F4DA");
    return rows;
}

}

class NextPlatformApplicationServicesSpeakingEvaluationReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsExactNameAndPreservesOrderedUnicodeMatrix();
    void reportsDeletedParentEvenWhenOrphanEvaluationRowsRemain();
    void reportsNeverPresentParentAsNotFound();
    void readsExistingEvaluationHeaderWithNoRowsAsEmpty();
    void readsBatchWithRequestedOrderAndSuccessfulEmptyOutcomes();
    void rejectsFractionalRowIndexesInTypedReads();
    void emptyBatchDoesNotRequireAnActiveSession();
    void rejectsNoncanonicalClassIdsBeforeSessionAccess();
    void mapsRepositoryErrorsToTechnicalFailures();
    void closedSessionFailsWithoutDataServiceFallback();
};

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
readsExactNameAndPreservesOrderedUnicodeMatrix()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Read Test")
        );
    QVERIFY(createdClass);

    const SpeakingEvalRows expected = makeRows();
    SpeakingEvalRepository* const repository =
        services.databaseSession()->speakingEvalRepository();
    QVERIFY(repository->saveSpeakingEval(
        *createdClass,
        QStringLiteral("Winter"),
        expected
        ));

    Platform::ApplicationServicesSpeakingEvaluationReadPort port(services);
    auto result = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass, u"Winter"),
        port
        );

    QVERIFY2(
        result,
        qPrintable(result ? QString() : QString::fromStdString(result.error().message))
        );
    QCOMPARE(result.value().rows.size(), std::size_t(25));
    QCOMPARE(result.value().rows[0].size(), std::size_t(11));
    QCOMPARE(result.value().rows[0][1], std::u16string(u"Student A"));
    QCOMPARE(result.value().rows[0][2], std::u16string(u"\uAE40\uBBFC\uC9C0"));
    QCOMPARE(result.value().rows[1][1], std::u16string(u"Student B"));
    QCOMPARE(result.value().rows[7][1], std::u16string(u"Student H"));
    QCOMPARE(result.value().rows[7][9], std::u16string(u"Review \U0001F4DA"));
    QCOMPARE(result.value().rows[24][1], std::u16string(u"Student Y"));
    QCOMPARE(result.value().rows[24][10], std::u16string(u"\uC218\uC5C5 \U0001F4DA"));

    const auto exactNameMiss = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass, u" Winter "),
        port
        );
    QVERIFY(exactNameMiss);
    QVERIFY(exactNameMiss.value().rows.empty());
    QCOMPARE(exactNameMiss.value().evaluationName, std::u16string(u" Winter "));
}

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
reportsDeletedParentEvenWhenOrphanEvaluationRowsRemain()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Orphan Read Test")
        );
    QVERIFY(createdClass);

    SpeakingEvalRepository* const repository =
        services.databaseSession()->speakingEvalRepository();
    QVERIFY(repository->saveSpeakingEval(
        *createdClass,
        QStringLiteral("Winter"),
        makeRows()
        ));

    QSqlQuery disableForeignKeys(services.databaseSession()->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));

    QSqlQuery deleteParent(services.databaseSession()->database());
    deleteParent.prepare(QStringLiteral("DELETE FROM classes WHERE id=?"));
    deleteParent.addBindValue(*createdClass);
    QVERIFY(deleteParent.exec());
    QCOMPARE(deleteParent.numRowsAffected(), 1);

    QSqlQuery orphanCounts(services.databaseSession()->database());
    orphanCounts.prepare(QStringLiteral(R"(
        SELECT
            (SELECT COUNT(*) FROM speaking_evaluations WHERE class_id=?)
                AS evaluation_count,
            (SELECT COUNT(*) FROM speaking_eval_data WHERE evaluation_id=(
                SELECT id FROM speaking_evaluations
                WHERE class_id=? AND evaluation_name=?
            )) AS data_count
    )"));
    orphanCounts.addBindValue(*createdClass);
    orphanCounts.addBindValue(*createdClass);
    orphanCounts.addBindValue(QStringLiteral("Winter"));
    QVERIFY(orphanCounts.exec());
    QVERIFY(orphanCounts.next());
    QCOMPARE(orphanCounts.value("evaluation_count").toInt(), 1);
    QCOMPARE(orphanCounts.value("data_count").toInt(), 25);
    orphanCounts.finish();

    // The legacy API keeps its historical child-table behavior. The typed
    // read uses its parent-rooted result query and classifies this ID as absent.
    const auto legacyRead = repository->loadSpeakingEval(
        *createdClass,
        QStringLiteral("Winter")
        );
    QVERIFY(legacyRead);
    QCOMPARE(legacyRead->size(), 25);
    QCOMPARE(legacyRead->at(0).size(), 11);
    QCOMPARE(legacyRead->at(0).at(1), QStringLiteral("Student A"));

    const auto legacyBatchRead = repository->loadSpeakingEvalBatch(
        *createdClass,
        {QStringLiteral("Winter")}
        );
    QVERIFY(legacyBatchRead);
    QCOMPARE(legacyBatchRead->size(), 1);
    QCOMPARE(legacyBatchRead->at(0).rows.size(), 25);
    QCOMPARE(
        legacyBatchRead->at(0).rows[0][1],
        QStringLiteral("Student A")
        );

    Platform::ApplicationServicesSpeakingEvaluationReadPort port(services);
    const auto typedRead = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass, u"Winter"),
        port
        );
    QVERIFY(!typedRead);
    QCOMPARE(typedRead.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(typedRead.error().recoverable);

    Platform::ApplicationServicesSpeakingEvaluationBatchReadPort batchPort(services);
    const auto typedBatchRead =
        Application::SpeakingEvaluationReadBatchQueryHandler::execute(
            batchQuery(*createdClass, {u"Winter"}),
            batchPort
            );
    QVERIFY(!typedBatchRead);
    QCOMPARE(typedBatchRead.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(typedBatchRead.error().recoverable);
}

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
reportsNeverPresentParentAsNotFound()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Missing Parent Test")
        );
    QVERIFY(createdClass);

    Platform::ApplicationServicesSpeakingEvaluationReadPort port(services);
    const auto typedRead = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass + 1, u"Winter"),
        port
        );
    QVERIFY(!typedRead);
    QCOMPARE(typedRead.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(typedRead.error().recoverable);

    Platform::ApplicationServicesSpeakingEvaluationBatchReadPort batchPort(services);
    const auto typedBatchRead =
        Application::SpeakingEvaluationReadBatchQueryHandler::execute(
            batchQuery(*createdClass + 1, {u"Winter"}),
            batchPort
            );
    QVERIFY(!typedBatchRead);
    QCOMPARE(typedBatchRead.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(typedBatchRead.error().recoverable);
}

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
readsExistingEvaluationHeaderWithNoRowsAsEmpty()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Empty Header Test")
        );
    QVERIFY(createdClass);

    QSqlQuery insertHeader(services.databaseSession()->database());
    insertHeader.prepare(QStringLiteral(R"(
        INSERT INTO speaking_evaluations (class_id, evaluation_name)
        VALUES (?, ?)
    )"));
    insertHeader.addBindValue(*createdClass);
    insertHeader.addBindValue(QStringLiteral("Header Only"));
    QVERIFY(insertHeader.exec());

    Platform::ApplicationServicesSpeakingEvaluationReadPort port(services);
    const auto typedRead = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass, u"Header Only"),
        port
        );
    QVERIFY(typedRead);
    QVERIFY(typedRead.value().rows.empty());
}

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
readsBatchWithRequestedOrderAndSuccessfulEmptyOutcomes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Batch Read Test")
        );
    QVERIFY(createdClass);

    SpeakingEvalRepository* const repository =
        services.databaseSession()->speakingEvalRepository();
    SpeakingEvalRows batchRows = makeRows();
    for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
    {
        batchRows[3][column] = QStringLiteral("Stored cell %1").arg(column);
    }
    QVERIFY(repository->saveSpeakingEval(
        *createdClass,
        QStringLiteral("Winter"),
        batchRows
        ));

    QSqlQuery createEmptyEvaluation(services.databaseSession()->database());
    createEmptyEvaluation.prepare(QStringLiteral(
        "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
        "VALUES (?, ?)"
        ));
    createEmptyEvaluation.addBindValue(*createdClass);
    createEmptyEvaluation.addBindValue(QStringLiteral("Speech Contest"));
    QVERIFY(createEmptyEvaluation.exec());

    Platform::ApplicationServicesSpeakingEvaluationBatchReadPort port(services);
    const Application::SpeakingEvaluationReadBatchQuery requested = batchQuery(
        *createdClass,
        {u"Speech Contest", u"Missing", u"Winter", u"Winter", u" Winter "}
        );
    const auto result = Application::SpeakingEvaluationReadBatchQueryHandler::execute(
        requested,
        port
        );

    QVERIFY2(
        result,
        qPrintable(result ? QString() : QString::fromStdString(result.error().message))
        );
    QCOMPARE(result.value().evaluations.size(), std::size_t(5));
    for (std::size_t index = 0; index < requested.evaluationNames.size(); ++index)
    {
        QVERIFY(result.value().evaluations[index].classId == requested.classId);
        QCOMPARE(
            result.value().evaluations[index].evaluationName,
            requested.evaluationNames[index]
            );
    }
    QVERIFY(result.value().evaluations[0].rows.empty());
    QVERIFY(result.value().evaluations[1].rows.empty());
    const auto& winterRows = result.value().evaluations[2].rows;
    QCOMPARE(winterRows.size(), std::size_t(25));
    QCOMPARE(winterRows[0].size(), std::size_t(11));
    QVERIFY(winterRows[0][0].empty());
    QCOMPARE(winterRows[0][1], std::u16string(u"Student A"));
    QCOMPARE(winterRows[1][1], std::u16string(u"Student B"));
    for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
    {
        QCOMPARE(
            winterRows[3][static_cast<std::size_t>(column)],
            QStringLiteral("Stored cell %1").arg(column).toStdU16String()
            );
    }
    QCOMPARE(winterRows[7][9], std::u16string(u"Review \U0001F4DA"));
    QCOMPARE(winterRows[24][10], std::u16string(u"\uC218\uC5C5 \U0001F4DA"));
    QCOMPARE(result.value().evaluations[3].rows, winterRows);
    QVERIFY(result.value().evaluations[4].rows.empty());

    // Exact-name lookup remains class-scoped and does not trim the requested
    // name, matching the existing single-evaluation query contract.
    const auto exactNameMiss = Application::SpeakingEvaluationReadBatchQueryHandler::execute(
        batchQuery(*createdClass, {u" Winter "}),
        port
        );
    QVERIFY(exactNameMiss);
    QVERIFY(exactNameMiss.value().evaluations[0].rows.empty());

    const auto otherClass = services.classService()->create(
        QStringLiteral("Other Speaking Evaluation Batch Read Test")
        );
    QVERIFY(otherClass);
    const auto classScopedMiss =
        Application::SpeakingEvaluationReadBatchQueryHandler::execute(
            batchQuery(*otherClass, {u"Winter"}),
            port
            );
    QVERIFY(classScopedMiss);
    QVERIFY(classScopedMiss.value().evaluations[0].rows.empty());

    Platform::ApplicationServicesSpeakingEvaluationReadPort singlePort(services);
    const auto isolatedFallback = Application::
        SpeakingEvaluationRosterScoreImportBatchUseCase::execute(
            batchQuery(*createdClass, {u"   ", u"Winter"}),
            port,
            singlePort
            );
    QVERIFY(isolatedFallback);
    QCOMPARE(isolatedFallback.value().size(), std::size_t(2));
    QVERIFY(!isolatedFallback.value()[0]);
    QVERIFY(isolatedFallback.value()[1]);
    QCOMPARE(isolatedFallback.value()[1].value().size(), std::size_t(25));
    QCOMPARE(
        isolatedFallback.value()[1].value()[0].englishName,
        std::u16string(u"Student A")
        );
}

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
rejectsFractionalRowIndexesInTypedReads()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Fractional Row Read Test")
        );
    QVERIFY(createdClass);

    SpeakingEvalRepository* const repository =
        services.databaseSession()->speakingEvalRepository();
    QVERIFY(repository->saveSpeakingEval(
        *createdClass,
        QStringLiteral("Winter"),
        makeRows()
        ));

    QSqlQuery insertFractionalRow(services.databaseSession()->database());
    insertFractionalRow.prepare(QStringLiteral(R"(
        INSERT INTO speaking_eval_data (
            evaluation_id,
            row_index,
            col_1
        )
        SELECT id, 0.25, 'Fractional student'
        FROM speaking_evaluations
        WHERE class_id=? AND evaluation_name=?
    )"));
    insertFractionalRow.addBindValue(*createdClass);
    insertFractionalRow.addBindValue(QStringLiteral("Winter"));
    QVERIFY(insertFractionalRow.exec());
    QCOMPARE(insertFractionalRow.numRowsAffected(), 1);

    QSqlQuery storedTypes(services.databaseSession()->database());
    storedTypes.prepare(QStringLiteral(R"(
        SELECT row_index, typeof(row_index) AS storage_type
        FROM speaking_eval_data
        WHERE evaluation_id=(
            SELECT id FROM speaking_evaluations
            WHERE class_id=? AND evaluation_name=?
        ) AND row_index IN (0, 0.25, 1, 24)
        ORDER BY row_index
    )"));
    storedTypes.addBindValue(*createdClass);
    storedTypes.addBindValue(QStringLiteral("Winter"));
    QVERIFY(storedTypes.exec());
    QVERIFY(storedTypes.next());
    QCOMPARE(storedTypes.value("row_index").toInt(), 0);
    QCOMPARE(storedTypes.value("storage_type").toString(), QStringLiteral("integer"));
    QVERIFY(storedTypes.next());
    QCOMPARE(storedTypes.value("row_index").toDouble(), 0.25);
    QCOMPARE(storedTypes.value("storage_type").toString(), QStringLiteral("real"));
    QVERIFY(storedTypes.next());
    QCOMPARE(storedTypes.value("row_index").toInt(), 1);
    QCOMPARE(storedTypes.value("storage_type").toString(), QStringLiteral("integer"));
    QVERIFY(storedTypes.next());
    QCOMPARE(storedTypes.value("row_index").toInt(), 24);
    QCOMPARE(storedTypes.value("storage_type").toString(), QStringLiteral("integer"));
    QVERIFY(!storedTypes.next());

    Platform::ApplicationServicesSpeakingEvaluationReadPort singlePort(services);
    const auto single = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass, u"Winter"),
        singlePort
        );
    QVERIFY(!single);
    QCOMPARE(single.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(single.error().message).contains(
        QStringLiteral("invalid row index storage type")
        ));

    Platform::ApplicationServicesSpeakingEvaluationBatchReadPort batchPort(services);
    const auto batch = Application::SpeakingEvaluationReadBatchQueryHandler::execute(
        batchQuery(*createdClass, {u"Winter"}),
        batchPort
        );
    QVERIFY(!batch);
    QCOMPARE(batch.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(batch.error().message).contains(
        QStringLiteral("invalid row index storage type")
        ));
}

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
emptyBatchDoesNotRequireAnActiveSession()
{
    Platform::ApplicationServicesSpeakingEvaluationBatchReadPort port(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto result = port.readEvaluations(batchQuery(42, {}));

    QVERIFY(result);
    QVERIFY(result.value().evaluations.empty());
}

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
rejectsNoncanonicalClassIdsBeforeSessionAccess()
{
    ApplicationServices services;
    Platform::ApplicationServicesSpeakingEvaluationReadPort port(services);
    const std::vector<std::string> invalidIds{
        "0",
        "-1",
        "042",
        "12x",
        "2147483648"
    };

    for (const std::string& value : invalidIds)
    {
        const auto classId = Domain::ClassId::fromString(value);
        QVERIFY(classId);
        const auto result = port.readEvaluation({
            .classId = *classId,
            .evaluationName = u"Winter"
        });
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
}

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
mapsRepositoryErrorsToTechnicalFailures()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Read Failure Test")
        );
    QVERIFY(createdClass);

    Platform::ApplicationServicesSpeakingEvaluationReadPort port(services);
    const auto blankName = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass, u"   "),
        port
        );
    QVERIFY(!blankName);
    QCOMPARE(blankName.error().code, Domain::ErrorCode::Technical);
    QVERIFY(blankName.error().recoverable);
    QVERIFY(QString::fromStdString(blankName.error().message).contains(
        QStringLiteral("invalid class id or evaluation name")
        ));

    SpeakingEvalRepository* const repository =
        services.databaseSession()->speakingEvalRepository();
    QVERIFY(repository->saveSpeakingEval(
        *createdClass,
        QStringLiteral("Winter"),
        makeRows()
        ));
    QSqlQuery dropDataTable(services.databaseSession()->database());
    QVERIFY(dropDataTable.exec(QStringLiteral(
        "DROP TABLE speaking_eval_data"
        )));

    const auto failedRead = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass, u"Winter"),
        port
        );
    QVERIFY(!failedRead);
    QCOMPARE(failedRead.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedRead.error().recoverable);
    QVERIFY(!failedRead.error().message.empty());

    Platform::ApplicationServicesSpeakingEvaluationBatchReadPort batchPort(services);
    const auto failedBatchRead =
        Application::SpeakingEvaluationReadBatchQueryHandler::execute(
            batchQuery(*createdClass, {u"Winter"}),
            batchPort
            );
    QVERIFY(!failedBatchRead);
    QCOMPARE(failedBatchRead.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedBatchRead.error().recoverable);
    QVERIFY(!failedBatchRead.error().message.empty());
}

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
closedSessionFailsWithoutDataServiceFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Closed Speaking Evaluation Read Test")
        );
    QVERIFY(createdClass);

    services.closeDatabase();
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.dataService());
    QVERIFY(!services.dataService()->isOpen());

    Platform::ApplicationServicesSpeakingEvaluationReadPort port(services);
    const auto result = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass, u"Winter"),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("unavailable")
        ));
}

QTEST_MAIN(NextPlatformApplicationServicesSpeakingEvaluationReadPortTests)

#include "next_platform_application_services_speaking_evaluation_read_port_tests.moc"
