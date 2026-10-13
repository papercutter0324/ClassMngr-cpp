#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/class_transfer_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_transfer.h"
#include "next/application/class_transfer_apply.h"
#include "next/platform/application_services_class_transfer_apply_port.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{
QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(QStringLiteral("class-transfer-apply-%1.tps")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

Domain::TeacherId teacherId(const int value)
{
    return *Domain::TeacherId::fromString(std::to_string(value));
}

ClassTransferPackage validPackage()
{
    ClassTransferPackage package;
    ClassTransferTeacher transferTeacher;
    transferTeacher.key = QStringLiteral("teacher-1");
    transferTeacher.teacher.teacherEn = QStringLiteral(" Alex  Kim ");
    transferTeacher.teacher.preferredRomanization = QStringLiteral("A. Kim");
    transferTeacher.teacher.roomNumber = QStringLiteral(" 405 ");
    transferTeacher.teacher.phoneNumber = QStringLiteral("01012345678");
    package.teachers.append(transferTeacher);

    ClassTransferClass transferClass;
    transferClass.key = QStringLiteral("class-1");
    transferClass.name = QStringLiteral("Imported Class");
    transferClass.teacherKey = transferTeacher.key;
    transferClass.info.classGrade = QStringLiteral("E4");
    transferClass.info.classLevel = QStringLiteral("Theseus");
    package.classes.append(transferClass);
    return package;
}

Application::ClassTransferApplyCommand createCommand(
    ClassTransferPackage package = validPackage()
    )
{
    Application::ClassTransferApplyRequest choices;
    choices.classes.push_back({
        0,
        Application::ClassTransferReviewClassAction::Create,
        std::nullopt
    });
    choices.teachers.push_back({
        "teacher-1",
        Application::ClassTransferReviewTeacherAction::Create,
        std::nullopt
    });
    return {.package = std::move(package), .choices = std::move(choices)};
}

int count(QSqlDatabase database, const QString& table)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

bool insertLocalClass(
    ApplicationServices& services,
    const QString& name,
    const int teacher,
    int* classIdOut
    )
{
    ClassRepository* const classes = services.databaseSession()->classRepository();
    if (!classes)
    {
        return false;
    }
    const auto created = classes->createClass(name);
    if (!created)
    {
        return false;
    }
    *classIdOut = *created;
    QSqlQuery info(services.databaseSession()->database());
    info.prepare(QStringLiteral(
        "INSERT INTO class_info (class_id, teacher_id, class_grade, class_level) "
        "VALUES (?, ?, 'E4', 'Theseus')"));
    info.addBindValue(*created);
    info.addBindValue(teacher > 0 ? QVariant(teacher) : QVariant());
    return info.exec();
}
}

class NextPlatformApplicationServicesClassTransferApplyPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void rejectsUnavailableSessionWithoutFallback();
    void rejectsInvalidPayloadWithoutWrites();
    void rejectsStaleChoiceWithoutWrites();
    void rejectsOnceValidChoiceAfterDestinationChanges();
    void preflightsScheduleConflictBeforeWrites();
    void normalizesOwnedPackageWithoutChangingArrayOrder();
    void createsWithNormalizedPayloadAndTypedSummary();
    void replacesClassAndTeacherThroughRepository();
    void importsSpeakingEvaluationWithWideGeneratedId();
    void lateWriteFailureRollsBackTransferRows();
};

