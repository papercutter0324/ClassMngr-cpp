#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/speaking_evaluation_save_use_case.h"
#include "next/platform/application_services_speaking_evaluation_save_port.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("speaking-evaluation-save-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return false;
    }

    QSqlQuery query(session->database());
    return query.exec(statement);
}

Application::SpeakingEvaluationSaveRequest request(const int classId)
{
    const auto typedId = Domain::ClassId::fromString(std::to_string(classId));
    if (!typedId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    Application::SpeakingEvaluationSnapshot snapshot;
    snapshot.rows.resize(Application::SpeakingEvaluationRowCount);
    for (int row = 0; row < Application::SpeakingEvaluationRowCount; ++row)
    {
        std::u16string englishName = u"Student ";
        englishName.push_back(static_cast<char16_t>(u'A' + row));

        std::vector<std::u16string>& cells =
            snapshot.rows[static_cast<std::size_t>(row)];
        cells.resize(Application::SpeakingEvaluationColumnCount);
        cells[1] = std::move(englishName);
        cells[2] = u"\uAE40\uBBFC\uC9C0";
        cells[3] = row % 2 == 0 ? u"A+" : u"B";
        cells[4] = u"A";
        cells[10] = row == 24 ? u"Last row" : u"";
    }
    snapshot.rows[0][9] = u"Review \U0001F4DA";
    snapshot.rows[3][10] = u"\uC218\uC5C5 \U0001F4DA";

    return {
        .classId = *typedId,
        .evaluationName = u"Winter",
        .evaluation = std::move(snapshot),
        .allowQuestionableKoreanNameLengths = false
    };
}

}

class NextPlatformApplicationServicesSpeakingEvaluationSavePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void savesCompleteMatrixAndAppliesExactChangedCellDelta();
    void normalizesNameAndScoreAliasBeforePersisting();
    void validationRejectsInvalidRowsBeforePersistence();
    void repositoryWriteFailureMapsTechnicalAndRollsBack();
    void forwardsQuestionableKoreanNameDecision();
    void closedSessionFailsWithoutDataServiceFallback();
};

void NextPlatformApplicationServicesSpeakingEvaluationSavePortTests::
savesCompleteMatrixAndAppliesExactChangedCellDelta()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Save Test")
        );
    QVERIFY(createdClass);

    Platform::ApplicationServicesSpeakingEvaluationSavePort port(services);
    Application::SpeakingEvaluationSaveRequest saveRequest = request(*createdClass);
    auto saved = Application::SpeakingEvaluationSaveUseCase::execute(
        saveRequest,
        port
        );
    QVERIFY2(
        saved,
        qPrintable(saved ? QString() : QString::fromStdString(saved.error().message))
        );

    const auto persisted = services.speakingEvaluationService()->evaluation(
        *createdClass,
        QStringLiteral("Winter")
        );
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 25);
    QCOMPARE(persisted->at(0).size(), 11);
    QCOMPARE(persisted->at(0).at(1), QStringLiteral("Student A"));
    QCOMPARE(persisted->at(0).at(2), QStringLiteral("\uAE40\uBBFC\uC9C0"));
    QCOMPARE(persisted->at(0).at(9), QStringLiteral("Review \U0001F4DA"));
    QCOMPARE(persisted->at(3).at(10),
        QStringLiteral("\uC218\uC5C5 \U0001F4DA"));
    QCOMPARE(persisted->at(24).at(1), QStringLiteral("Student Y"));
    QCOMPARE(persisted->at(24).at(10), QStringLiteral("Last row"));
    QVERIFY(persisted->at(0).at(10).isEmpty());

    saveRequest.evaluation.rows[0][10] = u"Unlisted in-memory edit";
    saveRequest.evaluation.rows[3][10] = u"Updated listed note";
    saveRequest.evaluation.changedCells = {{3, 10}};
    saved = Application::SpeakingEvaluationSaveUseCase::execute(
        saveRequest,
        port
        );
    QVERIFY2(
        saved,
        qPrintable(saved ? QString() : QString::fromStdString(saved.error().message))
        );

    const auto updated = services.speakingEvaluationService()->evaluation(
        *createdClass,
        QStringLiteral("Winter")
        );
    QVERIFY(updated);
    QVERIFY(updated->at(0).at(10).isEmpty());
    QCOMPARE(updated->at(3).at(10), QStringLiteral("Updated listed note"));
    QCOMPARE(updated->at(24).at(10), QStringLiteral("Last row"));
}

void NextPlatformApplicationServicesSpeakingEvaluationSavePortTests::
normalizesNameAndScoreAliasBeforePersisting()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Normalization Test")
        );
    QVERIFY(createdClass);

    Application::SpeakingEvaluationSaveRequest saveRequest = request(*createdClass);
    saveRequest.evaluationName = u"  Alias Review  ";
    saveRequest.evaluation.rows[0][3] = u" 5 ";

    Platform::ApplicationServicesSpeakingEvaluationSavePort port(services);
    const auto saved = Application::SpeakingEvaluationSaveUseCase::execute(
        saveRequest,
        port
        );
    QVERIFY2(
        saved,
        qPrintable(saved ? QString() : QString::fromStdString(saved.error().message))
        );

    const auto persisted = services.speakingEvaluationService()->evaluation(
        *createdClass,
        QStringLiteral("Alias Review")
        );
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 25);
    QCOMPARE(persisted->at(0).at(3), QStringLiteral("A+"));
}

