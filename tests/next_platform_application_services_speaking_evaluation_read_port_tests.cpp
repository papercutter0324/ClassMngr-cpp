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
    void readsBatchWithRequestedOrderAndSuccessfulEmptyOutcomes();
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
        {u"Speech Contest", u"Missing", u"Winter"}
        );
    const auto result = Application::SpeakingEvaluationReadBatchQueryHandler::execute(
        requested,
        port
        );

    QVERIFY2(
        result,
        qPrintable(result ? QString() : QString::fromStdString(result.error().message))
        );
    QCOMPARE(result.value().evaluations.size(), std::size_t(3));
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
    for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
    {
        QCOMPARE(
            winterRows[3][static_cast<std::size_t>(column)],
            QStringLiteral("Stored cell %1").arg(column).toStdU16String()
            );
    }
    QCOMPARE(winterRows[7][9], std::u16string(u"Review \U0001F4DA"));
    QCOMPARE(winterRows[24][10], std::u16string(u"\uC218\uC5C5 \U0001F4DA"));

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
