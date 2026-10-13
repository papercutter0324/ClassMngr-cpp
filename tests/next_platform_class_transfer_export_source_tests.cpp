#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "data/database/database_session.h"
#include "next/platform/application_services_class_transfer_export_source_read_port.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>
using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
namespace
{
Domain::ClassId id(int value) { return *Domain::ClassId::fromString(std::to_string(value)); }
}
class ClassTransferExportSourceTests : public QObject
{
    Q_OBJECT
private slots:
    void successfulSourceHasEveryRequiredStageAndSanitizedProjection();
    void wideEvaluationIdsKeepTheirOwnExportRows();
    void stagedErrorRollsBackAndKeepsDownstreamNotAttempted();
    void beginAndUnavailableErrorsStayAtSourceBoundary();
};
void ClassTransferExportSourceTests::successfulSourceHasEveryRequiredStageAndSanitizedProjection()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto* repository = services.databaseSession()->classTransferRepository();
    QSqlQuery insert(services.databaseSession()->database());
    QVERIFY(insert.exec(QStringLiteral("INSERT INTO classes(name) VALUES('Unassigned')")));
    const int value = insert.lastInsertId().toInt();
    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto source = port.readSource({{id(value)}}); QVERIFY(source);
    QCOMPARE(source.value().classes.size(), std::size_t(1));
    const auto& item = source.value().classes.front();
    QVERIFY(item.id == id(value)); QCOMPARE(item.info.state(), ClassExportStageState::Value);
    QCOMPARE(item.roster.state(), ClassExportStageState::Value);
    QCOMPARE(item.teacher.state(), ClassExportStageState::Value);
    QCOMPARE(item.evaluations.state(), ClassExportStageState::Value);
    QVERIFY(!item.teacher.value()); QVERIFY(source.value().teachers.empty());
    auto package = ClassTransferExportQuery(port).execute({{id(value)}}); QVERIFY(package);
    QVERIFY(package.value().classes[0].teacherKey.empty());
    QVERIFY(package.value().classes[0].info.classColor == u"#FFFFFF");
    auto legacy = repository->buildPackage({value}); QVERIFY(legacy);
    QCOMPARE(legacy->classes[0].info.classId, -1); QCOMPARE(legacy->classes[0].info.teacherId, -1);
    auto database = services.databaseSession()->database(); QVERIFY(database.transaction()); QVERIFY(database.rollback());
}
void ClassTransferExportSourceTests::wideEvaluationIdsKeepTheirOwnExportRows()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database();
    QSqlQuery insertClass(database);
    QVERIFY(insertClass.exec(QStringLiteral("INSERT INTO classes(name) VALUES('Wide Evaluation IDs')")));
    const int classId = insertClass.lastInsertId().toInt();

    struct EvaluationFixture
    {
        qlonglong id;
        QString name;
        QString marker;
    };
    const EvaluationFixture fixtures[] = {
        {2147483648LL, QStringLiteral("Wide evaluation A"), QStringLiteral("Marker A")},
        {2147483649LL, QStringLiteral("Wide evaluation B"), QStringLiteral("Marker B")}
    };
    for (const auto& fixture : fixtures)
    {
        QSqlQuery insertEvaluation(database);
        insertEvaluation.prepare(QStringLiteral(
            "INSERT INTO speaking_evaluations (id, class_id, evaluation_name) VALUES (?, ?, ?)"));
        insertEvaluation.addBindValue(QVariant::fromValue<qlonglong>(fixture.id));
        insertEvaluation.addBindValue(classId);
        insertEvaluation.addBindValue(fixture.name);
        QVERIFY2(insertEvaluation.exec(), qPrintable(insertEvaluation.lastError().text()));

        QSqlQuery insertRow(database);
        insertRow.prepare(QStringLiteral(
            "INSERT INTO speaking_eval_data (evaluation_id, row_index, col_0) VALUES (?, 0, ?)"));
        insertRow.addBindValue(QVariant::fromValue<qlonglong>(fixture.id));
        insertRow.addBindValue(fixture.marker);
        QVERIFY2(insertRow.exec(), qPrintable(insertRow.lastError().text()));
    }

    QSqlQuery storedIds(database);
    QVERIFY(storedIds.exec(QStringLiteral(
        "SELECT typeof(id), id FROM speaking_evaluations "
        "WHERE id IN (2147483648, 2147483649) ORDER BY id")));
    for (const auto& fixture : fixtures)
    {
        QVERIFY(storedIds.next());
        QCOMPARE(storedIds.value(0).toString(), QStringLiteral("integer"));
        QCOMPARE(storedIds.value(1).toLongLong(), fixture.id);
    }
    QVERIFY(!storedIds.next());

    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto package = ClassTransferExportQuery(port).execute({{id(classId)}});
    QVERIFY(package);
    QCOMPARE(package.value().classes.size(), std::size_t(1));
    const auto& evaluations = package.value().classes.front().evaluations;
    QCOMPARE(evaluations.size(), std::size_t(2));

    bool foundA = false;
    bool foundB = false;
    for (const auto& evaluation : evaluations)
    {
        QVERIFY(!evaluation.rows.empty());
        QVERIFY(!evaluation.rows.front().empty());
        if (evaluation.name == u"Wide evaluation A")
        {
            foundA = true;
            QCOMPARE(evaluation.rows.front().front(), std::u16string(u"Marker A"));
        }
        else if (evaluation.name == u"Wide evaluation B")
        {
            foundB = true;
            QCOMPARE(evaluation.rows.front().front(), std::u16string(u"Marker B"));
        }
        else
        {
            QFAIL("Unexpected exported evaluation name");
        }
    }
    QVERIFY(foundA);
    QVERIFY(foundB);
}
void ClassTransferExportSourceTests::stagedErrorRollsBackAndKeepsDownstreamNotAttempted()
{
    QTemporaryDir directory; QVERIFY(directory.isValid()); ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database(); QSqlQuery sql(database);
    QVERIFY(sql.exec(QStringLiteral("INSERT INTO classes(name) VALUES('First')"))); const int first = sql.lastInsertId().toInt();
    QVERIFY(sql.exec(QStringLiteral("INSERT INTO classes(name) VALUES('Second')"))); const int second = sql.lastInsertId().toInt();
    QVERIFY(sql.exec(QStringLiteral("DROP TABLE class_times")));
    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto source = port.readSource({{id(first), id(second), id(-1)}}); QVERIFY(source);
    QCOMPARE(source.value().classes.size(), std::size_t(2));
    QVERIFY(source.value().deferredSelectionOrLookupError);
    for (const auto& item : source.value().classes)
    {
        QCOMPARE(item.info.state(), ClassExportStageState::Error);
        QCOMPARE(item.roster.state(), ClassExportStageState::Value);
        QCOMPARE(item.teacher.state(), ClassExportStageState::NotAttempted);
        QCOMPARE(item.evaluations.state(), ClassExportStageState::NotAttempted);
    }
    auto replay = ClassTransferExportQuery(port).execute({{id(first), id(second), id(-1)}}); QVERIFY(!replay);
    QVERIFY(replay.error().message == source.value().classes[0].info.error().message);
    QVERIFY(database.transaction()); QVERIFY(database.rollback());
}
void ClassTransferExportSourceTests::beginAndUnavailableErrorsStayAtSourceBoundary()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassTransferExportSourceReadPort port(services);
    auto missing = port.readSource({{id(1)}}); QVERIFY(!missing);
    const auto legacy = services.classService()->buildTransferPackage({1}); QVERIFY(!legacy);
    QVERIFY(missing.error().message == "No Teacher Profile service is available.");
    QCOMPARE(QString::fromStdString(missing.error().message), legacy.error());
    QTemporaryDir directory; QVERIFY(directory.isValid()); QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("source.db"))));
    auto database = services.databaseSession()->database(); QVERIFY(database.transaction());
    auto result = port.readSource({{id(1)}}); QVERIFY(!result);
    QVERIFY(result.error().message.starts_with("Unable to start the class export transaction: "));
    QVERIFY(database.rollback());
}
QTEST_GUILESS_MAIN(ClassTransferExportSourceTests)
#include "next_platform_class_transfer_export_source_tests.moc"