void NextPlatformApplicationServicesSpeakingEvaluationSavePortTests::
validationRejectsInvalidRowsBeforePersistence()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Validation Test")
        );
    QVERIFY(createdClass);

    Platform::ApplicationServicesSpeakingEvaluationSavePort port(services);

    auto invalidNew = request(*createdClass);
    invalidNew.evaluationName = u"Invalid New Evaluation";
    invalidNew.evaluation.rows[0][3] = u"Not a score";
    const auto rejectedNew = Application::SpeakingEvaluationSaveUseCase::execute(
        invalidNew,
        port
        );
    QVERIFY(!rejectedNew);
    QCOMPARE(rejectedNew.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!rejectedNew.error().recoverable);

    const auto absentEvaluation = services.speakingEvaluationService()->evaluation(
        *createdClass,
        QStringLiteral("Invalid New Evaluation")
        );
    QVERIFY(absentEvaluation);
    QVERIFY(absentEvaluation->isEmpty());

    auto validExisting = request(*createdClass);
    validExisting.evaluationName = u"Existing Evaluation";
    const auto savedExisting = Application::SpeakingEvaluationSaveUseCase::execute(
        validExisting,
        port
        );
    QVERIFY(savedExisting);
    const auto beforeUpdate = services.speakingEvaluationService()->evaluation(
        *createdClass,
        QStringLiteral("Existing Evaluation")
        );
    QVERIFY(beforeUpdate);

    auto invalidUpdate = validExisting;
    invalidUpdate.evaluation.rows[0][3] = u"Not a score";
    invalidUpdate.evaluation.changedCells = {{0, 3}};
    const auto rejectedUpdate = Application::SpeakingEvaluationSaveUseCase::execute(
        invalidUpdate,
        port
        );
    QVERIFY(!rejectedUpdate);
    QCOMPARE(rejectedUpdate.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!rejectedUpdate.error().recoverable);

    const auto afterUpdate = services.speakingEvaluationService()->evaluation(
        *createdClass,
        QStringLiteral("Existing Evaluation")
        );
    QVERIFY(afterUpdate);
    QCOMPARE(*afterUpdate, *beforeUpdate);
}

void NextPlatformApplicationServicesSpeakingEvaluationSavePortTests::
repositoryWriteFailureMapsTechnicalAndRollsBack()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Repository Failure Test")
        );
    QVERIFY(createdClass);

    Platform::ApplicationServicesSpeakingEvaluationSavePort port(services);
    auto saveRequest = request(*createdClass);
    saveRequest.evaluationName = u"Rollback Evaluation";
    const auto initialSave = Application::SpeakingEvaluationSaveUseCase::execute(
        saveRequest,
        port
        );
    QVERIFY(initialSave);

    const auto beforeFailure = services.speakingEvaluationService()->evaluation(
        *createdClass,
        QStringLiteral("Rollback Evaluation")
        );
    QVERIFY(beforeFailure);
    QCOMPARE(beforeFailure->size(), 25);
    QCOMPARE(beforeFailure->at(0).at(3), QStringLiteral("A+"));

    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_speaking_evaluation_cell_update
            BEFORE UPDATE ON speaking_eval_data
            WHEN OLD.row_index = 0 AND NEW.col_3 = 'C'
            BEGIN
                SELECT RAISE(ABORT, 'forced speaking evaluation write failure');
            END
        )")
        ));

    saveRequest.evaluation.rows[0][3] = u"1";
    saveRequest.evaluation.changedCells = {{0, 3}};
    const auto rejectedSave = Application::SpeakingEvaluationSaveUseCase::execute(
        saveRequest,
        port
        );
    QVERIFY(!rejectedSave);
    QCOMPARE(rejectedSave.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!rejectedSave.error().recoverable);
    QVERIFY(!rejectedSave.error().message.empty());

    const auto afterFailure = services.speakingEvaluationService()->evaluation(
        *createdClass,
        QStringLiteral("Rollback Evaluation")
        );
    QVERIFY(afterFailure);
    QCOMPARE(*afterFailure, *beforeFailure);
}

void NextPlatformApplicationServicesSpeakingEvaluationSavePortTests::
forwardsQuestionableKoreanNameDecision()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Name Length Test")
        );
    QVERIFY(createdClass);

    Application::SpeakingEvaluationSaveRequest saveRequest = request(*createdClass);
    for (std::vector<std::u16string>& row : saveRequest.evaluation.rows)
    {
        std::fill(row.begin(), row.end(), std::u16string{});
    }
    saveRequest.evaluation.rows[0][1] = u"Alice";
    saveRequest.evaluation.rows[0][2] = u"\uAE40";

    Platform::ApplicationServicesSpeakingEvaluationSavePort port(services);
    auto saved = Application::SpeakingEvaluationSaveUseCase::execute(
        saveRequest,
        port
        );
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);

    saveRequest.allowQuestionableKoreanNameLengths = true;
    saved = Application::SpeakingEvaluationSaveUseCase::execute(
        saveRequest,
        port
        );
    QVERIFY2(
        saved,
        qPrintable(saved ? QString() : QString::fromStdString(saved.error().message))
        );

    const auto persisted = services.speakingEvaluationService()->evaluation(
        *createdClass,
        QStringLiteral("Winter")
        );
    QVERIFY(persisted);
    QCOMPARE(persisted->at(0).at(2), QStringLiteral("\uAE40"));
}

void NextPlatformApplicationServicesSpeakingEvaluationSavePortTests::
closedSessionFailsWithoutDataServiceFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Closed Speaking Evaluation Save Test")
        );
    QVERIFY(createdClass);

    services.closeDatabase();
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.dataService());
    QVERIFY(!services.dataService()->isOpen());

    Platform::ApplicationServicesSpeakingEvaluationSavePort port(services);
    const auto result = Application::SpeakingEvaluationSaveUseCase::execute(
        request(*createdClass),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("unavailable")
        ));
}

QTEST_MAIN(NextPlatformApplicationServicesSpeakingEvaluationSavePortTests)

#include "next_platform_application_services_speaking_evaluation_save_port_tests.moc"
