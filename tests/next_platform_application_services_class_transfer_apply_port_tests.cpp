#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/class_transfer_legacy_package_adapter.h"
#include "data/repositories/class_transfer_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_transfer.h"
#include "next/application/class_transfer_apply.h"
#include "next/platform/application_services_class_transfer_apply_port.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTranslator>
#include <QUuid>
#include <QtTest/QtTest>

#include <limits>
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
        Application::ClassTransferPackageClassIndex{0},
        Application::ClassTransferReviewClassAction::Create,
        std::nullopt
    });
    choices.teachers.push_back({
        Application::ClassTransferTeacherKey{"teacher-1"},
        Application::ClassTransferReviewTeacherAction::Create,
        std::nullopt
    });
    return {.package = ClassMngr::Data::Legacy::toPortableClassTransferPackage(
        std::move(package)), .choices = std::move(choices)};
}

Application::ClassTransferApplyCommand createManyCreatesCommand(
    const int classCount
    )
{
    ClassTransferPackage package = validPackage();
    const ClassTransferTeacher& transferTeacher = package.teachers.first();
    const QString teacherKey = transferTeacher.key;
    package.classes.clear();

    Application::ClassTransferApplyRequest choices;
    choices.classes.reserve(static_cast<std::size_t>(classCount));
    for (int index = 0; index < classCount; ++index)
    {
        ClassTransferClass transferClass;
        transferClass.key = QStringLiteral("class-%1").arg(index);
        transferClass.name = QStringLiteral("Imported Class %1").arg(index);
        transferClass.teacherKey = teacherKey;
        transferClass.info.classGrade = QStringLiteral("E4");
        transferClass.info.classLevel = QStringLiteral("Theseus");
        package.classes.append(std::move(transferClass));
        choices.classes.push_back({
            Application::ClassTransferPackageClassIndex{index},
            Application::ClassTransferReviewClassAction::Create,
            std::nullopt
        });
    }
    choices.teachers.push_back({
        Application::ClassTransferTeacherKey{teacherKey.toStdString()},
        Application::ClassTransferReviewTeacherAction::Create,
        std::nullopt
    });
    return {
        .package = ClassMngr::Data::Legacy::toPortableClassTransferPackage(
            std::move(package)),
        .choices = std::move(choices)
    };
}

Application::ClassTransferApplyCommand createReplaceAndSkipCommand(
    const int replaceTargetClassId,
    const qint64 existingTeacherId
    )
{
    ClassTransferPackage package = validPackage();
    ClassTransferClass skippedClass = package.classes.first();
    skippedClass.key = QStringLiteral("class-skip");
    skippedClass.name = QStringLiteral("Skipped Class");
    package.classes.append(std::move(skippedClass));

    Application::ClassTransferApplyRequest choices;
    choices.classes = {
        {
            Application::ClassTransferPackageClassIndex{0},
            Application::ClassTransferReviewClassAction::Replace,
            classId(replaceTargetClassId)
        },
        {
            Application::ClassTransferPackageClassIndex{1},
            Application::ClassTransferReviewClassAction::Skip,
            std::nullopt
        }
    };
    choices.teachers.push_back({
        Application::ClassTransferTeacherKey{"teacher-1"},
        Application::ClassTransferReviewTeacherAction::KeepExisting,
        teacherId(static_cast<int>(existingTeacherId))
    });
    return {
        .package = ClassMngr::Data::Legacy::toPortableClassTransferPackage(
            std::move(package)),
        .choices = std::move(choices)
    };
}

bool seedVisibleClasses(
    QSqlDatabase database,
    const int classCount,
    const int specialClassIndex = -1,
    const QString& specialClassName = {},
    int* specialClassId = nullptr
    )
{
    QSqlQuery insert(database);
    if (!insert.prepare(QStringLiteral("INSERT INTO classes(name) VALUES(?)")))
    {
        return false;
    }
    for (int index = 0; index < classCount; ++index)
    {
        const QString name = index == specialClassIndex
            ? specialClassName
            : QStringLiteral("Existing Class %1").arg(index);
        insert.bindValue(0, name);
        if (!insert.exec())
        {
            return false;
        }
        if (index == specialClassIndex && specialClassId)
        {
            bool idValid = false;
            const qint64 id = insert.lastInsertId().toLongLong(&idValid);
            if (!idValid || id <= 0 || id > std::numeric_limits<int>::max())
            {
                return false;
            }
            *specialClassId = static_cast<int>(id);
        }
    }
    return true;
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

qint64 totalChanges(QSqlDatabase database)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT total_changes()"))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toLongLong();
}