void NextPlatformApplicationServicesClassTransferApplyPortTests::
rejectsUnavailableSessionWithoutFallback()
{
    auto command = createCommand();
    Platform::ApplicationServicesClassTransferApplyPort nullPort(
        static_cast<ApplicationServices*>(nullptr));
    const auto missing = nullPort.apply(command);
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, Domain::ErrorCode::NotFound);

    ApplicationServices services;
    Platform::ApplicationServicesClassTransferApplyPort port(services);
    const auto closed = port.apply(command);
    QVERIFY(!closed);
    QCOMPARE(closed.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!services.hasOpenDatabase());
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
rejectsInvalidPayloadWithoutWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    auto package = validPackage();
    package.teachers.first().teacher.teacherEn.clear();
    package.teachers.first().teacher.preferredRomanization.clear();
    package.teachers.first().teacher.teacherKr.clear();
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createCommand(std::move(package)));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("teachers")), 0);
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("classes")), 0);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
rejectsStaleChoiceWithoutWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    auto command = createCommand();
    command.choices.classes.front().targetClassId = classId(999);
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(command);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("teachers")), 0);
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("classes")), 0);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
rejectsOnceValidChoiceAfterDestinationChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Teacher local;
    local.teacherEn = QStringLiteral("Alex Kim");
    local.preferredName = QStringLiteral("Alex Kim");
    const auto localTeacher = services.databaseSession()->teacherRepository()
        ->createTeacher(local);
    QVERIFY(localTeacher);
    int localClass = -1;
    QVERIFY(insertLocalClass(services, QStringLiteral("Imported Class"),
        *localTeacher, &localClass));
    const auto preview = services.databaseSession()->classTransferRepository()
        ->previewImport(validPackage());
    QVERIFY(preview);
    QCOMPARE(preview->classes.first().matchingClassIds, QList<int>{localClass});
    QCOMPARE(preview->teachers.first().matchingTeacherIds, QList<qint64>{*localTeacher});

    auto command = createCommand();
    command.choices.classes.front().action =
        Application::ClassTransferReviewClassAction::Replace;
    command.choices.classes.front().targetClassId = classId(localClass);
    command.choices.teachers.front().action =
        Application::ClassTransferReviewTeacherAction::KeepExisting;
    command.choices.teachers.front().targetTeacherId = teacherId(*localTeacher);
    QVERIFY(services.databaseSession()->classRepository()->deleteClass(localClass));
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(command);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().message.find("replacement") != std::string::npos
        || result.error().message.find("class") != std::string::npos);
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("teachers")), 1);
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("classes")), 0);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
preflightsScheduleConflictBeforeWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    int localClass = -1;
    QVERIFY(insertLocalClass(services, QStringLiteral("Scheduled Class"), -1,
        &localClass));
    QSqlQuery schedule(services.databaseSession()->database());
    schedule.prepare(QStringLiteral(
        "INSERT INTO class_times (class_id, day, start_time, end_time) "
        "VALUES (?, 'Monday', '9:00 AM', '10:00 AM')"));
    schedule.addBindValue(localClass);
    QVERIFY(schedule.exec());
    auto package = validPackage();
    package.classes.first().info.classTimes.append({
        QStringLiteral("Monday"), QStringLiteral("9:00 AM"),
        QStringLiteral("10:00 AM")
    });
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createCommand(std::move(package)));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().message.find("Schedule conflicts") != std::string::npos);
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("teachers")), 0);
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("classes")), 1);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
createsWithNormalizedPayloadAndTypedSummary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createCommand());

    QVERIFY2(result, result ? "" : result.error().message.c_str());
    QCOMPARE(result.value().createdClassIds.size(), std::size_t(1));
    QVERIFY(result.value().replacedClassIds.empty());
    QCOMPARE(result.value().skippedClassCount, 0);
    const auto parsed = Application::classTransferApplyDestinationId(
        result.value().createdClassIds.front());
    QVERIFY(parsed.has_value());
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("teachers")), 1);
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("classes")), 1);
    const auto stored = services.databaseSession()->teacherRepository()->getTeacher(1);
    QVERIFY(stored);
    QCOMPARE(stored->teacherEn, QStringLiteral("Alex Kim"));
    QCOMPARE(stored->roomNumber, QStringLiteral("405"));
    QCOMPARE(stored->phoneNumber, QStringLiteral("010-1234-5678"));

    QVERIFY2(createCommand().package.teachers.first().teacher.teacherEn
        != stored->teacherEn,
        "fixture should require normalization");
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
normalizesOwnedPackageWithoutChangingArrayOrder()
{
    auto package = validPackage();
    ClassTransferClass secondClass = package.classes.first();
    secondClass.key = QStringLiteral("class-2");
    secondClass.name = QStringLiteral("Second Imported Class");
    package.classes.append(std::move(secondClass));

    const auto result = ClassTransferPackageValidator::normalizedAndValidated(
        std::move(package)
        );

    QVERIFY(result);
    QCOMPARE(result->teachers.size(), 1);
    QCOMPARE(result->teachers.first().key, QStringLiteral("teacher-1"));
    QCOMPARE(result->teachers.first().teacher.teacherEn,
        QStringLiteral("Alex Kim"));
    QCOMPARE(result->classes.size(), 2);
    QCOMPARE(result->classes.at(0).key, QStringLiteral("class-1"));
    QCOMPARE(result->classes.at(1).key, QStringLiteral("class-2"));
    QCOMPARE(result->classes.at(0).info.classId, 1);
    QCOMPARE(result->classes.at(0).info.classGrade, QStringLiteral("E4"));
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
replacesClassAndTeacherThroughRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Teacher local;
    local.teacherEn = QStringLiteral("Alex Kim");
    local.preferredName = QStringLiteral("Alex Kim");
    local.roomNumber = QStringLiteral("Local Room");
    const auto localTeacher = services.databaseSession()->teacherRepository()
        ->createTeacher(local);
    QVERIFY(localTeacher);
    int localClass = -1;
    QVERIFY(insertLocalClass(services, QStringLiteral("Imported Class"),
        *localTeacher, &localClass));
    const auto preview = services.databaseSession()->classTransferRepository()
        ->previewImport(validPackage());
    QVERIFY(preview);
    QCOMPARE(preview->classes.first().matchingClassIds, QList<int>{localClass});
    QCOMPARE(preview->teachers.first().matchingTeacherIds, QList<qint64>{*localTeacher});

    auto command = createCommand();
    command.choices.classes.front().action =
        Application::ClassTransferReviewClassAction::Replace;
    command.choices.classes.front().targetClassId = classId(localClass);
    command.choices.teachers.front().action =
        Application::ClassTransferReviewTeacherAction::ReplaceExisting;
    command.choices.teachers.front().targetTeacherId = teacherId(*localTeacher);
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(command);

    QVERIFY2(result, result ? "" : result.error().message.c_str());
    QVERIFY(result.value().createdClassIds.empty());
    QCOMPARE(result.value().replacedClassIds.size(), std::size_t(1));
    QCOMPARE(result.value().replacedClassIds.front().value(),
        std::to_string(localClass));
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("teachers")), 1);
    QCOMPARE(count(services.databaseSession()->database(), QStringLiteral("classes")), 1);
    const auto replaced = services.databaseSession()->teacherRepository()
        ->getTeacher(*localTeacher);
    QVERIFY(replaced);
    QCOMPARE(replaced->roomNumber, QStringLiteral("405"));
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
importsSpeakingEvaluationWithWideGeneratedId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.databaseSession()->database();

    QSqlQuery seedClass(database);
    QVERIFY(seedClass.exec(QStringLiteral(
        "INSERT INTO classes (name) VALUES ('Evaluation Sequence Seed')")));
    const qint64 seedClassId = seedClass.lastInsertId().toLongLong();
    QVERIFY(seedClassId > 0);
    QSqlQuery seedEvaluation(database);
    seedEvaluation.prepare(QStringLiteral(
        "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
        "VALUES (?, 'Sequence Seed')"));
    seedEvaluation.addBindValue(seedClassId);
    QVERIFY(seedEvaluation.exec());

    QSqlQuery removeSeedClass(database);
    removeSeedClass.prepare(QStringLiteral(
        "DELETE FROM classes WHERE id = ?"));
    removeSeedClass.addBindValue(seedClassId);
    QVERIFY(removeSeedClass.exec());

    constexpr qint64 sequenceBeforeImport = 2147483648LL;
    QSqlQuery setEvaluationSequence(database);
    setEvaluationSequence.prepare(QStringLiteral(
        "UPDATE sqlite_sequence SET seq = ? "
        "WHERE name = 'speaking_evaluations'"));
    setEvaluationSequence.addBindValue(sequenceBeforeImport);
    QVERIFY(setEvaluationSequence.exec());
    QCOMPARE(setEvaluationSequence.numRowsAffected(), 1);

    auto package = validPackage();
    ClassTransferEvaluation evaluation;
    evaluation.name = QStringLiteral("Wide ID Evaluation");
    evaluation.rows = SpeakingEval::emptyRows();
    evaluation.rows[0][1] = QStringLiteral("Wide ID student");
    evaluation.rows[3][9] = QStringLiteral("Wide ID marker survives");
    package.classes.first().evaluations.append(std::move(evaluation));
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createCommand(std::move(package)));

    QVERIFY2(result, result ? "" : result.error().message.c_str());
    QCOMPARE(result.value().createdClassIds.size(), std::size_t(1));
    const auto importedClassId = Application::classTransferApplyDestinationId(
        result.value().createdClassIds.front());
    QVERIFY(importedClassId.has_value());

    QSqlQuery storedEvaluation(database);
    storedEvaluation.prepare(QStringLiteral(
        "SELECT id, typeof(id) FROM speaking_evaluations "
        "WHERE class_id = ? AND evaluation_name = ?"));
    storedEvaluation.addBindValue(*importedClassId);
    storedEvaluation.addBindValue(QStringLiteral("Wide ID Evaluation"));
    QVERIFY(storedEvaluation.exec());
    QVERIFY(storedEvaluation.next());
    constexpr qint64 expectedEvaluationId = sequenceBeforeImport + 1;
    QCOMPARE(storedEvaluation.value(0).toLongLong(), expectedEvaluationId);
    QCOMPARE(storedEvaluation.value(1).toString(), QStringLiteral("integer"));
    QVERIFY(!storedEvaluation.next());

    QSqlQuery storedRows(database);
    storedRows.prepare(QStringLiteral(
        "SELECT COUNT(*), MIN(evaluation_id), MAX(evaluation_id), "
        "MIN(typeof(evaluation_id)), MAX(typeof(evaluation_id)) "
        "FROM speaking_eval_data WHERE evaluation_id = ?"));
    storedRows.addBindValue(expectedEvaluationId);
    QVERIFY(storedRows.exec());
    QVERIFY(storedRows.next());
    QCOMPARE(storedRows.value(0).toInt(), SpeakingEval::RowCount);
    QCOMPARE(storedRows.value(1).toLongLong(), expectedEvaluationId);
    QCOMPARE(storedRows.value(2).toLongLong(), expectedEvaluationId);
    QCOMPARE(storedRows.value(3).toString(), QStringLiteral("integer"));
    QCOMPARE(storedRows.value(4).toString(), QStringLiteral("integer"));

    const auto readback = services.databaseSession()->classTransferRepository()
        ->readExportSource({*importedClassId});
    QVERIFY(readback);
    QCOMPARE(readback->classes.size(), 1);
    QCOMPARE(readback->classes.first().evaluations.size(), 1);
    const ClassTransferEvaluation& loadedEvaluation =
        readback->classes.first().evaluations.first();
    QCOMPARE(loadedEvaluation.name, QStringLiteral("Wide ID Evaluation"));
    QCOMPARE(loadedEvaluation.rows[0][1], QStringLiteral("Wide ID student"));
    QCOMPARE(loadedEvaluation.rows[3][9],
        QStringLiteral("Wide ID marker survives"));
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
lateWriteFailureRollsBackTransferRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.databaseSession()->database();
    QSqlQuery trigger(database);
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER fail_transfer_roster BEFORE INSERT ON roster_data "
        "BEGIN SELECT RAISE(ABORT, 'F367 transfer roster failure'); END")));
    auto package = validPackage();
    package.classes.first().roster.columns = {QStringLiteral("English")};
    package.classes.first().roster.columnWidths = {120};
    package.classes.first().roster.rows = {{QStringLiteral("A")}};
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createCommand(std::move(package)));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(count(database, QStringLiteral("teachers")), 0);
    QCOMPARE(count(database, QStringLiteral("classes")), 0);
    QCOMPARE(count(database, QStringLiteral("class_info")), 0);
    QCOMPARE(count(database, QStringLiteral("roster_columns")), 0);
    QCOMPARE(count(database, QStringLiteral("roster_data")), 0);
}

QTEST_MAIN(NextPlatformApplicationServicesClassTransferApplyPortTests)

#include "next_platform_application_services_class_transfer_apply_port_tests.moc"