bool insertLocalClass(
    ApplicationServices& services,
    const QString& name,
    const int teacher,
    int* classIdOut
    )
{
    ClassRepository* const classes = services.workspaceStore()->classRepository();
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
    QSqlQuery info(services.workspaceStore()->database());
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
    void rejectsMalformedCallerChoicesAsInvalidInput();
    void failedImportTransactionStartIsTechnicalWithoutWrites();
    void rejectsStaleChoiceWithoutWrites();
    void rejectsOnceValidChoiceAfterDestinationChanges();
    void preflightsScheduleConflictBeforeWrites();
    void classifiesMalformedPersistedScheduleAsValidationBeforeWrites();
    void normalizesOwnedPackageWithoutChangingArrayOrder();
    void createsWithNormalizedPayloadAndTypedSummary();
    void skipsClassAndPreservesPositiveCount();
    void rejects4097CreateImportWithoutPartialMutation();
    void nextImportKeepsThe4096VisibleClassLimitUsable();
    void testingClassesDoNotCountTowardVisibleLimit();
    void replaceAndSkipDoNotIncreaseTheVisibleClassCount();
    void legacyRepositoryImportRemainsUnbounded();
    void rejectsVisibleOverflowWithKoreanTranslation();
    void createNewRejectsGeneratedClassIdOutsideIntAndRollsBack();
    void createNewAcceptsIntMaxGeneratedClassId();
    void replacesClassAndTeacherThroughRepository();
    void importsSpeakingEvaluationWithWideGeneratedId();
    void failedImportCommitRollsBackAndKeepsCommitDiagnostic();
    void lateWriteFailureRollsBackTransferRows();
    void lateWriteFailureKeepsPrimaryDiagnosticWhenRollbackWasAlreadyForced();
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
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("teachers")), 0);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("classes")), 0);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
rejectsMalformedCallerChoicesAsInvalidInput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    auto command = createCommand();
    command.choices.classes.front().packageClassIndex =
        Application::ClassTransferPackageClassIndex{8};
    const ::ClassTransferPackage legacyPackage =
        ClassMngr::Data::Legacy::toLegacyClassTransferPackage(command.package);
    const auto repositoryResult = services.workspaceStore()
        ->classTransferRepository()->importClasses(
            legacyPackage, command.choices);
    QVERIFY(!repositoryResult);
    QCOMPARE(repositoryResult.error().code, RepositoryErrorCode::InvalidInput);
    QCOMPARE(repositoryResult.error().diagnostic,
        QStringLiteral("The class import plan contains an invalid or duplicate class entry."));
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(std::move(command));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("teachers")), 0);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("classes")), 0);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
failedImportTransactionStartIsTechnicalWithoutWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(database.transaction());
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createCommand());

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().message.find(
        "Unable to start the class import transaction") != std::string::npos);
    QCOMPARE(count(database, QStringLiteral("teachers")), 0);
    QCOMPARE(count(database, QStringLiteral("classes")), 0);
    QVERIFY(database.rollback());
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
rejectsStaleChoiceWithoutWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    auto command = createCommand();
    command.choices.classes.front().action =
        Application::ClassTransferReviewClassAction::Replace;
    command.choices.classes.front().targetClassId = classId(999);
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(command);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("teachers")), 0);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("classes")), 0);
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
    const auto localTeacher = services.workspaceStore()->teacherRepository()
        ->createTeacher(local);
    QVERIFY(localTeacher);
    int localClass = -1;
    QVERIFY(insertLocalClass(services, QStringLiteral("Imported Class"),
        *localTeacher, &localClass));
    const auto preview = services.workspaceStore()->classTransferRepository()
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
    QVERIFY(services.workspaceStore()->classRepository()->deleteClass(localClass));
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(command);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QVERIFY(result.error().message.find("replacement") != std::string::npos
        || result.error().message.find("class") != std::string::npos);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("teachers")), 1);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("classes")), 0);
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
    QSqlQuery schedule(services.workspaceStore()->database());
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
    const qint64 changesBefore =
        totalChanges(services.workspaceStore()->database());
    QVERIFY(changesBefore >= 0);
    auto command = createCommand(std::move(package));
    const ::ClassTransferPackage legacyPackage =
        ClassMngr::Data::Legacy::toLegacyClassTransferPackage(command.package);
    const auto detailed = services.workspaceStore()->classTransferRepository()
        ->importClassesWithFailureDetails(legacyPackage, command.choices);
    QVERIFY(!detailed);
    QCOMPARE(detailed.error().kind,
        ClassTransferImportError::Kind::ScheduleConflict);
    QCOMPARE(detailed.error().repositoryError.code,
        RepositoryErrorCode::Conflict);
    QCOMPARE(detailed.error().message,
        QStringLiteral(
            "Schedule conflicts prevent this import:\n\n"
            "Regular schedule: E4 Theseus \u2014 Monday 9:00 AM\u201310:00 AM "
            "conflicts with E4 Theseus \u2014 Monday 9:00 AM\u201310:00 AM"));
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(command);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QVERIFY(result.error().recoverable);
    QCOMPARE(
        QString::fromStdString(result.error().message),
        QStringLiteral(
            "Schedule conflicts prevent this import:\n\n"
            "Regular schedule: E4 Theseus \u2014 Monday 9:00 AM\u201310:00 AM "
            "conflicts with E4 Theseus \u2014 Monday 9:00 AM\u201310:00 AM"));
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("teachers")), 0);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("classes")), 1);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("class_info")), 1);
    QCOMPARE(totalChanges(services.workspaceStore()->database()), changesBefore);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
classifiesMalformedPersistedScheduleAsValidationBeforeWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    int localClass = -1;
    QVERIFY(insertLocalClass(services, QStringLiteral("Malformed Schedule Class"),
        -1, &localClass));
    QSqlDatabase database = services.workspaceStore()->database();
    QSqlQuery malformedSchedule(database);
    malformedSchedule.prepare(QStringLiteral(
        "INSERT INTO class_times (class_id, day, start_time, end_time) "
        "VALUES (?, 'Not a weekday', '9:00 AM', '10:00 AM')"));
    malformedSchedule.addBindValue(localClass);
    QVERIFY(malformedSchedule.exec());
    const qint64 changesBefore = totalChanges(database);
    QVERIFY(changesBefore >= 0);
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createCommand());

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QVERIFY(result.error().message.find("invalid regular schedule entry")
        != std::string::npos);
    QCOMPARE(totalChanges(database), changesBefore);
    QCOMPARE(count(database, QStringLiteral("teachers")), 0);
    QCOMPARE(count(database, QStringLiteral("classes")), 1);
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
    QCOMPARE(result.value().skippedClassCount.rawValue(), 0);
    const auto parsed = Application::classTransferApplyDestinationId(
        result.value().createdClassIds.front());
    QVERIFY(parsed.has_value());
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("teachers")), 1);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("classes")), 1);
    const auto stored = services.workspaceStore()->teacherRepository()->getTeacher(1);
    QVERIFY(stored);
    QCOMPARE(stored->teacherEn, QStringLiteral("Alex Kim"));
    QCOMPARE(stored->roomNumber, QStringLiteral("405"));
    QCOMPARE(stored->phoneNumber, QStringLiteral("010-1234-5678"));

    QVERIFY2(createCommand().package.teachers.front().teacher.teacherEn
        != stored->teacherEn.toStdU16String(),
        "fixture should require normalization");
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
skipsClassAndPreservesPositiveCount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    auto command = createCommand();
    command.choices.classes.front().action =
        Application::ClassTransferReviewClassAction::Skip;
    const auto result = port.apply(std::move(command));

    QVERIFY2(result, result ? "" : result.error().message.c_str());
    QVERIFY(result.value().createdClassIds.empty());
    QVERIFY(result.value().replacedClassIds.empty());
    QCOMPARE(result.value().skippedClassCount.rawValue(), 1);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("teachers")), 0);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("classes")), 0);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
rejects4097CreateImportWithoutPartialMutation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();
    const qint64 changesBefore = totalChanges(database);
    QVERIFY(changesBefore >= 0);
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createManyCreatesCommand(4097));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(QString::fromStdString(result.error().message),
        QStringLiteral(
            "The class import would exceed the visible Classes limit of 4096."));
    QCOMPARE(totalChanges(database), changesBefore);
    QCOMPARE(count(database, QStringLiteral("teachers")), 0);
    QCOMPARE(count(database, QStringLiteral("classes")), 0);
    QCOMPARE(count(database, QStringLiteral("class_info")), 0);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
nextImportKeepsThe4096VisibleClassLimitUsable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedVisibleClasses(database, 4095));
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createCommand());

    QVERIFY2(result, result ? "" : result.error().message.c_str());
    QCOMPARE(result.value().createdClassIds.size(), std::size_t(1));
    QCOMPARE(count(database, QStringLiteral("classes")), 4096);
    const auto visibleClasses =
        services.workspaceStore()->classRepository()
            ->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(visibleClasses);
    QCOMPARE(visibleClasses->size(), 4096);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
testingClassesDoNotCountTowardVisibleLimit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedVisibleClasses(database, 4095));

    QSqlQuery insertTestingClass(database);
    QVERIFY(insertTestingClass.exec(QStringLiteral(
        "INSERT INTO classes(name) VALUES('Testing Bound Class')")));
    bool testingClassIdValid = false;
    const int testingClassId =
        insertTestingClass.lastInsertId().toInt(&testingClassIdValid);
    QVERIFY(testingClassIdValid);
    QVERIFY(testingClassId > 0);
    QSqlQuery markTestingClass(database);
    QVERIFY(markTestingClass.prepare(QStringLiteral(
        "INSERT INTO testing_classes(class_id, room) VALUES(?, ?)")));
    markTestingClass.addBindValue(testingClassId);
    markTestingClass.addBindValue(QStringLiteral("Bound testing room"));
    QVERIFY(markTestingClass.exec());
    QCOMPARE(count(database, QStringLiteral("classes")), 4096);
    QCOMPARE(count(database, QStringLiteral("testing_classes")), 1);

    const auto visibleBeforeImport =
        services.workspaceStore()->classRepository()
            ->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(visibleBeforeImport);
    QCOMPARE(visibleBeforeImport->size(), 4095);
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createCommand());

    QVERIFY2(result, result ? "" : result.error().message.c_str());
    QCOMPARE(result.value().createdClassIds.size(), std::size_t(1));
    QCOMPARE(count(database, QStringLiteral("classes")), 4097);
    const auto visibleAfterImport =
        services.workspaceStore()->classRepository()
            ->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(visibleAfterImport);
    QCOMPARE(visibleAfterImport->size(), 4096);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
replaceAndSkipDoNotIncreaseTheVisibleClassCount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();
    Teacher localTeacher;
    localTeacher.teacherEn = QStringLiteral("Alex Kim");
    localTeacher.preferredName = QStringLiteral("Alex Kim");
    const auto existingTeacherId = services.workspaceStore()
        ->teacherRepository()->createTeacher(localTeacher);
    QVERIFY(existingTeacherId);
    QVERIFY(seedVisibleClasses(database, 4095));
    int replaceTargetClassId = -1;
    QVERIFY(insertLocalClass(services, QStringLiteral("Imported Class"),
        static_cast<int>(*existingTeacherId), &replaceTargetClassId));
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createReplaceAndSkipCommand(
        replaceTargetClassId, *existingTeacherId));

    QVERIFY2(result, result ? "" : result.error().message.c_str());
    QVERIFY(result.value().createdClassIds.empty());
    QCOMPARE(result.value().replacedClassIds.size(), std::size_t(1));
    QCOMPARE(result.value().replacedClassIds.front(),
        classId(replaceTargetClassId));
    QCOMPARE(result.value().skippedClassCount.rawValue(), 1);
    QCOMPARE(count(database, QStringLiteral("classes")), 4096);
    const auto visibleClasses =
        services.workspaceStore()->classRepository()
            ->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(visibleClasses);
    QCOMPARE(visibleClasses->size(), 4096);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
legacyRepositoryImportRemainsUnbounded()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedVisibleClasses(database, 4096));

    const auto atLimit = services.workspaceStore()->classRepository()
        ->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(atLimit);
    QCOMPARE(atLimit->size(), 4096);

    ClassImportPlan legacyPlan;
    legacyPlan.classes.append({0, ClassImportAction::Create, -1});
    legacyPlan.teachers.append({
        QStringLiteral("teacher-1"), TeacherImportAction::Create, -1
    });
    const RepositoryResult<ClassImportSummary> result =
        services.workspaceStore()->classTransferRepository()->importClasses(
            validPackage(), legacyPlan);

    QVERIFY2(result, result ? "" : result.error().diagnostic.toUtf8().constData());
    QCOMPARE(count(database, QStringLiteral("classes")), 4097);
    const auto overLimit = services.workspaceStore()->classRepository()
        ->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(!overLimit);
    QCOMPARE(overLimit.error().code, RepositoryErrorCode::Validation);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
rejectsVisibleOverflowWithKoreanTranslation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedVisibleClasses(database, 4096));
    const qint64 changesBefore = totalChanges(database);
    QVERIFY(changesBefore >= 0);

    QTranslator translator;
    const QString translationPath = QDir(QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY))
            .filePath(QStringLiteral("ClassMngr_ko_KR.qm"));
    QVERIFY2(translator.load(translationPath), qPrintable(translationPath));
    QVERIFY(QCoreApplication::installTranslator(&translator));
    Platform::ApplicationServicesClassTransferApplyPort port(services);
    const auto result = port.apply(createCommand());
    QCoreApplication::removeTranslator(&translator);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(QString::fromStdString(result.error().message),
        QStringLiteral(
            "수업을 가져오면 표시 가능한 수업 수 한도(4096개)를 초과합니다."));
    QCOMPARE(totalChanges(database), changesBefore);
    QCOMPARE(count(database, QStringLiteral("classes")), 4096);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
createNewRejectsGeneratedClassIdOutsideIntAndRollsBack()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();

    QSqlQuery existingClass(database);
    QVERIFY(existingClass.exec(QStringLiteral(
        "INSERT INTO classes (id, name) VALUES (1, 'Preexisting Sentinel Class')")));
    QCOMPARE(count(database, QStringLiteral("class_info")), 0);

    QSqlQuery setClassSequence(database);
    setClassSequence.prepare(QStringLiteral(
        "UPDATE sqlite_sequence SET seq = ? WHERE name = 'classes'"));
    setClassSequence.addBindValue(qint64(4294967296LL));
    QVERIFY(setClassSequence.exec());
    QCOMPARE(setClassSequence.numRowsAffected(), 1);

    Platform::ApplicationServicesClassTransferApplyPort port(services);
    const auto result = port.apply(createCommand());

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(count(database, QStringLiteral("teachers")), 0);
    QCOMPARE(count(database, QStringLiteral("classes")), 1);
    QCOMPARE(count(database, QStringLiteral("class_info")), 0);
    QCOMPARE(count(database, QStringLiteral("class_times")), 0);
    QCOMPARE(count(database, QStringLiteral("class_intensive_times")), 0);
    QCOMPARE(count(database, QStringLiteral("roster_columns")), 0);
    QCOMPARE(count(database, QStringLiteral("roster_data")), 0);
    QCOMPARE(count(database, QStringLiteral("speaking_evaluations")), 0);
    QCOMPARE(count(database, QStringLiteral("speaking_eval_data")), 0);

    QSqlQuery sentinelClass(database);
    sentinelClass.prepare(QStringLiteral(
        "SELECT name FROM classes WHERE id = 1"));
    QVERIFY(sentinelClass.exec());
    QVERIFY(sentinelClass.next());
    QCOMPARE(sentinelClass.value(0).toString(),
        QStringLiteral("Preexisting Sentinel Class"));
    QVERIFY(!sentinelClass.next());

    QSqlQuery aliasedClassInfo(database);
    aliasedClassInfo.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM class_info WHERE class_id = 1"));
    QVERIFY(aliasedClassInfo.exec());
    QVERIFY(aliasedClassInfo.next());
    QCOMPARE(aliasedClassInfo.value(0).toInt(), 0);

    QSqlQuery generatedClass(database);
    QVERIFY(generatedClass.exec(QStringLiteral(
        "SELECT COUNT(*) FROM classes WHERE id = 4294967297")));
    QVERIFY(generatedClass.next());
    QCOMPARE(generatedClass.value(0).toInt(), 0);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
createNewAcceptsIntMaxGeneratedClassId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();

    QSqlQuery seedClass(database);
    QVERIFY(seedClass.exec(QStringLiteral(
        "INSERT INTO classes (name) VALUES ('Class Sequence Seed')")));
    const qint64 seedClassId = seedClass.lastInsertId().toLongLong();
    QVERIFY(seedClassId > 0);

    QSqlQuery removeSeedClass(database);
    removeSeedClass.prepare(QStringLiteral(
        "DELETE FROM classes WHERE id = ?"));
    removeSeedClass.addBindValue(seedClassId);
    QVERIFY(removeSeedClass.exec());

    constexpr int maximumClassId = std::numeric_limits<int>::max();
    QSqlQuery setClassSequence(database);
    setClassSequence.prepare(QStringLiteral(
        "UPDATE sqlite_sequence SET seq = ? WHERE name = 'classes'"));
    setClassSequence.addBindValue(qint64(maximumClassId) - 1);
    QVERIFY(setClassSequence.exec());
    QCOMPARE(setClassSequence.numRowsAffected(), 1);

    Platform::ApplicationServicesClassTransferApplyPort port(services);
    const auto result = port.apply(createCommand());

    QVERIFY2(result, result ? "" : result.error().message.c_str());
    QCOMPARE(result.value().createdClassIds.size(), std::size_t(1));
    QCOMPARE(result.value().createdClassIds.front().value(),
        std::to_string(maximumClassId));

    QSqlQuery importedClass(database);
    importedClass.prepare(QStringLiteral(
        "SELECT name FROM classes WHERE id = ?"));
    importedClass.addBindValue(maximumClassId);
    QVERIFY(importedClass.exec());
    QVERIFY(importedClass.next());
    QCOMPARE(importedClass.value(0).toString(),
        QStringLiteral("Imported Class"));
    QVERIFY(!importedClass.next());

    QSqlQuery importedClassInfo(database);
    importedClassInfo.prepare(QStringLiteral(
        "SELECT class_grade, class_level, teacher_id FROM class_info "
        "WHERE class_id = ?"));
    importedClassInfo.addBindValue(maximumClassId);
    QVERIFY(importedClassInfo.exec());
    QVERIFY(importedClassInfo.next());
    QCOMPARE(importedClassInfo.value(0).toString(), QStringLiteral("E4"));
    QCOMPARE(importedClassInfo.value(1).toString(), QStringLiteral("Theseus"));
    QVERIFY(importedClassInfo.value(2).toLongLong() > 0);
    QVERIFY(!importedClassInfo.next());
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
    const auto localTeacher = services.workspaceStore()->teacherRepository()
        ->createTeacher(local);
    QVERIFY(localTeacher);
    int localClass = -1;
    QVERIFY(insertLocalClass(services, QStringLiteral("Imported Class"),
        *localTeacher, &localClass));
    const auto preview = services.workspaceStore()->classTransferRepository()
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
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("teachers")), 1);
    QCOMPARE(count(services.workspaceStore()->database(), QStringLiteral("classes")), 1);
    const auto replaced = services.workspaceStore()->teacherRepository()
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
    QSqlDatabase database = services.workspaceStore()->database();

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

    const auto readback = services.workspaceStore()->classTransferRepository()
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
failedImportCommitRollsBackAndKeepsCommitDiagnostic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();
    QSqlQuery deferConstraints(database);
    QVERIFY(deferConstraints.exec(QStringLiteral("PRAGMA defer_foreign_keys = ON")));
    QSqlQuery trigger(database);
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER fail_transfer_commit AFTER INSERT ON classes "
        "BEGIN INSERT INTO roster_data(class_id, row_index, col_index, value) "
        "VALUES (999999, 0, 0, 'deferred foreign key'); END")));
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createCommand());

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().message.find("commit the class import transaction")
        != std::string::npos);
    QVERIFY(result.error().message.find("FOREIGN KEY")
        != std::string::npos);
    QCOMPARE(count(database, QStringLiteral("teachers")), 0);
    QCOMPARE(count(database, QStringLiteral("classes")), 0);
    QCOMPARE(count(database, QStringLiteral("roster_data")), 0);
}

void NextPlatformApplicationServicesClassTransferApplyPortTests::
lateWriteFailureRollsBackTransferRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();
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

void NextPlatformApplicationServicesClassTransferApplyPortTests::
lateWriteFailureKeepsPrimaryDiagnosticWhenRollbackWasAlreadyForced()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlDatabase database = services.workspaceStore()->database();
    QSqlQuery trigger(database);
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER fail_transfer_with_rollback BEFORE INSERT ON classes "
        "BEGIN SELECT RAISE(ROLLBACK, 'F367 forced rollback'); END")));
    Platform::ApplicationServicesClassTransferApplyPort port(services);

    const auto result = port.apply(createCommand());

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().message.find("F367 forced rollback")
        != std::string::npos);
    QVERIFY(result.error().recoverable);
    QCOMPARE(count(database, QStringLiteral("teachers")), 0);
    QCOMPARE(count(database, QStringLiteral("classes")), 0);
}

QTEST_MAIN(NextPlatformApplicationServicesClassTransferApplyPortTests)

#include "next_platform_application_services_class_transfer_apply_port_tests.moc"
