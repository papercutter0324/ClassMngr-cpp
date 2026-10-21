#include "data/data_service.h"
#include "data/database/database_file_operations.h"
#include "data/database/database_schema_manager.h"
#include "data/database/workspace_store.h"
#include "data/repositories/roster_repository.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "teacher_choices_cursor_error_fixture.h"

#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include <limits>

namespace
{
struct DatabaseIds
{
    int teacherId = -1;
    int classId = -1;
    int eventId = -1;
    int campusId = -1;
    bool settingSaved = false;
    bool intensiveSlotStateSaved = false;
    bool testingBlockSaved = false;
    bool rosterSaved = false;
    bool speakingEvaluationSaved = false;
};

DatabaseIds populateDatabase(
    DataService& service,
    const QString& prefix
    )
{
    Teacher teacher;
    teacher.teacherEn = prefix + QStringLiteral(" Teacher");
    teacher.teacherKr = prefix + QStringLiteral(" Korean Teacher");
    teacher.preferredRomanization =
        prefix + QStringLiteral(" Romanization");
    teacher.preferredName =
        prefix + QStringLiteral(" Preferred Name");
    teacher.roomNumber = prefix + QStringLiteral(" Room");
    teacher.birthday = QStringLiteral("02-29");
    teacher.phoneNumber = prefix + QStringLiteral(" Phone");
    teacher.notes = prefix + QStringLiteral(" Teacher Notes");

    DatabaseIds ids;
    ids.settingSaved = service.saveSetting(
        QStringLiteral("lifecycle/value"),
        prefix
        ).has_value();
    ids.teacherId = service.createTeacher(teacher).value_or(-1);
    ids.classId = service.createClass(
        prefix + QStringLiteral(" Class")
        ).value_or(-1);

    ClassInfo classInfo;
    classInfo.classId = ids.classId;
    classInfo.teacherId = ids.teacherId;
    classInfo.classGrade = prefix + QStringLiteral(" Grade");
    classInfo.classLevel = prefix + QStringLiteral(" Level");
    classInfo.notes = prefix + QStringLiteral(" Class Notes");
    classInfo.classTimes.append(
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
        );
    const bool classInfoSaved =
        service.saveClassInfo(classInfo).has_value();
    Q_UNUSED(classInfoSaved);

    ids.intensiveSlotStateSaved = service.saveIntensiveSlotState(
        QStringLiteral("Tuesday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("lunch"),
        QStringLiteral("essay")
        ).has_value();
    ids.testingBlockSaved =
        service.saveTestingBlock(
            QStringLiteral("Wednesday"),
            QStringLiteral("16:00"),
            prefix + QStringLiteral(" Testing Room")
            ).has_value();

    CalendarEvent event;
    event.title = prefix + QStringLiteral(" Event");
    event.startDate = QDate(2026, 7, 17);
    event.endDate = event.startDate;
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    ids.eventId = service.saveCalendarEvent(event).value_or(-1);

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows.append(
        {
            prefix + QStringLiteral(" Student"),
            prefix + QStringLiteral(" 학생")
        }
        );
    ids.rosterSaved = service.saveRoster(
        ids.classId,
        roster
        ).has_value();

    SpeakingEvalRows evaluation =
        SpeakingEval::emptyRows();
    evaluation[0][SpeakingEval::toInt(
        SpeakingEvalColumn::EnglishName
        )] = prefix + QStringLiteral(" Student");
    ids.speakingEvaluationSaved = service.saveSpeakingEval(
            ids.classId,
            QStringLiteral("First Semester"),
            evaluation
        ).has_value();

    CampusRecord campus;
    campus.name = prefix + QStringLiteral(" Campus");
    campus.officeNumber = prefix + QStringLiteral(" Office");
    ids.campusId = service.saveCampus(campus).value_or(-1);

    return ids;
}

void verifyDatabase(
    DataService& service,
    const QString& prefix,
    const DatabaseIds& ids
    )
{
    QVERIFY(ids.testingBlockSaved);
    QVERIFY(ids.settingSaved);
    QVERIFY(ids.intensiveSlotStateSaved);
    QVERIFY(ids.rosterSaved);
    QVERIFY(ids.speakingEvaluationSaved);
    const Result<QVariant> setting =
        service.loadSetting(QStringLiteral("lifecycle/value"));
    QVERIFY(setting);
    QCOMPARE(setting->toString(), prefix);
    const Result<Teacher> teacher = service.getTeacher(ids.teacherId);
    QVERIFY(teacher);
    QCOMPARE(teacher->teacherEn, prefix + QStringLiteral(" Teacher"));
    QCOMPARE(
        teacher->preferredRomanization,
        prefix + QStringLiteral(" Romanization")
        );
    QCOMPARE(
        teacher->preferredName,
        prefix + QStringLiteral(" Preferred Name")
        );
    QCOMPARE(teacher->roomNumber, prefix + QStringLiteral(" Room"));
    QCOMPARE(teacher->birthday, QStringLiteral("02-29"));
    QCOMPARE(teacher->phoneNumber, prefix + QStringLiteral(" Phone"));
    const Result<Classroom> classroom = service.getClassById(ids.classId);
    QVERIFY(classroom);
    QCOMPARE(
        classroom->name,
        prefix + QStringLiteral(" Class")
        );
    QCOMPARE(
        service.loadClassInfo(ids.classId)->notes,
        prefix + QStringLiteral(" Class Notes")
        );
    QCOMPARE(
        service.loadClassInfo(ids.classId)->teacherPreferredName,
        prefix + QStringLiteral(" Preferred Name")
        );
    const Result<QList<IntensiveSlotState>> intensiveSlotStates =
        service.loadIntensiveSlotStates();
    QVERIFY(intensiveSlotStates);
    QCOMPARE(intensiveSlotStates->size(), 1);
    const Result<QList<TestingBlock>> testingBlocks =
        service.loadTestingBlocks();
    QVERIFY(testingBlocks);
    QCOMPARE(testingBlocks->size(), 1);
    QCOMPARE(
        testingBlocks->first().room,
        prefix + QStringLiteral(" Testing Room")
        );
    QCOMPARE(
        service.getCalendarEvent(ids.eventId)->title,
        prefix + QStringLiteral(" Event")
        );
    const Result<Roster> roster =
        service.loadRoster(ids.classId);
    QVERIFY(roster);
    QVERIFY(!roster->rows.isEmpty());
    QVERIFY(!roster->rows.first().isEmpty());
    QCOMPARE(
        roster->rows.first().first(),
        prefix + QStringLiteral(" Student")
        );
    const Result<SpeakingEvalRows> evaluation =
        service.loadSpeakingEval(
            ids.classId,
            QStringLiteral("First Semester")
            );
    QVERIFY(evaluation);
    QVERIFY(!evaluation->isEmpty());
    QVERIFY(
        evaluation->first().size()
        > SpeakingEval::toInt(
            SpeakingEvalColumn::EnglishName
            )
        );
    QCOMPARE(
        (*evaluation)[0][SpeakingEval::toInt(
                SpeakingEvalColumn::EnglishName
                )],
        prefix + QStringLiteral(" Student")
        );
    const Result<CampusRecord> campus = service.getCampus(ids.campusId);
    QVERIFY(campus);
    QCOMPARE(campus->name, prefix + QStringLiteral(" Campus"));
}
}

class DataServiceLifecycleTests : public QObject
{
    Q_OBJECT

private slots:
    void databaseSessionOwnsRepositoryLifetime();
    void borrowedDataServicePreservesOwnedSession();
    void failedReplacementOpenPreservesSessionAndReleasesCandidate();
    void databaseFileCopyTreatsSameFileAliasesAsNoOp();
    void databaseFileCopyPreservesDestinationWhenSourceMissing();
    void databaseFileCopyReplacesDestinationAndCreatesParent();
    void databaseFileCopyPreservesDestinationWhenReplacementFails();
    void legacyDataServiceOwnsDatabaseFileOperations();
    void applicationServicesOwnDatabaseFileOperations();
    void applicationServicesShareCanonicalSessionAcrossLifecycle();
    void borrowedFacadeResolvesRepositoriesAfterDirectSessionTransitions();
    void boundedRosterOutputLoadsOnlyRequestedColumnsAndEnforcesLimits();
    void featureServicesExposeNarrowOperations();
    void settingsServiceDoesNotFallBackFromClosedSession();
    void teacherServiceDoesNotFallBackFromClosedSession();
    void calendarServiceDoesNotFallBackFromClosedSession();
    void classNotesServiceDoesNotFallBackFromClosedSession();
    void subPrepOutputReadsDoNotFallBackFromClosedSession();
    void closeAndSwitchReleaseEveryRepository();
    void unsupportedSchemaClosesDatabaseSession();
    void classDeleteFailureRollsBackAllChanges();
    void teacherDeleteFailureRollsBackClassAssignments();
    void campusCreationValidatesGeneratedIdAtomically();
    void campusListValidatesPersistedIds();
    void campusListRejectsLateFetchFailure();
    void teacherListValidatesPersistedIds();
    void repositoryWriteFailuresAreObservable();
    void coreLookupReadFailuresAreObservable();
    void teacherDirectoryRejectsLateFetchFailure();
    void compoundAndCollectionReadFailuresAreObservable();
    void speakingEvaluationRejectsLateScoreRowFetchFailure();
    void classInfoRejectsLateRegularScheduleFetchFailure();
    void classInfoRejectsLateIntensiveScheduleFetchFailure();
    void speakingEvaluationRepositoryPreservesWideIds();
    void speakingEvaluationSaveRejectsMalformedStoredRowIndex();
    void legacyRepositoryWriteFailuresRollBack();
    void existingTeacherSchemaIsRejectedWithoutPersonalDetailMigration();
    void existingTestingSchemaIsRejectedWithoutAssignmentMigration();
};

void DataServiceLifecycleTests::borrowedDataServicePreservesOwnedSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DatabaseSession session;
    const QString path = directory.filePath(QStringLiteral("borrowed.db"));
    QVERIFY(session.open(path));
    const QString connection = session.database().connectionName();
    {
        DataService facade(session);
        QCOMPARE(facade.databaseSession(), &session);
        QVERIFY(facade.isOpen());
        QCOMPARE(facade.currentDatabasePath(), session.databasePath());
        QVERIFY(facade.saveSetting(QStringLiteral("borrowed/value"), 42));
        const auto loaded = facade.loadSetting(QStringLiteral("borrowed/value"));
        QVERIFY(loaded);
        QCOMPARE(loaded->toInt(), 42);
    }
    QVERIFY(session.isOpen());
    QCOMPARE(session.database().connectionName(), connection);
    QVERIFY(QSqlDatabase::contains(connection));
    {
        DataService secondFacade(session);
        const auto retained = secondFacade.loadSetting(QStringLiteral("borrowed/value"));
        QVERIFY(retained);
        QCOMPARE(retained->toInt(), 42);
        secondFacade.closeDatabase();
        QVERIFY(!session.isOpen());
        QVERIFY(!secondFacade.isOpen());
        QVERIFY(!secondFacade.loadSetting(QStringLiteral("borrowed/value")));
        QVERIFY(secondFacade.openDatabase(path));
        QCOMPARE(secondFacade.databaseSession(), &session);
        const auto reopened = secondFacade.loadSetting(QStringLiteral("borrowed/value"));
        QVERIFY(reopened);
        QCOMPARE(reopened->toInt(), 42);
    }
    QVERIFY(session.isOpen());
    session.close();
}
void DataServiceLifecycleTests::applicationServicesShareCanonicalSessionAcrossLifecycle()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    WorkspaceStore* const store = services.workspaceStore();
    DatabaseSession* const session = services.databaseSession();
    DataService* const facade = services.dataService();
    QVERIFY(store != nullptr);
    QVERIFY(session != nullptr);
    QVERIFY(facade != nullptr);
    QCOMPARE(store, session);
    QCOMPARE(facade->databaseSession(), session);
    const QList<FeatureService*> features = {
        services.settingsService(), services.teacherService(), services.classService(),
        services.scheduleService(), services.calendarService(), services.rosterService(),
        services.speakingEvaluationService()
    };
    for (FeatureService* feature : features)
        QVERIFY(!feature->isAvailable());
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(!store->isOpen());
    QVERIFY(!session->isOpen());
    QVERIFY(!facade->isOpen());
    const QString path = directory.filePath(QStringLiteral("canonical.db"));
    const QStringList connectionsBeforeOpen = QSqlDatabase::connectionNames();
    QVERIFY(services.openDatabase(path));
    QCOMPARE(services.workspaceStore(), store);
    QCOMPARE(services.databaseSession(), session);
    QCOMPARE(store, session);
    QVERIFY(session->isOpen());
    QVERIFY(store->isOpen());
    QVERIFY(facade->isOpen());
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), session->databasePath());
    QCOMPARE(facade->currentDatabasePath(), session->databasePath());
    QCOMPARE(store->databasePath(), session->databasePath());
    QStringList addedConnections;
    for (const QString& name : QSqlDatabase::connectionNames())
    {
        if (!connectionsBeforeOpen.contains(name))
        {
            addedConnections.append(name);
        }
    }
    QCOMPARE(addedConnections.size(), 1);
    QCOMPARE(addedConnections.first(), store->database().connectionName());
    QCOMPARE(store->settingsRepository(), session->settingsRepository());
    QSqlDatabase storeDatabase = store->database();
    const Result<int> schemaVersion =
        DatabaseSchemaManager::schemaVersion(storeDatabase);
    QVERIFY(schemaVersion);
    QCOMPARE(*schemaVersion, DatabaseSchemaManager::LatestSchemaVersion);
    for (FeatureService* feature : features)
        QVERIFY(feature->isAvailable());
    QVERIFY(services.settingsService()->save(QStringLiteral("canonical/value"), 73));
    const auto facadeValue = facade->loadSetting(QStringLiteral("canonical/value"));
    QVERIFY(facadeValue);
    QCOMPARE(facadeValue->toInt(), 73);
    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Canonical teacher");
    const auto teacherId = facade->createTeacher(teacher);
    QVERIFY(teacherId);
    const auto featureTeacher = services.teacherService()->teacher(*teacherId);
    QVERIFY(featureTeacher);
    QCOMPARE(featureTeacher->teacherEn, teacher.teacherEn);
    const QString connection = session->database().connectionName();
    const QStringList connections = QSqlDatabase::connectionNames();
    const QString missingPath = directory.filePath(QStringLiteral("missing.db"));
    QVERIFY(!services.openExistingDatabase(missingPath));
    QVERIFY(!QFileInfo::exists(missingPath));
    QCOMPARE(services.workspaceStore(), store);
    QCOMPARE(services.databaseSession(), session);
    QCOMPARE(store->database().connectionName(), connection);
    QCOMPARE(QSqlDatabase::connectionNames(), connections);
    QCOMPARE(services.currentDatabasePath(), QFileInfo(path).absoluteFilePath());
    const QString invalidPath = directory.filePath(QStringLiteral("invalid.db"));
    QFile invalidFile(invalidPath);
    QVERIFY(invalidFile.open(QIODevice::WriteOnly));
    const QByteArray invalidBytes = QByteArrayLiteral("not a SQLite database");
    QCOMPARE(invalidFile.write(invalidBytes), qint64(invalidBytes.size()));
    invalidFile.close();
    QVERIFY(!services.openDatabase(invalidPath));
    QCOMPARE(services.workspaceStore(), store);
    QCOMPARE(services.databaseSession(), session);
    QCOMPARE(store, session);
    QCOMPARE(facade->databaseSession(), session);
    QCOMPARE(session->database().connectionName(), connection);
    QCOMPARE(QSqlDatabase::connectionNames(), connections);
    QCOMPARE(services.currentDatabasePath(), QFileInfo(path).absoluteFilePath());
    for (FeatureService* feature : features)
        QVERIFY(feature->isAvailable());
    const auto retained = services.settingsService()->load(QStringLiteral("canonical/value"));
    QVERIFY(retained);
    QCOMPARE(retained->toInt(), 73);
    services.closeDatabase();
    QCOMPARE(services.workspaceStore(), store);
    QCOMPARE(services.databaseSession(), session);
    QCOMPARE(store, session);
    QVERIFY(!store->isOpen());
    QVERIFY(!session->isOpen());
    QVERIFY(!facade->isOpen());
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.currentDatabasePath().isEmpty());
    QCOMPARE(facade->currentDatabasePath(), session->databasePath());
    QVERIFY(!QSqlDatabase::contains(connection));
    for (FeatureService* feature : features)
        QVERIFY(!feature->isAvailable());
    QVERIFY(!facade->loadSetting(QStringLiteral("canonical/value")));
    QVERIFY(!services.settingsService()->load(QStringLiteral("canonical/value")));
    QVERIFY(services.openExistingDatabase(path));
    QCOMPARE(services.workspaceStore(), store);
    QCOMPARE(services.databaseSession(), session);
    QCOMPARE(store, session);
    QCOMPARE(facade->databaseSession(), session);
    for (FeatureService* feature : features)
        QVERIFY(feature->isAvailable());
    const auto reopened = services.settingsService()->load(QStringLiteral("canonical/value"));
    QVERIFY(reopened);
    QCOMPARE(reopened->toInt(), 73);
}
void DataServiceLifecycleTests::applicationServicesOwnDatabaseFileOperations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString sourcePath =
        directory.filePath(QStringLiteral("source.db"));
    const QString savedPath =
        directory.filePath(QStringLiteral("saved.db"));
    const QString exportedPath =
        directory.filePath(QStringLiteral("exported.db"));

    ApplicationServices services;
    QVERIFY(services.openDatabase(sourcePath).has_value());
    QVERIFY(services.settingsService()->save(
        QStringLiteral("application-services/value"),
        QStringLiteral("saved")
        ).has_value());
    services.saveDatabase();

    QVERIFY(services.saveDatabaseAs(savedPath).has_value());
    QVERIFY(services.exportDatabaseAs(exportedPath).has_value());
    QVERIFY(QFileInfo::exists(savedPath));
    QVERIFY(QFileInfo::exists(exportedPath));

    ApplicationServices copiedServices;
    QVERIFY(copiedServices.openDatabase(savedPath).has_value());
    QCOMPARE(
        copiedServices.settingsService()
            ->load(QStringLiteral("application-services/value"))
            ->toString(),
        QStringLiteral("saved")
        );

    services.closeDatabase();
    QVERIFY(!services.saveDatabaseAs(savedPath).has_value());
    QVERIFY(!services.exportDatabaseAs(exportedPath).has_value());
}

void DataServiceLifecycleTests::databaseFileCopyTreatsSameFileAliasesAsNoOp()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString sourcePath =
        directory.filePath(QStringLiteral("Profile.tps"));
    const QString caseAliasPath =
        directory.filePath(QStringLiteral("profile.tps"));
    const QString settingKey = QStringLiteral("same-file-alias/value");

    DataService source;
    QVERIFY(source.openDatabase(sourcePath));
    QVERIFY(source.saveSetting(settingKey, QStringLiteral("preserved data")));
    source.closeDatabase();

#if defined(Q_OS_WIN)
    QVERIFY(QFileInfo::exists(caseAliasPath));
#endif

    const Status directCopy =
        DatabaseFileOperations::copyDatabaseFile(sourcePath, caseAliasPath);
    QVERIFY2(directCopy.has_value(),
             directCopy ? "" : qPrintable(directCopy.error()));
    QVERIFY(QFileInfo::exists(sourcePath));

    DataService verification;
    QVERIFY(verification.openDatabase(sourcePath));
    const Result<QVariant> beforeExport = verification.loadSetting(settingKey);
    QVERIFY(beforeExport);
    QCOMPARE(beforeExport->toString(), QStringLiteral("preserved data"));

    QVERIFY(verification.exportAs(caseAliasPath));
    QVERIFY(QFileInfo::exists(sourcePath));
    const Result<QVariant> afterExport = verification.loadSetting(settingKey);
    QVERIFY(afterExport);
    QCOMPARE(afterExport->toString(), QStringLiteral("preserved data"));
    verification.closeDatabase();

    DataService copied;
    QVERIFY(copied.openDatabase(caseAliasPath));
    const Result<QVariant> copiedValue = copied.loadSetting(settingKey);
    QVERIFY(copiedValue);
    QCOMPARE(copiedValue->toString(), QStringLiteral("preserved data"));
}

void DataServiceLifecycleTests::databaseFileCopyPreservesDestinationWhenSourceMissing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString sourcePath =
        directory.filePath(QStringLiteral("missing-source.db"));
    const QString destinationPath =
        directory.filePath(QStringLiteral("existing-destination.db"));
    const QByteArray sentinelBytes("existing destination sentinel");

    QVERIFY(sourcePath != destinationPath);
    QVERIFY(!QFileInfo::exists(sourcePath));

    QFile destination(destinationPath);
    QVERIFY(destination.open(QIODevice::WriteOnly));
    QCOMPARE(destination.write(sentinelBytes), qint64(sentinelBytes.size()));
    destination.close();

    const Status copyResult =
        DatabaseFileOperations::copyDatabaseFile(sourcePath, destinationPath);
    QVERIFY(!copyResult.has_value());
    QCOMPARE(
        copyResult.error(),
        QStringLiteral("Unable to copy Teacher Profile to:\n%1")
            .arg(QFileInfo(destinationPath).absoluteFilePath())
        );

    const bool destinationExists = QFileInfo::exists(destinationPath);
    QVERIFY(destinationExists);
    QVERIFY(destination.open(QIODevice::ReadOnly));
    QCOMPARE(destination.readAll(), sentinelBytes);
    destination.close();
}

void DataServiceLifecycleTests::databaseFileCopyReplacesDestinationAndCreatesParent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString sourcePath =
        directory.filePath(QStringLiteral("source-프로필.db"));
    const QString destinationPath = directory.filePath(
        QStringLiteral("nested/대상.db")
        );
    const QByteArray sourceBytes("replacement source contents");
    const QByteArray previousBytes("previous destination contents");
    const QFileDevice::Permissions permissionMask =
        QFileDevice::ReadOwner
        | QFileDevice::WriteOwner
        | QFileDevice::ReadUser
        | QFileDevice::WriteUser
        | QFileDevice::ReadGroup
        | QFileDevice::WriteGroup
        | QFileDevice::ReadOther
        | QFileDevice::WriteOther;

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write(sourceBytes), qint64(sourceBytes.size()));
    source.close();
    QVERIFY(source.setPermissions(permissionMask));
    const QFileDevice::Permissions sourcePermissions = source.permissions();
    QVERIFY(!QFileInfo::exists(QFileInfo(destinationPath).absolutePath()));

    const Status firstCopy =
        DatabaseFileOperations::copyDatabaseFile(sourcePath, destinationPath);
    QVERIFY2(firstCopy.has_value(),
             firstCopy ? "" : qPrintable(firstCopy.error()));

    QFile destination(destinationPath);
    QVERIFY(destination.open(QIODevice::ReadOnly));
    QCOMPARE(destination.readAll(), sourceBytes);
    QCOMPARE(destination.permissions(), sourcePermissions);
    destination.close();

    QVERIFY(source.open(QIODevice::ReadOnly));
    QCOMPARE(source.readAll(), sourceBytes);
    QCOMPARE(source.permissions(), sourcePermissions);
    source.close();

    QVERIFY(destination.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(destination.write(previousBytes), qint64(previousBytes.size()));
    destination.close();

    const Status replacingCopy =
        DatabaseFileOperations::copyDatabaseFile(sourcePath, destinationPath);
    QVERIFY2(replacingCopy.has_value(),
             replacingCopy ? "" : qPrintable(replacingCopy.error()));
    QVERIFY(destination.open(QIODevice::ReadOnly));
    QCOMPARE(destination.readAll(), sourceBytes);
    QCOMPARE(destination.permissions(), sourcePermissions);
    destination.close();

    QVERIFY(source.open(QIODevice::ReadOnly));
    QCOMPARE(source.readAll(), sourceBytes);
    QCOMPARE(source.permissions(), sourcePermissions);
    source.close();
}

void DataServiceLifecycleTests::databaseFileCopyPreservesDestinationWhenReplacementFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString sourcePath =
        directory.filePath(QStringLiteral("source.db"));
    const QString destinationPath =
        directory.filePath(QStringLiteral("existing-destination"));
    const QString sentinelPath =
        QDir(destinationPath).filePath(QStringLiteral("sentinel.txt"));
    const QByteArray sourceBytes("replacement source contents");
    const QByteArray sentinelBytes("replacement failure sentinel");

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write(sourceBytes), qint64(sourceBytes.size()));
    source.close();

    QVERIFY(QDir().mkpath(destinationPath));
    QFile sentinel(sentinelPath);
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    QCOMPARE(sentinel.write(sentinelBytes), qint64(sentinelBytes.size()));
    sentinel.close();

    const Status copyResult =
        DatabaseFileOperations::copyDatabaseFile(sourcePath, destinationPath);
    QVERIFY(!copyResult.has_value());
    QCOMPARE(
        copyResult.error(),
        QStringLiteral("Unable to replace existing Teacher Profile file:\n%1")
            .arg(QFileInfo(destinationPath).absoluteFilePath())
        );

    QVERIFY(QFileInfo(destinationPath).isDir());
    QVERIFY(sentinel.open(QIODevice::ReadOnly));
    QCOMPARE(sentinel.readAll(), sentinelBytes);
    sentinel.close();
}

void DataServiceLifecycleTests::legacyDataServiceOwnsDatabaseFileOperations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString sourcePath =
        directory.filePath(QStringLiteral("legacy-source.db"));
    const QString savedPath =
        directory.filePath(QStringLiteral("legacy-saved.db"));
    const QString exportedPath =
        directory.filePath(QStringLiteral("legacy-exported.db"));
    const QString key = QStringLiteral("legacy-file-operation/value");

    DataService legacyService;
    QVERIFY(legacyService.openDatabase(sourcePath));
    QVERIFY(legacyService.saveSetting(key, QStringLiteral("legacy value")));
    legacyService.save();
    QVERIFY(legacyService.saveAs(savedPath));
    QVERIFY(legacyService.exportAs(exportedPath));
    QCOMPARE(
        legacyService.currentDatabasePath(),
        QFileInfo(sourcePath).absoluteFilePath()
        );

    DataService savedDatabase;
    QVERIFY(savedDatabase.openDatabase(savedPath));
    const Result<QVariant> savedValue = savedDatabase.loadSetting(key);
    QVERIFY(savedValue);
    QCOMPARE(savedValue->toString(), QStringLiteral("legacy value"));

    DataService exportedDatabase;
    QVERIFY(exportedDatabase.openDatabase(exportedPath));
    const Result<QVariant> exportedValue = exportedDatabase.loadSetting(key);
    QVERIFY(exportedValue);
    QCOMPARE(exportedValue->toString(), QStringLiteral("legacy value"));
}

void DataServiceLifecycleTests::
borrowedFacadeResolvesRepositoriesAfterDirectSessionTransitions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    DatabaseSession* const session = services.databaseSession();
    DataService* const facade = services.dataService();
    QVERIFY(session != nullptr);
    QVERIFY(facade != nullptr);
    QVERIFY(!session->isOpen());
    QVERIFY(!facade->loadSetting(QStringLiteral("lifecycle/value")));

    const QString databaseA =
        directory.filePath(QStringLiteral("database-a.db"));
    const QString invalidDatabase =
        directory.filePath(QStringLiteral("invalid.db"));
    const QString databaseB =
        directory.filePath(QStringLiteral("database-b.db"));

    QVERIFY(services.openDatabase(databaseA));
    QVERIFY(facade->saveSetting(
        QStringLiteral("lifecycle/value"),
        QStringLiteral("Database A")
        ));
    Teacher teacherA;
    teacherA.teacherEn = QStringLiteral("Database A Teacher");
    const Result<int> teacherAId = facade->createTeacher(teacherA);
    QVERIFY(teacherAId);

    QFile invalidFile(invalidDatabase);
    QVERIFY(invalidFile.open(QIODevice::WriteOnly));
    const QByteArray invalidContents = QByteArrayLiteral("not a SQLite database");
    QCOMPARE(invalidFile.write(invalidContents), qint64(invalidContents.size()));
    invalidFile.close();
    QVERIFY(!services.openDatabase(invalidDatabase));
    QCOMPARE(services.currentDatabasePath(), QFileInfo(databaseA).absoluteFilePath());
    const Result<QVariant> retainedAValue =
        facade->loadSetting(QStringLiteral("lifecycle/value"));
    QVERIFY(retainedAValue);
    QCOMPARE(retainedAValue->toString(), QStringLiteral("Database A"));
    const Result<Teacher> retainedATeacher = facade->getTeacher(*teacherAId);
    QVERIFY(retainedATeacher);
    QCOMPARE(retainedATeacher->teacherEn, QStringLiteral("Database A Teacher"));

    QVERIFY(session->open(databaseB));
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(databaseB).absoluteFilePath()
        );
    const Result<QVariant> initialBValue =
        facade->loadSetting(QStringLiteral("lifecycle/value"));
    QVERIFY(initialBValue);
    QVERIFY(initialBValue->toString().isEmpty());
    const Result<QList<Teacher>> initialBTeachers = facade->getAllTeachers();
    QVERIFY(initialBTeachers);
    QVERIFY(initialBTeachers->isEmpty());

    QVERIFY(facade->saveSetting(
        QStringLiteral("lifecycle/value"),
        QStringLiteral("Database B")
        ));
    Teacher teacherB;
    teacherB.teacherEn = QStringLiteral("Database B Teacher");
    const Result<int> teacherBId = facade->createTeacher(teacherB);
    QVERIFY(teacherBId);
    QCOMPARE(*teacherBId, *teacherAId);
    const Result<Teacher> currentBTeacher = facade->getTeacher(*teacherBId);
    QVERIFY(currentBTeacher);
    QCOMPARE(currentBTeacher->teacherEn, QStringLiteral("Database B Teacher"));

    services.closeDatabase();
    QVERIFY(!session->isOpen());
    QVERIFY(!facade->loadSetting(QStringLiteral("lifecycle/value")));
    QVERIFY(!facade->getAllTeachers());
    QVERIFY(session->open(databaseB));
    const Result<QVariant> reopenedBValue =
        facade->loadSetting(QStringLiteral("lifecycle/value"));
    QVERIFY(reopenedBValue);
    QCOMPARE(reopenedBValue->toString(), QStringLiteral("Database B"));
    const Result<Teacher> reopenedBTeacher = facade->getTeacher(*teacherBId);
    QVERIFY(reopenedBTeacher);
    QCOMPARE(reopenedBTeacher->teacherEn, QStringLiteral("Database B Teacher"));
    const Result<Teacher> noLeakedATeacher = facade->getTeacher(*teacherAId);
    QVERIFY(noLeakedATeacher);
    QCOMPARE(noLeakedATeacher->teacherEn, QStringLiteral("Database B Teacher"));
}

void DataServiceLifecycleTests::
boundedRosterOutputLoadsOnlyRequestedColumnsAndEnforcesLimits()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("bounded-roster-output.db"))
        ).has_value());
    const Result<int> createdClass =
        service.createClass(QStringLiteral("Roster output class"));
    QVERIFY(createdClass);

    Roster fullRoster;
    fullRoster.columns = {
        QStringLiteral("Korean"),
        QStringLiteral("Notes"),
        QStringLiteral("English"),
        QStringLiteral("Unused")
    };
    fullRoster.rows = {
        {
            QStringLiteral("학생 하나"),
            QStringLiteral("First note"),
            QStringLiteral("Alex"),
            QStringLiteral("unneeded value")
        },
        {
            QStringLiteral("학생 둘"),
            QStringLiteral("Second note"),
            QStringLiteral("Casey"),
            QStringLiteral("another unneeded value")
        }
    };
    QVERIFY(service.saveRoster(*createdClass, fullRoster));

    const QStringList requestedColumns{
        QStringLiteral("Notes"),
        QStringLiteral("English")
    };
    const Result<Roster> projected = service.loadRosterForOutput(
        *createdClass,
        requestedColumns,
        2,
        4,
        256
        );
    QVERIFY(projected);
    QCOMPARE(projected->columns, requestedColumns);
    QCOMPARE(projected->columnWidths, QVector<int>({0, 0}));
    QCOMPARE(projected->rows.size(), 2);
    QCOMPARE(projected->rows.at(0), QStringList({"First note", "Alex"}));
    QCOMPARE(projected->rows.at(1), QStringList({"Second note", "Casey"}));

    const Result<Roster> rowOverflow = service.loadRosterForOutput(
        *createdClass,
        requestedColumns,
        1,
        4,
        256
        );
    QVERIFY(!rowOverflow);

    const Result<Roster> cellOverflow = service.loadRosterForOutput(
        *createdClass,
        requestedColumns,
        2,
        3,
        256
        );
    QVERIFY(!cellOverflow);

    const Result<Roster> textOverflow = service.loadRosterForOutput(
        *createdClass,
        requestedColumns,
        2,
        4,
        16
        );
    QVERIFY(!textOverflow);

    QSqlQuery injectedValue(service.databaseSession()->database());
    injectedValue.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, 2, 2, ?)"
        ));
    injectedValue.addBindValue(*createdClass);
    injectedValue.addBindValue(
        QString(kRosterRepositoryOutputMaxCellBytes + 1, QLatin1Char('x'))
        );
    QVERIFY(injectedValue.exec());
    const Result<Roster> oversizedCell = service.loadRosterForOutput(
        *createdClass,
        requestedColumns,
        kRosterRepositoryOutputMaxRows,
        kRosterRepositoryOutputMaxCells,
        kRosterRepositoryOutputMaxTextBytes
        );
    QVERIFY(!oversizedCell);

    injectedValue.prepare(QStringLiteral(
        "DELETE FROM roster_data WHERE class_id=? AND row_index=2 AND col_index=2"
        ));
    injectedValue.addBindValue(*createdClass);
    QVERIFY(injectedValue.exec());
    injectedValue.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, 1000000000, 2, 'out of range')"
        ));
    injectedValue.addBindValue(*createdClass);
    QVERIFY(injectedValue.exec());
    const Result<Roster> sparseRowOverflow = service.loadRosterForOutput(
        *createdClass,
        requestedColumns,
        kRosterRepositoryOutputMaxRows,
        kRosterRepositoryOutputMaxCells,
        kRosterRepositoryOutputMaxTextBytes
        );
    QVERIFY(!sparseRowOverflow);
}

void DataServiceLifecycleTests::unsupportedSchemaClosesDatabaseSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path =
        directory.filePath(QStringLiteral("invalid-schema.db"));
    const QString connectionName =
        QStringLiteral("invalid-schema-seed");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(path);
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE VIEW campuses AS SELECT 1 AS id"
            )));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    DatabaseSession session;
    const Status status = session.open(path);
    QVERIFY(!status);
    QVERIFY(status.error().contains(QStringLiteral("unsupported schema version 0")));
    QVERIFY(!session.isOpen());
    QVERIFY(session.databasePath().isEmpty());
    QCOMPARE(session.settingsRepository(), nullptr);
    QCOMPARE(session.campusRecordRepository(), nullptr);
}

void DataServiceLifecycleTests::classDeleteFailureRollsBackAllChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("class-delete-rollback.db"))
        ).has_value());

    const Result<int> createdClass =
        service.createClass(QStringLiteral("Rollback Class"));
    QVERIFY(createdClass);
    const int classId = *createdClass;

    ClassInfo info;
    info.classId = classId;
    QVERIFY(service.saveClassInfo(info));

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO roster_columns (class_id, name, position, width) "
        "VALUES (?, 'English', 0, 180)"
        ));
    query.addBindValue(classId);
    QVERIFY(query.exec());
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_class_info_delete "
        "BEFORE DELETE ON class_info "
        "WHEN OLD.class_id = %1 "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected class delete failure'); "
        "END"
        ).arg(classId)));

    const Status deleted = service.deleteClass(classId);
    QVERIFY(!deleted);
    QVERIFY(deleted.error().contains(
        QStringLiteral("Deleting class information")
        ));
    QVERIFY(deleted.error().contains(
        QStringLiteral("class id %1").arg(classId)
        ));

    const Result<Classroom> rolledBackClass = service.getClassById(classId);
    QVERIFY(rolledBackClass);
    QCOMPARE(rolledBackClass->id, classId);
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM roster_columns WHERE class_id=?"
        ));
    query.addBindValue(classId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void DataServiceLifecycleTests
    ::teacherDeleteFailureRollsBackClassAssignments()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("teacher-delete-rollback.db"))
        ).has_value());

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Rollback Teacher");
    const Result<int> createdTeacher = service.createTeacher(teacher);
    const Result<int> createdClass =
        service.createClass(QStringLiteral("Assigned Class"));
    QVERIFY(createdTeacher);
    QVERIFY(createdClass);

    ClassInfo info;
    info.classId = *createdClass;
    info.teacherId = *createdTeacher;
    QVERIFY(service.saveClassInfo(info));

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_teacher_delete "
        "BEFORE DELETE ON teachers "
        "WHEN OLD.id = %1 "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected teacher delete failure'); "
        "END"
        ).arg(*createdTeacher)));

    const Status deleted = service.deleteTeacher(*createdTeacher);
    QVERIFY(!deleted);
    QVERIFY(deleted.error().contains(QStringLiteral("Deleting teacher")));
    QVERIFY(deleted.error().contains(
        QStringLiteral("teacher id %1").arg(*createdTeacher)
        ));

    const Result<Teacher> rolledBackTeacher =
        service.getTeacher(*createdTeacher);
    QVERIFY(rolledBackTeacher);
    QCOMPARE(rolledBackTeacher->id, *createdTeacher);
    QCOMPARE(
        service.loadClassInfo(*createdClass)->teacherId,
        *createdTeacher
        );
}

void DataServiceLifecycleTests::campusCreationValidatesGeneratedIdAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("campus-wide-id-baseline.db"))
        ).has_value());

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO campuses (id, name) "
        "VALUES (1, 'F871 Sentinel Campus')"
        )));
    QCOMPARE(query.numRowsAffected(), 1);

    CampusRecord normalCampus;
    normalCampus.name = QStringLiteral("F871 Normal Campus");
    const Result<int> normalCampusId = service.saveCampus(normalCampus);
    QVERIFY(normalCampusId);
    QCOMPARE(*normalCampusId, 2);
    const Result<CampusRecord> reloadedNormalCampus =
        service.getCampus(*normalCampusId);
    QVERIFY(reloadedNormalCampus);
    QCOMPARE(reloadedNormalCampus->name, normalCampus.name);

    QVERIFY(query.exec(QStringLiteral(
        "UPDATE sqlite_sequence SET seq=2147483646 "
        "WHERE name='campuses'"
        )));
    QCOMPARE(query.numRowsAffected(), 1);

    CampusRecord maximumCampus;
    maximumCampus.name = QStringLiteral("F871 Maximum Campus");
    const Result<int> maximumCampusId = service.saveCampus(maximumCampus);
    QVERIFY(maximumCampusId);
    QCOMPARE(*maximumCampusId, std::numeric_limits<int>::max());
    const Result<CampusRecord> reloadedMaximumCampus =
        service.getCampus(*maximumCampusId);
    QVERIFY(reloadedMaximumCampus);
    QCOMPARE(reloadedMaximumCampus->id, *maximumCampusId);
    QCOMPARE(reloadedMaximumCampus->name, maximumCampus.name);

    QVERIFY(query.exec(QStringLiteral(
        "UPDATE sqlite_sequence SET seq=4294967296 "
        "WHERE name='campuses'"
        )));
    QCOMPARE(query.numRowsAffected(), 1);

    CampusRecord campus;
    campus.name = QStringLiteral("F871 Out Of Range Campus");
    const Result<int> createdCampus = service.saveCampus(campus);
    QVERIFY(!createdCampus);
    QVERIFY(createdCampus.error().contains(
        QStringLiteral("outside the supported integer range")
        ));

    QVERIFY(query.exec(QStringLiteral(
        "SELECT id, name, typeof(id) AS id_type "
        "FROM campuses ORDER BY id"
        )));
    QVERIFY(query.next());
    QCOMPARE(query.value("id").toLongLong(), qint64{1});
    QCOMPARE(query.value("id_type").toString(), QStringLiteral("integer"));
    QCOMPARE(query.value("name").toString(),
        QStringLiteral("F871 Sentinel Campus"));

    QVERIFY(query.next());
    QCOMPARE(query.value("id").toLongLong(), qint64{2});
    QCOMPARE(query.value("id_type").toString(), QStringLiteral("integer"));
    QCOMPARE(query.value("name").toString(),
        QStringLiteral("F871 Normal Campus"));

    QVERIFY(query.next());
    QCOMPARE(
        query.value("id").toLongLong(),
        qint64{std::numeric_limits<int>::max()}
        );
    QCOMPARE(query.value("id_type").toString(), QStringLiteral("integer"));
    QCOMPARE(query.value("name").toString(),
        QStringLiteral("F871 Maximum Campus"));
    QVERIFY(!query.next());
}

void DataServiceLifecycleTests::campusListValidatesPersistedIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("campus-list-id-validation.db"))
        ).has_value());

    const Result<QList<CampusRecord>> emptyCampuses =
        service.getAllCampuses();
    QVERIFY(emptyCampuses);
    QVERIFY(emptyCampuses->isEmpty());

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO campuses (id, name) VALUES "
        "(1, 'F872 A Campus'), "
        "(2147483647, 'F872 B Campus')"
        )));
    QCOMPARE(query.numRowsAffected(), 2);

    const Result<QList<CampusRecord>> campuses = service.getAllCampuses();
    QVERIFY(campuses);
    QCOMPARE(campuses->size(), 2);
    QCOMPARE(campuses->first().id, 1);
    QCOMPARE(campuses->first().name, QStringLiteral("F872 A Campus"));
    QCOMPARE(campuses->last().id, std::numeric_limits<int>::max());
    QCOMPARE(campuses->last().name, QStringLiteral("F872 B Campus"));

    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO campuses (id, name) "
        "VALUES (4294967297, 'F872 C Wide Campus')"
        )));
    QCOMPARE(query.numRowsAffected(), 1);

    const Result<QList<CampusRecord>> campusesWithWideId =
        service.getAllCampuses();
    QVERIFY(!campusesWithWideId);
    QVERIFY(campusesWithWideId.error().contains(
        QStringLiteral("outside the supported integer range")
        ));

    QVERIFY(query.exec(QStringLiteral(
        "SELECT id, name, typeof(id) AS id_type "
        "FROM campuses ORDER BY id"
        )));
    QVERIFY(query.next());
    QCOMPARE(query.value("id").toLongLong(), qint64{1});
    QCOMPARE(query.value("id_type").toString(), QStringLiteral("integer"));
    QCOMPARE(query.value("name").toString(), QStringLiteral("F872 A Campus"));

    QVERIFY(query.next());
    QCOMPARE(query.value("id").toLongLong(), qint64{2147483647LL});
    QCOMPARE(query.value("id_type").toString(), QStringLiteral("integer"));
    QCOMPARE(query.value("name").toString(), QStringLiteral("F872 B Campus"));

    QVERIFY(query.next());
    QCOMPARE(query.value("id").toLongLong(), qint64{4294967297LL});
    QCOMPARE(query.value("id_type").toString(), QStringLiteral("integer"));
    QCOMPARE(query.value("name").toString(),
        QStringLiteral("F872 C Wide Campus"));
    QVERIFY(!query.next());

    QVERIFY(query.prepare(QStringLiteral(
        "DELETE FROM campuses WHERE id=?"
        )));
    query.addBindValue(qint64{4294967297LL});
    QVERIFY(query.exec());
    QCOMPARE(query.numRowsAffected(), 1);

    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO campuses (id, name) "
        "VALUES (0, 'F872 C Zero Campus')"
        )));
    QCOMPARE(query.numRowsAffected(), 1);

    const Result<QList<CampusRecord>> campusesWithZeroId =
        service.getAllCampuses();
    QVERIFY(!campusesWithZeroId);
    QVERIFY(campusesWithZeroId.error().contains(
        QStringLiteral("invalid campus id 0")
        ));

    QVERIFY(query.exec(QStringLiteral(
        "SELECT id, name, typeof(id) AS id_type "
        "FROM campuses ORDER BY id"
        )));
    QVERIFY(query.next());
    QCOMPARE(query.value("id").toLongLong(), qint64{0});
    QCOMPARE(query.value("id_type").toString(), QStringLiteral("integer"));
    QCOMPARE(
        query.value("name").toString(),
        QStringLiteral("F872 C Zero Campus")
        );

    QVERIFY(query.next());
    QCOMPARE(query.value("id").toLongLong(), qint64{1});
    QCOMPARE(query.value("id_type").toString(), QStringLiteral("integer"));
    QCOMPARE(
        query.value("name").toString(),
        QStringLiteral("F872 A Campus")
        );

    QVERIFY(query.next());
    QCOMPARE(query.value("id").toLongLong(), qint64{2147483647LL});
    QCOMPARE(query.value("id_type").toString(), QStringLiteral("integer"));
    QCOMPARE(
        query.value("name").toString(),
        QStringLiteral("F872 B Campus")
        );
    QVERIFY(!query.next());
}

void DataServiceLifecycleTests::campusListRejectsLateFetchFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("late-campus-fetch.db"))
        ));

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery setup(database);
    QVERIFY(setup.exec(QStringLiteral("DROP TABLE campuses")));
    QVERIFY(setup.exec(QStringLiteral(R"(
        CREATE TABLE campus_cursor_error_backing (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            payload TEXT NOT NULL
        )
    )")));
    QVERIFY(setup.exec(QStringLiteral(R"(
        CREATE INDEX campus_cursor_error_name_idx
        ON campus_cursor_error_backing (name)
    )")));
    QVERIFY(setup.exec(QStringLiteral(R"(
        INSERT INTO campus_cursor_error_backing (id, name, payload)
        VALUES
            (1, 'A Alpha Campus', '{}'),
            (2, 'Z Broken Campus', '{')
    )")));
    QVERIFY(setup.exec(QStringLiteral(R"(
        CREATE VIEW campuses AS
        SELECT
            id,
            name,
            json_extract(payload, '$.building_name') AS building_name,
            NULL AS address,
            NULL AS phone_number,
            NULL AS office_number,
            NULL AS transit_steps,
            NULL AS arrival_info,
            NULL AS image_path,
            NULL AS office_wifi,
            NULL AS office_wifi_password,
            NULL AS printer_name,
            NULL AS printer_steps,
            NULL AS photocopier_code,
            NULL AS housing_locations
        FROM campus_cursor_error_backing
    )")));

    QSqlQuery preflight(database);
    QVERIFY(preflight.exec(QStringLiteral(
        "SELECT * FROM campuses ORDER BY name"
        )));
    QVERIFY(preflight.next());
    QCOMPARE(preflight.value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(
        preflight.value(QStringLiteral("name")).toString(),
        QStringLiteral("A Alpha Campus")
        );
    QVERIFY(!preflight.next());
    QVERIFY(preflight.lastError().isValid());
    const QString terminalCursorDetail =
        preflight.lastError().text().trimmed();
    QVERIFY(!terminalCursorDetail.isEmpty());
    QVERIFY(terminalCursorDetail.contains(
        QStringLiteral("malformed JSON"), Qt::CaseInsensitive
        ));
    preflight.finish();

    const Result<QList<CampusRecord>> failedCampuses =
        service.getAllCampuses();
    QVERIFY(!failedCampuses);
    QVERIFY(failedCampuses.error().contains(
        QStringLiteral("Loading campuses")
        ));
    QVERIFY(failedCampuses.error().contains(terminalCursorDetail));
}

void DataServiceLifecycleTests::teacherListValidatesPersistedIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("teacher-list-wide-id.db"))
        ).has_value());

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO teachers ("
        "id, teacher_kr, teacher_en, preferred_romanization, preferred_name, "
        "room_number, birthday, phone_number, wifi_name, wifi_password, "
        "internet_type, zoom_id, zoom_password, projection_type, notes"
        ") VALUES "
        "(1, 'F876 A Korean', 'F876 A Teacher', 'F876 A Romanization', "
        "'F876 A Preferred', 'F876 A Room', '01-02', '010-1000-0001', "
        "'F876 A WiFi', 'F876 A WiFi Password', 'LAN', 'F876 A Zoom', "
        "'F876 A Zoom Password', 'Zoom', 'F876 A Notes'), "
        "(2147483647, 'F876 B Korean', 'F876 B Teacher', "
        "'F876 B Romanization', 'F876 B Preferred', 'F876 B Room', "
        "'03-04', '010-2000-0002', 'F876 B WiFi', 'F876 B WiFi Password', "
        "'Both', 'F876 B Zoom', 'F876 B Zoom Password', 'Any', "
        "'F876 B Notes')"
        )));
    QCOMPARE(query.numRowsAffected(), 2);

    const auto snapshotTeacherRows = [&database]()
        {
            QList<QStringList> rows;
            QSqlQuery snapshotQuery(database);
            if (!snapshotQuery.exec(QStringLiteral(
                "SELECT id, teacher_kr, teacher_en, preferred_romanization, "
                "preferred_name, room_number, birthday, phone_number, "
                "wifi_name, wifi_password, internet_type, zoom_id, "
                "zoom_password, projection_type, notes, "
                "typeof(id) AS id_type "
                "FROM teachers ORDER BY id"
                )))
            {
                return rows;
            }

            while (snapshotQuery.next())
            {
                rows.append({
                    snapshotQuery.value("id").toString(),
                    snapshotQuery.value("teacher_kr").toString(),
                    snapshotQuery.value("teacher_en").toString(),
                    snapshotQuery.value("preferred_romanization").toString(),
                    snapshotQuery.value("preferred_name").toString(),
                    snapshotQuery.value("room_number").toString(),
                    snapshotQuery.value("birthday").toString(),
                    snapshotQuery.value("phone_number").toString(),
                    snapshotQuery.value("wifi_name").toString(),
                    snapshotQuery.value("wifi_password").toString(),
                    snapshotQuery.value("internet_type").toString(),
                    snapshotQuery.value("zoom_id").toString(),
                    snapshotQuery.value("zoom_password").toString(),
                    snapshotQuery.value("projection_type").toString(),
                    snapshotQuery.value("notes").toString(),
                    snapshotQuery.value("id_type").toString()
                });
            }
            return rows;
        };

    const Result<QList<Teacher>> teachers = service.getAllTeachers();
    QVERIFY(teachers);
    QCOMPARE(teachers->size(), 2);
    QCOMPARE(teachers->first().id, 1);
    QCOMPARE(teachers->first().teacherEn, QStringLiteral("F876 A Teacher"));
    QCOMPARE(teachers->first().teacherKr, QStringLiteral("F876 A Korean"));
    QCOMPARE(teachers->first().preferredRomanization,
        QStringLiteral("F876 A Romanization"));
    QCOMPARE(teachers->first().preferredName, QStringLiteral("F876 A Preferred"));
    QCOMPARE(teachers->first().roomNumber, QStringLiteral("F876 A Room"));
    QCOMPARE(teachers->first().birthday, QStringLiteral("01-02"));
    QCOMPARE(teachers->first().phoneNumber, QStringLiteral("010-1000-0001"));
    QCOMPARE(teachers->first().wifiName, QStringLiteral("F876 A WiFi"));
    QCOMPARE(teachers->first().wifiPassword,
        QStringLiteral("F876 A WiFi Password"));
    QCOMPARE(teachers->first().internetType, QStringLiteral("LAN"));
    QCOMPARE(teachers->first().zoomId, QStringLiteral("F876 A Zoom"));
    QCOMPARE(teachers->first().zoomPassword,
        QStringLiteral("F876 A Zoom Password"));
    QCOMPARE(teachers->first().projectionType, QStringLiteral("Zoom"));
    QCOMPARE(teachers->first().notes, QStringLiteral("F876 A Notes"));
    QCOMPARE(teachers->last().id, std::numeric_limits<int>::max());
    QCOMPARE(teachers->last().teacherEn, QStringLiteral("F876 B Teacher"));
    QCOMPARE(teachers->last().teacherKr, QStringLiteral("F876 B Korean"));
    QCOMPARE(teachers->last().preferredRomanization,
        QStringLiteral("F876 B Romanization"));
    QCOMPARE(teachers->last().preferredName, QStringLiteral("F876 B Preferred"));
    QCOMPARE(teachers->last().roomNumber, QStringLiteral("F876 B Room"));
    QCOMPARE(teachers->last().birthday, QStringLiteral("03-04"));
    QCOMPARE(teachers->last().phoneNumber, QStringLiteral("010-2000-0002"));
    QCOMPARE(teachers->last().wifiName, QStringLiteral("F876 B WiFi"));
    QCOMPARE(teachers->last().wifiPassword,
        QStringLiteral("F876 B WiFi Password"));
    QCOMPARE(teachers->last().internetType, QStringLiteral("Both"));
    QCOMPARE(teachers->last().zoomId, QStringLiteral("F876 B Zoom"));
    QCOMPARE(teachers->last().zoomPassword,
        QStringLiteral("F876 B Zoom Password"));
    QCOMPARE(teachers->last().projectionType, QStringLiteral("Any"));
    QCOMPARE(teachers->last().notes, QStringLiteral("F876 B Notes"));

    const QList<QStringList> expectedRows = {
        {
            QStringLiteral("1"), QStringLiteral("F876 A Korean"),
            QStringLiteral("F876 A Teacher"),
            QStringLiteral("F876 A Romanization"),
            QStringLiteral("F876 A Preferred"), QStringLiteral("F876 A Room"),
            QStringLiteral("01-02"), QStringLiteral("010-1000-0001"),
            QStringLiteral("F876 A WiFi"), QStringLiteral("F876 A WiFi Password"),
            QStringLiteral("LAN"), QStringLiteral("F876 A Zoom"),
            QStringLiteral("F876 A Zoom Password"), QStringLiteral("Zoom"),
            QStringLiteral("F876 A Notes"), QStringLiteral("integer")
        },
        {
            QStringLiteral("2147483647"), QStringLiteral("F876 B Korean"),
            QStringLiteral("F876 B Teacher"),
            QStringLiteral("F876 B Romanization"),
            QStringLiteral("F876 B Preferred"), QStringLiteral("F876 B Room"),
            QStringLiteral("03-04"), QStringLiteral("010-2000-0002"),
            QStringLiteral("F876 B WiFi"), QStringLiteral("F876 B WiFi Password"),
            QStringLiteral("Both"), QStringLiteral("F876 B Zoom"),
            QStringLiteral("F876 B Zoom Password"), QStringLiteral("Any"),
            QStringLiteral("F876 B Notes"), QStringLiteral("integer")
        }
    };
    QCOMPARE(snapshotTeacherRows(), expectedRows);

    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO teachers ("
        "id, teacher_kr, teacher_en, preferred_romanization, preferred_name, "
        "room_number, birthday, phone_number, wifi_name, wifi_password, "
        "internet_type, zoom_id, zoom_password, projection_type, notes"
        ") VALUES (4294967297, 'F876 C Wide Korean', "
        "'F876 C Wide Teacher', 'F876 C Wide Romanization', "
        "'F876 C Wide Preferred', 'F876 C Wide Room', '05-06', "
        "'010-3000-0003', 'F876 C Wide WiFi', 'F876 C Wide WiFi Password', "
        "'N/A', 'F876 C Wide Zoom', 'F876 C Wide Zoom Password', 'HDMI', "
        "'F876 C Wide Notes')"
        )));
    QCOMPARE(query.numRowsAffected(), 1);

    const QList<QStringList> expectedRowsWithWideId = {
        expectedRows.at(0),
        expectedRows.at(1),
        {
            QStringLiteral("4294967297"), QStringLiteral("F876 C Wide Korean"),
            QStringLiteral("F876 C Wide Teacher"),
            QStringLiteral("F876 C Wide Romanization"),
            QStringLiteral("F876 C Wide Preferred"),
            QStringLiteral("F876 C Wide Room"), QStringLiteral("05-06"),
            QStringLiteral("010-3000-0003"), QStringLiteral("F876 C Wide WiFi"),
            QStringLiteral("F876 C Wide WiFi Password"), QStringLiteral("N/A"),
            QStringLiteral("F876 C Wide Zoom"),
            QStringLiteral("F876 C Wide Zoom Password"), QStringLiteral("HDMI"),
            QStringLiteral("F876 C Wide Notes"),
            QStringLiteral("integer")
        }
    };
    const QList<QStringList> rowsBeforeWideRead = snapshotTeacherRows();
    QCOMPARE(rowsBeforeWideRead, expectedRowsWithWideId);

    const Result<QList<Teacher>> teachersWithWideId = service.getAllTeachers();
    QVERIFY(!teachersWithWideId);
    QVERIFY(!teachersWithWideId.error().isEmpty());
    QCOMPARE(snapshotTeacherRows(), rowsBeforeWideRead);

    QVERIFY(query.prepare(QStringLiteral(
        "DELETE FROM teachers WHERE id=?"
        )));
    query.addBindValue(qint64{4294967297LL});
    QVERIFY(query.exec());
    QCOMPARE(query.numRowsAffected(), 1);
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO teachers ("
        "id, teacher_kr, teacher_en, preferred_romanization, preferred_name, "
        "room_number, birthday, phone_number, wifi_name, wifi_password, "
        "internet_type, zoom_id, zoom_password, projection_type, notes"
        ") VALUES (0, 'F876 C Zero Korean', 'F876 C Zero Teacher', "
        "'F876 C Zero Romanization', 'F876 C Zero Preferred', "
        "'F876 C Zero Room', '07-08', '010-4000-0004', 'F876 C Zero WiFi', "
        "'F876 C Zero WiFi Password', 'LAN', 'F876 C Zero Zoom', "
        "'F876 C Zero Zoom Password', 'Zoom', 'F876 C Zero Notes')"
        )));
    QCOMPARE(query.numRowsAffected(), 1);

    const QList<QStringList> expectedRowsWithZeroId = {
        {
            QStringLiteral("0"), QStringLiteral("F876 C Zero Korean"),
            QStringLiteral("F876 C Zero Teacher"),
            QStringLiteral("F876 C Zero Romanization"),
            QStringLiteral("F876 C Zero Preferred"),
            QStringLiteral("F876 C Zero Room"), QStringLiteral("07-08"),
            QStringLiteral("010-4000-0004"), QStringLiteral("F876 C Zero WiFi"),
            QStringLiteral("F876 C Zero WiFi Password"), QStringLiteral("LAN"),
            QStringLiteral("F876 C Zero Zoom"),
            QStringLiteral("F876 C Zero Zoom Password"), QStringLiteral("Zoom"),
            QStringLiteral("F876 C Zero Notes"), QStringLiteral("integer")
        },
        expectedRows.at(0),
        expectedRows.at(1)
    };
    const QList<QStringList> rowsBeforeZeroRead = snapshotTeacherRows();
    QCOMPARE(rowsBeforeZeroRead, expectedRowsWithZeroId);

    const Result<QList<Teacher>> teachersWithZeroId = service.getAllTeachers();
    QVERIFY(!teachersWithZeroId);
    QVERIFY(!teachersWithZeroId.error().isEmpty());
    QCOMPARE(snapshotTeacherRows(), rowsBeforeZeroRead);
}

void DataServiceLifecycleTests::repositoryWriteFailuresAreObservable()
{
    DataService unavailableService;
    QVERIFY(!unavailableService.getTeacher(1));
    QVERIFY(!unavailableService.getAllTeachers());
    QVERIFY(!unavailableService.getClassById(1));
    QVERIFY(!unavailableService.getClasses());
    QVERIFY(!unavailableService.saveSetting(
        QStringLiteral("unavailable"),
        QStringLiteral("value")
        ));
    QVERIFY(!unavailableService.saveIntensiveSlotState(
        QStringLiteral("Monday"),
        QStringLiteral("09:00"),
        QStringLiteral("lunch")
        ));
    QVERIFY(!unavailableService.saveCampus(CampusRecord{}));
    QVERIFY(!unavailableService.getCampus(1));
    QVERIFY(!unavailableService.getAllCampuses());
    QVERIFY(!unavailableService.deleteCampus(1));
    QVERIFY(!unavailableService.saveCalendarEvent(CalendarEvent{}));
    QVERIFY(!unavailableService.saveCalendarEvents({CalendarEvent{}}));
    QVERIFY(!unavailableService.deleteCalendarEvent(1));
    QVERIFY(!unavailableService.deleteCalendarEventsForRepeatSeriesFromDate(
        QStringLiteral("series"),
        QDate(2026, 8, 1)
        ));
    QVERIFY(!unavailableService.deleteAllCalendarEvents());
    QVERIFY(!unavailableService.getClassTimeConflicts(
        1,
        {},
        ScheduleType::Regular
        ));
    QVERIFY(!unavailableService.saveClassInfo(ClassInfo{}));
    QVERIFY(!unavailableService.saveClassNotes(
        1,
        QStringLiteral("notes"),
        QStringLiteral("activities")
        ));
    QVERIFY(!unavailableService.saveRoster(1, Roster{}));
    QVERIFY(!unavailableService.saveRosters({qMakePair(1, Roster{})}));
    QVERIFY(!unavailableService.saveSpeakingEval(
        1,
        QStringLiteral("Evaluation"),
        SpeakingEval::emptyRows()
        ));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("write-failures.db"))
        ).has_value());

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery query(database);

    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_setting_write "
        "BEFORE INSERT ON app_settings "
        "WHEN NEW.key = 'failure/key' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected setting failure'); "
        "END"
        )));
    const Status settingSaved = service.saveSetting(
        QStringLiteral("failure/key"),
        QStringLiteral("value")
        );
    QVERIFY(!settingSaved);
    QVERIFY(settingSaved.error().contains(
        QStringLiteral("Saving application setting")
        ));
    QVERIFY(settingSaved.error().contains(QStringLiteral("failure/key")));

    const Status settingsSaved = service.saveSettings({
        {
            QStringLiteral("failure/first"),
            QStringLiteral("must roll back")
        },
        {
            QStringLiteral("failure/key"),
            QStringLiteral("must fail")
        }
    });
    QVERIFY(!settingsSaved);
    const Result<QVariant> rolledBackSetting =
        service.loadSetting(QStringLiteral("failure/first"));
    QVERIFY(rolledBackSetting);
    QVERIFY(!rolledBackSetting->isValid());

    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_slot_write "
        "BEFORE INSERT ON intensive_slot_states "
        "WHEN NEW.day = 'Failureday' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected slot failure'); "
        "END"
        )));
    const Status slotSaved = service.saveIntensiveSlotState(
        QStringLiteral("Failureday"),
        QStringLiteral("09:00"),
        QStringLiteral("lunch")
        );
    QVERIFY(!slotSaved);
    QVERIFY(slotSaved.error().contains(
        QStringLiteral("Saving intensive slot state")
        ));
    QVERIFY(slotSaved.error().contains(
        QStringLiteral("Failureday at 09:00")
        ));

    CampusRecord campus;
    campus.name = QStringLiteral("Failure Campus");
    const Result<int> campusCreated = service.saveCampus(campus);
    QVERIFY(campusCreated);
    campus.id = *campusCreated;
    campus.officeNumber = QStringLiteral("Changed");

    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_campus_update "
        "BEFORE UPDATE ON campuses "
        "WHEN OLD.id = %1 "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected campus update failure'); "
        "END"
        ).arg(campus.id)));
    const Result<int> campusUpdated = service.saveCampus(campus);
    QVERIFY(!campusUpdated);
    QVERIFY(campusUpdated.error().contains(QStringLiteral("Updating campus")));
    QVERIFY(campusUpdated.error().contains(
        QStringLiteral("campus id %1").arg(campus.id)
        ));
    const Result<CampusRecord> campusAfterFailedUpdate =
        service.getCampus(campus.id);
    QVERIFY(campusAfterFailedUpdate);
    QCOMPARE(campusAfterFailedUpdate->officeNumber, QString());

    QVERIFY(query.exec(QStringLiteral("DROP TRIGGER reject_campus_update")));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_campus_delete "
        "BEFORE DELETE ON campuses "
        "WHEN OLD.id = %1 "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected campus delete failure'); "
        "END"
        ).arg(campus.id)));
    const Status campusDeleted = service.deleteCampus(campus.id);
    QVERIFY(!campusDeleted);
    QVERIFY(campusDeleted.error().contains(QStringLiteral("Deleting campus")));
    QVERIFY(campusDeleted.error().contains(
        QStringLiteral("campus id %1").arg(campus.id)
        ));
    const Result<CampusRecord> campusAfterFailedDelete =
        service.getCampus(campus.id);
    QVERIFY(campusAfterFailedDelete);
    QCOMPARE(campusAfterFailedDelete->id, campus.id);

    const Result<CampusRecord> missingCampus =
        service.getCampus(campus.id + 1000);
    QVERIFY(!missingCampus);
    QVERIFY(missingCampus.error().contains(
        QStringLiteral("no matching record exists")
        ));

    QVERIFY(query.exec(QStringLiteral("DROP TABLE campuses")));

    const Result<CampusRecord> failedCampusRead =
        service.getCampus(campus.id);
    QVERIFY(!failedCampusRead);
    QVERIFY(failedCampusRead.error().contains(
        QStringLiteral("Loading campus")
        ));
    QVERIFY(failedCampusRead.error().contains(
        QStringLiteral("campus id %1").arg(campus.id)
        ));

    const Result<QList<CampusRecord>> failedCampusList =
        service.getAllCampuses();
    QVERIFY(!failedCampusList);
    QVERIFY(failedCampusList.error().contains(
        QStringLiteral("Loading campuses")
        ));
}

void DataServiceLifecycleTests::coreLookupReadFailuresAreObservable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("core-read-failures.db"))
        ).has_value());

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Readable Teacher");
    const Result<int> teacherId = service.createTeacher(teacher);
    const Result<int> classId = service.createClass(
        QStringLiteral("Readable Class")
        );
    QVERIFY(teacherId);
    QVERIFY(classId);

    const Result<Teacher> loadedTeacher = service.getTeacher(*teacherId);
    const Result<Classroom> loadedClass = service.getClassById(*classId);
    const Result<QList<Teacher>> loadedTeachers = service.getAllTeachers();
    const Result<QList<Classroom>> loadedClasses = service.getClasses();
    QVERIFY(loadedTeacher);
    QVERIFY(loadedClass);
    QVERIFY(loadedTeachers);
    QVERIFY(loadedClasses);
    QCOMPARE(loadedTeacher->teacherEn, QStringLiteral("Readable Teacher"));
    QCOMPARE(loadedClass->name, QStringLiteral("Readable Class"));
    QCOMPARE(loadedTeachers->size(), 1);
    QCOMPARE(loadedClasses->size(), 1);

    const Result<Teacher> missingTeacher =
        service.getTeacher(*teacherId + 1000);
    const Result<Classroom> missingClass =
        service.getClassById(*classId + 1000);
    QVERIFY(!missingTeacher);
    QVERIFY(!missingClass);
    QVERIFY(missingTeacher.error().contains(
        QStringLiteral("no matching record exists")
        ));
    QVERIFY(missingTeacher.error().contains(
        QStringLiteral("teacher id %1").arg(*teacherId + 1000)
        ));
    QVERIFY(missingClass.error().contains(
        QStringLiteral("no matching record exists")
        ));
    QVERIFY(missingClass.error().contains(
        QStringLiteral("class id %1").arg(*classId + 1000)
        ));

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral("DROP TABLE classes")));

    const Result<Classroom> failedClass = service.getClassById(*classId);
    const Result<QList<Classroom>> failedClasses = service.getClasses();
    QVERIFY(!failedClass);
    QVERIFY(!failedClasses);
    QVERIFY(failedClass.error().contains(QStringLiteral("Loading class")));
    QVERIFY(failedClass.error().contains(
        QStringLiteral("class id %1").arg(*classId)
        ));
    QVERIFY(failedClasses.error().contains(QStringLiteral("Loading classes")));

    QVERIFY(query.exec(QStringLiteral("DROP TABLE teachers")));

    const Result<Teacher> failedTeacher = service.getTeacher(*teacherId);
    const Result<QList<Teacher>> failedTeachers = service.getAllTeachers();
    QVERIFY(!failedTeacher);
    QVERIFY(!failedTeachers);
    QVERIFY(failedTeacher.error().contains(QStringLiteral("Loading teacher")));
    QVERIFY(failedTeacher.error().contains(
        QStringLiteral("teacher id %1").arg(*teacherId)
        ));
    QVERIFY(failedTeachers.error().contains(
        QStringLiteral("Loading teachers")
        ));
}

void DataServiceLifecycleTests::teacherDirectoryRejectsLateFetchFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("late-teacher-directory-fetch.db"))
        ));

    QSqlDatabase database = service.databaseSession()->database();
    const QString setupError =
        TeacherChoicesCursorErrorFixture::install(database);
    QVERIFY2(setupError.isEmpty(), qPrintable(setupError));

    QSqlQuery preflight(database);
    QVERIFY(preflight.exec(QStringLiteral(
        "SELECT * FROM teachers ORDER BY teacher_en"
        )));
    QVERIFY(preflight.next());
    QCOMPARE(preflight.value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(
        preflight.value(QStringLiteral("teacher_en")).toString(),
        QStringLiteral("A Alpha")
        );
    QVERIFY(!preflight.next());
    QVERIFY(preflight.lastError().isValid());
    QVERIFY(preflight.lastError().text().contains(
        QStringLiteral("malformed JSON"), Qt::CaseInsensitive
        ));
    preflight.finish();

    const Result<QList<Teacher>> failedTeachers = service.getAllTeachers();
    QVERIFY(!failedTeachers);
    QVERIFY(failedTeachers.error().contains(
        QStringLiteral("Loading teachers")
        ));
    QVERIFY(failedTeachers.error().contains(
        QStringLiteral("malformed JSON"), Qt::CaseInsensitive
        ));
}

void DataServiceLifecycleTests::compoundAndCollectionReadFailuresAreObservable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("compound-read-failures.db"))
        ).has_value());

    const Result<int> classId = service.createClass(QStringLiteral("Read Contract"));
    QVERIFY(classId);
    const QDate date(2026, 9, 1);

    // These success cases establish the legitimate empty/default states that
    // callers may receive before a database failure occurs.
    const Result<ClassInfo> emptyClassInfo = service.loadClassInfo(*classId);
    const Result<Roster> emptyRoster = service.loadRoster(*classId);
    const Result<SpeakingEvalRows> emptyEvaluation = service.loadSpeakingEval(
        *classId,
        QStringLiteral("Week 1")
        );
    const Result<QList<CalendarEvent>> emptyCalendarEvents =
        service.loadCalendarEventsForDate(date);
    const Result<QVariant> missingSetting = service.loadSetting(
        QStringLiteral("missing/read-contract")
        );
    QVERIFY(emptyClassInfo);
    QCOMPARE(emptyClassInfo->classId, *classId);
    QVERIFY(emptyRoster);
    QVERIFY(emptyRoster->columns.isEmpty());
    QVERIFY(emptyEvaluation);
    QVERIFY(emptyEvaluation->isEmpty());
    QVERIFY(emptyCalendarEvents);
    QVERIFY(emptyCalendarEvents->isEmpty());
    QVERIFY(missingSetting);
    QVERIFY(!missingSetting->isValid());

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery query(database);

    QVERIFY(query.exec(QStringLiteral("DROP TABLE class_times")));
    const Result<ClassInfo> failedClassInfo = service.loadClassInfo(*classId);
    QVERIFY(!failedClassInfo);
    QVERIFY(failedClassInfo.error().contains(
        QStringLiteral("Loading regular class times")
        ));
    QVERIFY(failedClassInfo.error().contains(
        QStringLiteral("class id %1").arg(*classId)
        ));

    QVERIFY(query.exec(QStringLiteral("DROP TABLE roster_columns")));
    const Result<Roster> failedRoster = service.loadRoster(*classId);
    QVERIFY(!failedRoster);
    QVERIFY(failedRoster.error().contains(
        QStringLiteral("Loading roster columns")
        ));
    QVERIFY(failedRoster.error().contains(
        QStringLiteral("class id %1").arg(*classId)
        ));

    QVERIFY(query.exec(QStringLiteral("DROP TABLE speaking_evaluations")));
    const Result<SpeakingEvalRows> failedEvaluation = service.loadSpeakingEval(
        *classId,
        QStringLiteral("Week 1")
        );
    QVERIFY(!failedEvaluation);
    QVERIFY(failedEvaluation.error().contains(
        QStringLiteral("Loading speaking evaluation")
        ));
    QVERIFY(failedEvaluation.error().contains(
        QStringLiteral("class id %1, evaluation 'Week 1'").arg(*classId)
        ));

    QVERIFY(query.exec(QStringLiteral("DROP TABLE calendar_events")));
    const Result<QList<CalendarEvent>> failedCalendarEvents =
        service.loadCalendarEventsForDate(date);
    QVERIFY(!failedCalendarEvents);
    QVERIFY(failedCalendarEvents.error().contains(
        QStringLiteral("Loading calendar events for date")
        ));
    QVERIFY(failedCalendarEvents.error().contains(
        QStringLiteral("date 2026-09-01")
        ));

    QVERIFY(query.exec(QStringLiteral("DROP TABLE app_settings")));
    const Result<QVariant> failedSetting = service.loadSetting(
        QStringLiteral("missing/read-contract")
        );
    QVERIFY(!failedSetting);
    QVERIFY(failedSetting.error().contains(
        QStringLiteral("Loading application setting")
        ));
    QVERIFY(failedSetting.error().contains(
        QStringLiteral("setting key 'missing/read-contract'")
        ));
}

void DataServiceLifecycleTests::
speakingEvaluationRejectsLateScoreRowFetchFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("late-speaking-evaluation-fetch.db"))
        ));

    const Result<int> createdClass =
        service.createClass(QStringLiteral("Late Speaking Fetch Class"));
    QVERIFY(createdClass);
    const int classId = *createdClass;

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery setup(database);
    QVERIFY(setup.exec(QStringLiteral("DROP TABLE speaking_eval_data")));
    QVERIFY(setup.exec(R"(
        CREATE TABLE speaking_evaluation_json_rows (
            id INTEGER PRIMARY KEY,
            evaluation_id INTEGER,
            row_index INTEGER,
            payload TEXT
        )
    )"));
    QVERIFY(setup.exec(R"(
        CREATE INDEX speaking_evaluation_json_order
        ON speaking_evaluation_json_rows (evaluation_id, row_index)
    )"));
    QVERIFY(setup.prepare(QStringLiteral(
        "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
        "VALUES (?, ?)"
        )));
    setup.addBindValue(classId);
    setup.addBindValue(QStringLiteral("Late Fetch"));
    QVERIFY(setup.exec());
    const qint64 evaluationId = setup.lastInsertId().toLongLong();
    QVERIFY(evaluationId > 0);

    QVERIFY(setup.prepare(QStringLiteral(
        "INSERT INTO speaking_evaluation_json_rows "
        "(evaluation_id, row_index, payload) VALUES (?, ?, ?)"
        )));
    setup.addBindValue(evaluationId);
    setup.addBindValue(0);
    setup.addBindValue(QStringLiteral(
        "{\"col_0\":\"valid prefix row\",\"col_1\":\"name\"}"
        ));
    QVERIFY(setup.exec());
    QVERIFY(setup.prepare(QStringLiteral(
        "INSERT INTO speaking_evaluation_json_rows "
        "(evaluation_id, row_index, payload) VALUES (?, ?, ?)"
        )));
    setup.addBindValue(evaluationId);
    setup.addBindValue(1);
    setup.addBindValue(QStringLiteral("{malformed"));
    QVERIFY(setup.exec());

    QVERIFY(setup.exec(R"(
        CREATE VIEW speaking_eval_data AS
        SELECT
            evaluation_id,
            row_index,
            json_extract(payload, '$.col_0') AS col_0,
            json_extract(payload, '$.col_1') AS col_1,
            json_extract(payload, '$.col_2') AS col_2,
            json_extract(payload, '$.col_3') AS col_3,
            json_extract(payload, '$.col_4') AS col_4,
            json_extract(payload, '$.col_5') AS col_5,
            json_extract(payload, '$.col_6') AS col_6,
            json_extract(payload, '$.col_7') AS col_7,
            json_extract(payload, '$.col_8') AS col_8,
            json_extract(payload, '$.col_9') AS col_9,
            json_extract(payload, '$.col_10') AS col_10
        FROM speaking_evaluation_json_rows
    )"));

    QSqlQuery preflight(database);
    QVERIFY(preflight.prepare(QStringLiteral(
        "SELECT *, typeof(row_index) AS row_index_storage_type "
        "FROM speaking_eval_data WHERE evaluation_id=? ORDER BY row_index"
        )));
    preflight.addBindValue(evaluationId);
    QVERIFY(preflight.exec());
    QVERIFY(preflight.next());
    QCOMPARE(preflight.value(QStringLiteral("row_index")).toInt(), 0);
    QCOMPARE(
        preflight.value(QStringLiteral("col_0")).toString(),
        QStringLiteral("valid prefix row")
        );
    QVERIFY(!preflight.next());
    QVERIFY(preflight.lastError().isValid());
    QVERIFY(preflight.lastError().text().contains(
        QStringLiteral("malformed JSON"), Qt::CaseInsensitive
        ));
    preflight.finish();

    const Result<SpeakingEvalRows> failedEvaluation = service.loadSpeakingEval(
        classId,
        QStringLiteral("Late Fetch")
        );
    QVERIFY(!failedEvaluation);
    QVERIFY(failedEvaluation.error().contains(
        QStringLiteral("Loading speaking evaluation rows")
        ));
    QVERIFY(failedEvaluation.error().contains(
        QStringLiteral("class id %1, evaluation 'Late Fetch'").arg(classId)
        ));
    QVERIFY(failedEvaluation.error().contains(
        QStringLiteral("malformed JSON"), Qt::CaseInsensitive
        ));
}

void DataServiceLifecycleTests::classInfoRejectsLateRegularScheduleFetchFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("late-regular-schedule-fetch.db"))
        ));

    const Result<int> createdClass =
        service.createClass(QStringLiteral("Late Regular Fetch Class"));
    QVERIFY(createdClass);
    const int classId = *createdClass;

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery setup(database);
    QVERIFY(setup.exec(QStringLiteral("DROP TABLE class_times")));
    QVERIFY(setup.exec(R"(
        CREATE TABLE regular_class_times_json (
            id INTEGER PRIMARY KEY,
            class_id INTEGER,
            payload TEXT
        )
    )"));
    QVERIFY(setup.prepare(QStringLiteral(
        "INSERT INTO regular_class_times_json (id, class_id, payload) "
        "VALUES (?, ?, ?)"
        )));
    setup.addBindValue(1);
    setup.addBindValue(classId);
    setup.addBindValue(QStringLiteral(
        "{\"day\":\"Monday\",\"start_time\":\"3:00 PM\","
        "\"end_time\":\"3:50 PM\"}"
        ));
    QVERIFY(setup.exec());
    setup.prepare(QStringLiteral(
        "INSERT INTO regular_class_times_json (id, class_id, payload) "
        "VALUES (?, ?, ?)"
        ));
    setup.addBindValue(2);
    setup.addBindValue(classId);
    setup.addBindValue(QStringLiteral("{malformed"));
    QVERIFY(setup.exec());
    QVERIFY(setup.exec(R"(
        CREATE VIEW class_times AS
        SELECT
            id,
            class_id,
            json_extract(payload, '$.day') AS day,
            json_extract(payload, '$.start_time') AS start_time,
            json_extract(payload, '$.end_time') AS end_time
        FROM regular_class_times_json
    )"));

    QSqlQuery preflight(database);
    QVERIFY(preflight.prepare(QStringLiteral(
        "SELECT * FROM class_times WHERE class_id = ? ORDER BY id"
        )));
    preflight.addBindValue(classId);
    QVERIFY(preflight.exec());
    QVERIFY(preflight.next());
    QCOMPARE(preflight.value(QStringLiteral("id")).toInt(), 1);
    QVERIFY(!preflight.next());
    QVERIFY(preflight.lastError().isValid());
    QVERIFY(preflight.lastError().text().contains(
        QStringLiteral("malformed JSON"), Qt::CaseInsensitive
        ));

    const Result<ClassInfo> failedClassInfo = service.loadClassInfo(classId);
    QVERIFY(!failedClassInfo);
    QVERIFY(failedClassInfo.error().contains(
        QStringLiteral("Loading regular class times")
        ));
    QVERIFY(failedClassInfo.error().contains(
        QStringLiteral("class id %1").arg(classId)
        ));
    QVERIFY(failedClassInfo.error().contains(
        QStringLiteral("malformed JSON"), Qt::CaseInsensitive
        ));
}

void DataServiceLifecycleTests::classInfoRejectsLateIntensiveScheduleFetchFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("late-intensive-schedule-fetch.db"))
        ));

    const Result<int> createdClass =
        service.createClass(QStringLiteral("Late Intensive Fetch Class"));
    QVERIFY(createdClass);
    const int classId = *createdClass;

    ClassInfo initialInfo;
    initialInfo.classId = classId;
    initialInfo.classTimes.append({
        QStringLiteral("Tuesday"),
        QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")
    });
    QVERIFY(service.saveClassInfo(initialInfo));

    const Result<ClassInfo> readableRegularSchedule =
        service.loadClassInfo(classId);
    QVERIFY(readableRegularSchedule);
    QCOMPARE(readableRegularSchedule->classTimes.size(), 1);
    QCOMPARE(
        readableRegularSchedule->classTimes.first().day,
        QStringLiteral("Tuesday")
        );

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery setup(database);
    QVERIFY(setup.exec(QStringLiteral("DROP TABLE class_intensive_times")));
    QVERIFY(setup.exec(R"(
        CREATE TABLE intensive_class_times_json (
            id INTEGER PRIMARY KEY,
            class_id INTEGER,
            payload TEXT
        )
    )"));
    QVERIFY(setup.prepare(QStringLiteral(
        "INSERT INTO intensive_class_times_json (id, class_id, payload) "
        "VALUES (?, ?, ?)"
        )));
    setup.addBindValue(1);
    setup.addBindValue(classId);
    setup.addBindValue(QStringLiteral(
        "{\"day\":\"Wednesday\",\"start_time\":\"5:00 PM\","
        "\"end_time\":\"5:50 PM\"}"
        ));
    QVERIFY(setup.exec());
    setup.prepare(QStringLiteral(
        "INSERT INTO intensive_class_times_json (id, class_id, payload) "
        "VALUES (?, ?, ?)"
        ));
    setup.addBindValue(2);
    setup.addBindValue(classId);
    setup.addBindValue(QStringLiteral("{malformed"));
    QVERIFY(setup.exec());
    QVERIFY(setup.exec(R"(
        CREATE VIEW class_intensive_times AS
        SELECT
            id,
            class_id,
            json_extract(payload, '$.day') AS day,
            json_extract(payload, '$.start_time') AS start_time,
            json_extract(payload, '$.end_time') AS end_time
        FROM intensive_class_times_json
    )"));

    QSqlQuery preflight(database);
    QVERIFY(preflight.prepare(QStringLiteral(
        "SELECT * FROM class_intensive_times WHERE class_id = ? ORDER BY id"
        )));
    preflight.addBindValue(classId);
    QVERIFY(preflight.exec());
    QVERIFY(preflight.next());
    QCOMPARE(preflight.value(QStringLiteral("id")).toInt(), 1);
    QVERIFY(!preflight.next());
    QVERIFY(preflight.lastError().isValid());
    QVERIFY(preflight.lastError().text().contains(
        QStringLiteral("malformed JSON"), Qt::CaseInsensitive
        ));

    const Result<ClassInfo> failedClassInfo = service.loadClassInfo(classId);
    QVERIFY(!failedClassInfo);
    QVERIFY(failedClassInfo.error().contains(
        QStringLiteral("Loading intensive class times")
        ));
    QVERIFY(failedClassInfo.error().contains(
        QStringLiteral("class id %1").arg(classId)
        ));
    QVERIFY(failedClassInfo.error().contains(
        QStringLiteral("malformed JSON"), Qt::CaseInsensitive
        ));
}

void DataServiceLifecycleTests::speakingEvaluationRepositoryPreservesWideIds()
{
    constexpr qint64 wideEvaluationId = 2147483648LL;

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("wide-speaking-evaluation-ids.db"))
        ));

    const Result<int> createdClass =
        service.createClass(QStringLiteral("Wide Evaluation Class"));
    QVERIFY(createdClass);
    const int classId = *createdClass;

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery query(database);
    QVERIFY(query.prepare(QStringLiteral(
        "INSERT INTO speaking_evaluations (id, class_id, evaluation_name) "
        "VALUES (?, ?, ?)"
        )));
    query.addBindValue(wideEvaluationId);
    query.addBindValue(classId);
    query.addBindValue(QStringLiteral("Seeded Wide Evaluation"));
    QVERIFY(query.exec());

    QVERIFY(query.prepare(QStringLiteral(
        "INSERT INTO speaking_eval_data "
        "(evaluation_id, row_index, col_0, col_1) VALUES (?, ?, ?, ?)"
        )));
    query.addBindValue(wideEvaluationId);
    query.addBindValue(0);
    query.addBindValue(QStringLiteral("Original Student"));
    query.addBindValue(QStringLiteral("Original Comment"));
    QVERIFY(query.exec());
    query.finish();
    QVERIFY(query.prepare(QStringLiteral(
        "INSERT INTO speaking_eval_data "
        "(evaluation_id, row_index, col_0) VALUES (?, ?, ?)"
        )));
    query.addBindValue(wideEvaluationId);
    query.addBindValue(1);
    query.addBindValue(QStringLiteral("Second Student"));
    QVERIFY(query.exec());

    const Result<SpeakingEvalRows> seededRows = service.loadSpeakingEval(
        classId,
        QStringLiteral("Seeded Wide Evaluation")
        );
    QVERIFY(seededRows);
    QCOMPARE(seededRows->size(), 2);
    QCOMPARE((*seededRows)[0][0], QStringLiteral("Original Student"));
    QCOMPARE((*seededRows)[0][1], QStringLiteral("Original Comment"));
    QCOMPARE((*seededRows)[1][0], QStringLiteral("Second Student"));

    SpeakingEvalRows changedRows = SpeakingEval::emptyRows();
    changedRows[0][0] = QStringLiteral("Updated Student");
    QVERIFY(service.saveSpeakingEval(
        classId,
        QStringLiteral("Seeded Wide Evaluation"),
        changedRows,
        {{0, 0}}
        ));

    QVERIFY(query.prepare(QStringLiteral(
        "SELECT id, typeof(id) AS id_type "
        "FROM speaking_evaluations WHERE class_id=? AND evaluation_name=?"
        )));
    query.addBindValue(classId);
    query.addBindValue(QStringLiteral("Seeded Wide Evaluation"));
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value("id").toLongLong(), wideEvaluationId);
    QCOMPARE(query.value("id_type").toString(), QStringLiteral("integer"));
    QVERIFY(!query.next());

    QVERIFY(query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM speaking_eval_data WHERE evaluation_id=?"
        )));
    query.addBindValue(wideEvaluationId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), SpeakingEval::RowCount);

    QVERIFY(query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM speaking_eval_data WHERE evaluation_id=0"
        )));
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);

    const Result<SpeakingEvalRows> updatedRows = service.loadSpeakingEval(
        classId,
        QStringLiteral("Seeded Wide Evaluation")
        );
    QVERIFY(updatedRows);
    QCOMPARE(updatedRows->size(), SpeakingEval::RowCount);
    QCOMPARE((*updatedRows)[0][0], QStringLiteral("Updated Student"));
    QCOMPARE((*updatedRows)[0][1], QStringLiteral("Original Comment"));
    QCOMPARE((*updatedRows)[1][0], QStringLiteral("Second Student"));

    QVERIFY(query.prepare(QStringLiteral(
        "DELETE FROM speaking_eval_data WHERE evaluation_id=?"
        )));
    query.addBindValue(wideEvaluationId);
    QVERIFY(query.exec());
    QCOMPARE(query.numRowsAffected(), SpeakingEval::RowCount);

    QVERIFY(query.prepare(QStringLiteral(
        "DELETE FROM speaking_evaluations WHERE id=?"
        )));
    query.addBindValue(wideEvaluationId);
    QVERIFY(query.exec());
    QCOMPARE(query.numRowsAffected(), 1);

    QVERIFY(query.exec(QStringLiteral(
        "UPDATE sqlite_sequence SET seq=2147483648 "
        "WHERE name='speaking_evaluations'"
        )));
    QCOMPARE(query.numRowsAffected(), 1);

    SpeakingEvalRows newRows = SpeakingEval::emptyRows();
    newRows[0][0] = QStringLiteral("New Wide Student");
    newRows[0][1] = QStringLiteral("New Wide Comment");
    QVERIFY(service.saveSpeakingEval(
        classId,
        QStringLiteral("New Wide Evaluation"),
        newRows
        ));

    QVERIFY(query.prepare(QStringLiteral(
        "SELECT id, typeof(id) AS id_type FROM speaking_evaluations "
        "WHERE class_id=? AND evaluation_name=?"
        )));
    query.addBindValue(classId);
    query.addBindValue(QStringLiteral("New Wide Evaluation"));
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value("id").toLongLong(), wideEvaluationId + 1);
    QCOMPARE(query.value("id_type").toString(), QStringLiteral("integer"));
    QVERIFY(!query.next());

    QVERIFY(query.prepare(QStringLiteral(
        "SELECT evaluation_id, row_index, col_0, col_1 "
        "FROM speaking_eval_data WHERE evaluation_id=? AND row_index=0"
        )));
    query.addBindValue(wideEvaluationId + 1);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value("evaluation_id").toLongLong(), wideEvaluationId + 1);
    QCOMPARE(query.value("row_index").toInt(), 0);
    QCOMPARE(query.value("col_0").toString(), QStringLiteral("New Wide Student"));
    QCOMPARE(query.value("col_1").toString(), QStringLiteral("New Wide Comment"));

    const Result<SpeakingEvalRows> reloadedNewRows = service.loadSpeakingEval(
        classId,
        QStringLiteral("New Wide Evaluation")
        );
    QVERIFY(reloadedNewRows);
    QCOMPARE(reloadedNewRows->size(), SpeakingEval::RowCount);
    QCOMPARE((*reloadedNewRows)[0][0], QStringLiteral("New Wide Student"));
    QCOMPARE((*reloadedNewRows)[0][1], QStringLiteral("New Wide Comment"));
}

void DataServiceLifecycleTests::speakingEvaluationSaveRejectsMalformedStoredRowIndex()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("malformed-speaking-row-index.db"))
        ));

    const Result<int> createdClass =
        service.createClass(QStringLiteral("Malformed Row Index Class"));
    QVERIFY(createdClass);
    const int classId = *createdClass;
    const QString evaluationName = QStringLiteral("Malformed Row Index Evaluation");

    SpeakingEvalRows originalRows = SpeakingEval::emptyRows();
    originalRows[0][0] = QStringLiteral("Original Row Zero");
    QVERIFY(service.saveSpeakingEval(
        classId,
        evaluationName,
        originalRows,
        {{0, 0}}
        ));

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery query(database);
    QVERIFY(query.prepare(QStringLiteral(
        "SELECT id FROM speaking_evaluations "
        "WHERE class_id=? AND evaluation_name=?"
        )));
    query.addBindValue(classId);
    query.addBindValue(evaluationName);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    const qint64 evaluationId = query.value("id").toLongLong();
    QVERIFY(evaluationId > 0);
    QVERIFY(!query.next());

    const QString malformedRowIndex = QStringLiteral("not-a-row-index");
    QVERIFY(query.prepare(QStringLiteral(
        "INSERT INTO speaking_eval_data "
        "(evaluation_id, row_index, col_0) VALUES (?, ?, ?)"
        )));
    query.addBindValue(evaluationId);
    query.addBindValue(malformedRowIndex);
    query.addBindValue(QStringLiteral("Malformed Row Zero Alias"));
    QVERIFY(query.exec());

    QVERIFY(query.prepare(QStringLiteral(
        "SELECT typeof(row_index) AS storage_type "
        "FROM speaking_eval_data WHERE evaluation_id=? AND row_index=?"
        )));
    query.addBindValue(evaluationId);
    query.addBindValue(malformedRowIndex);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value("storage_type").toString(), QStringLiteral("text"));
    QVERIFY(!query.next());

    // A later valid-row insert happens before the repository reads and rejects
    // the malformed persisted index. Its absence after failure proves the
    // transaction rolled that insert back.
    QVERIFY(query.prepare(QStringLiteral(
        "DELETE FROM speaking_eval_data "
        "WHERE evaluation_id=? AND row_index=1"
        )));
    query.addBindValue(evaluationId);
    QVERIFY(query.exec());
    QCOMPARE(query.numRowsAffected(), 1);

    SpeakingEvalRows changedRows = SpeakingEval::emptyRows();
    changedRows[0][0] = QStringLiteral("Changed Row Zero");
    const Status failedSave = service.saveSpeakingEval(
        classId,
        evaluationName,
        changedRows,
        {{0, 0}}
        );
    QVERIFY(!failedSave);
    QVERIFY(failedSave.error().contains(QStringLiteral("invalid row index")));

    QVERIFY(query.prepare(QStringLiteral(
        "SELECT col_0 FROM speaking_eval_data "
        "WHERE evaluation_id=? AND row_index=0"
        )));
    query.addBindValue(evaluationId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value("col_0").toString(), QStringLiteral("Original Row Zero"));
    QVERIFY(!query.next());

    QVERIFY(query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM speaking_eval_data "
        "WHERE evaluation_id=? AND row_index=1"
        )));
    query.addBindValue(evaluationId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
}

void DataServiceLifecycleTests::legacyRepositoryWriteFailuresRollBack()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("legacy-write-rollbacks.db"))
        ).has_value());

    const Result<int> createdClass =
        service.createClass(QStringLiteral("Rollback Class"));
    QVERIFY(createdClass);
    const int classId = *createdClass;

    ClassInfo originalInfo;
    originalInfo.classId = classId;
    originalInfo.notes = QStringLiteral("Original notes");
    originalInfo.classTimes.append({
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    });
    QVERIFY(service.saveClassInfo(originalInfo));

    QSqlDatabase database = service.databaseSession()->database();
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_intensive_time_insert "
        "BEFORE INSERT ON class_intensive_times "
        "WHEN NEW.class_id = %1 "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected class info failure'); "
        "END"
        ).arg(classId)));

    ClassInfo changedInfo = originalInfo;
    changedInfo.notes = QStringLiteral("Changed notes");
    changedInfo.classTimes = {{
        QStringLiteral("Tuesday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:50 AM")
    }};
    changedInfo.intensiveTimes.append({
        QStringLiteral("Wednesday"),
        QStringLiteral("11:00 AM"),
        QStringLiteral("11:50 AM")
    });
    const Status classInfoSaved = service.saveClassInfo(changedInfo);
    QVERIFY(!classInfoSaved);
    QVERIFY(classInfoSaved.error().contains(
        QStringLiteral("Inserting intensive class time")
        ));
    QVERIFY(classInfoSaved.error().contains(
        QStringLiteral("class id %1").arg(classId)
        ));
    const Result<ClassInfo> rolledBackInfo = service.loadClassInfo(classId);
    QVERIFY(rolledBackInfo);
    QCOMPARE(rolledBackInfo->notes, QStringLiteral("Original notes"));
    QCOMPARE(rolledBackInfo->classTimes.size(), 1);
    QCOMPARE(rolledBackInfo->classTimes.first().day, QStringLiteral("Monday"));
    QVERIFY(rolledBackInfo->intensiveTimes.isEmpty());

    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_class_notes_update "
        "BEFORE UPDATE OF notes ON class_info "
        "WHEN NEW.notes = 'Reject notes' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected class notes failure'); "
        "END"
        )));
    const Status notesSaved = service.saveClassNotes(
        classId,
        QStringLiteral("Reject notes"),
        QStringLiteral("Changed activities")
        );
    QVERIFY(!notesSaved);
    QVERIFY(notesSaved.error().contains(QStringLiteral("Saving class notes")));
    QVERIFY(notesSaved.error().contains(
        QStringLiteral("class id %1").arg(classId)
        ));
    QCOMPARE(service.loadClassInfo(classId)->notes,
             QStringLiteral("Original notes"));

    QVERIFY(query.exec(QStringLiteral("DROP TRIGGER reject_intensive_time_insert")));
    Roster originalRoster;
    originalRoster.columns = Roster::BaseColumns;
    originalRoster.rows.append({
        QStringLiteral("Original Student"),
        QStringLiteral("학생")
    });
    QVERIFY(service.saveRoster(classId, originalRoster));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_roster_data_insert "
        "BEFORE INSERT ON roster_data "
        "WHEN NEW.value = 'Reject' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected roster failure'); "
        "END"
        )));

    Roster changedRoster = originalRoster;
    changedRoster.rows[0][0] = QStringLiteral("Reject");
    const Status rosterSaved = service.saveRoster(classId, changedRoster);
    QVERIFY(!rosterSaved);
    QVERIFY(rosterSaved.error().contains(QStringLiteral("Inserting roster data")));
    QVERIFY(rosterSaved.error().contains(
        QStringLiteral("class id %1").arg(classId)
        ));
    QCOMPARE(service.loadRoster(classId)->rows.first().first(),
             QStringLiteral("Original Student"));

    QVERIFY(query.exec(QStringLiteral("DROP TRIGGER reject_roster_data_insert")));
    const Result<int> secondClass =
        service.createClass(QStringLiteral("Second Rollback Class"));
    QVERIFY(secondClass);
    Roster secondOriginalRoster = originalRoster;
    secondOriginalRoster.rows[0][0] = QStringLiteral("Second Original");
    QVERIFY(service.saveRoster(*secondClass, secondOriginalRoster));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_roster_batch_insert "
        "BEFORE INSERT ON roster_data "
        "WHEN NEW.value = 'Batch Reject' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected roster batch failure'); "
        "END"
        )));
    Roster firstBatchRoster = originalRoster;
    firstBatchRoster.rows[0][0] = QStringLiteral("First Changed");
    Roster secondBatchRoster = secondOriginalRoster;
    secondBatchRoster.rows[0][0] = QStringLiteral("Batch Reject");
    const Status rostersSaved = service.saveRosters({
        qMakePair(classId, firstBatchRoster),
        qMakePair(*secondClass, secondBatchRoster)
    });
    QVERIFY(!rostersSaved);
    QCOMPARE(service.loadRoster(classId)->rows.first().first(),
             QStringLiteral("Original Student"));
    QCOMPARE(service.loadRoster(*secondClass)->rows.first().first(),
             QStringLiteral("Second Original"));

    SpeakingEvalRows originalEvaluation = SpeakingEval::emptyRows();
    originalEvaluation[0][0] = QStringLiteral("Original A");
    originalEvaluation[1][0] = QStringLiteral("Original B");
    QVERIFY(service.saveSpeakingEval(
        classId,
        QStringLiteral("Rollback Evaluation"),
        originalEvaluation
        ));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TRIGGER reject_speaking_eval_update "
        "BEFORE UPDATE OF col_0 ON speaking_eval_data "
        "WHEN NEW.row_index = 1 AND NEW.col_0 = 'Reject' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected speaking evaluation failure'); "
        "END"
        )));

    SpeakingEvalRows changedEvaluation = originalEvaluation;
    changedEvaluation[0][0] = QStringLiteral("Changed A");
    changedEvaluation[1][0] = QStringLiteral("Reject");
    const Status evaluationSaved = service.saveSpeakingEval(
        classId,
        QStringLiteral("Rollback Evaluation"),
        changedEvaluation,
        {{0, 0}, {1, 0}}
        );
    QVERIFY(!evaluationSaved);
    QVERIFY(evaluationSaved.error().contains(
        QStringLiteral("Updating speaking evaluation cell")
        ));
    QVERIFY(evaluationSaved.error().contains(
        QStringLiteral("Rollback Evaluation")
        ));
    const Result<SpeakingEvalRows> rolledBackEvaluation = service.loadSpeakingEval(
        classId,
        QStringLiteral("Rollback Evaluation")
        );
    QVERIFY(rolledBackEvaluation);
    QCOMPARE((*rolledBackEvaluation)[0][0], QStringLiteral("Original A"));
    QCOMPARE((*rolledBackEvaluation)[1][0], QStringLiteral("Original B"));

    const Result<QList<ClassConflict>> noConflicts =
        service.getClassTimeConflicts(
            classId,
            {},
            ScheduleType::Regular
            );
    QVERIFY(noConflicts);
    QVERIFY(noConflicts->isEmpty());

    const Result<QList<ClassConflict>> missingClassConflicts =
        service.getClassTimeConflicts(
            classId + 1000,
            {},
            ScheduleType::Regular
            );
    QVERIFY(!missingClassConflicts);
    QVERIFY(missingClassConflicts.error().contains(
        QStringLiteral("no matching record exists")
        ));

    QVERIFY(query.exec(QStringLiteral("DROP TABLE class_times")));
    const Result<QList<ClassConflict>> failedConflictRead =
        service.getClassTimeConflicts(
            classId,
            {},
            ScheduleType::Regular
            );
    QVERIFY(!failedConflictRead);
    QVERIFY(failedConflictRead.error().contains(
        QStringLiteral("Loading class time conflicts")
        ));
    QVERIFY(failedConflictRead.error().contains(
        QStringLiteral("class id %1").arg(classId)
        ));
}

void DataServiceLifecycleTests::databaseSessionOwnsRepositoryLifetime()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DatabaseSession session;
    QVERIFY(!session.isOpen());
    QVERIFY(session.teacherRepository() == nullptr);
    QVERIFY(session.classRepository() == nullptr);

    const QString path =
        directory.filePath(QStringLiteral("session.db"));
    QVERIFY(session.open(path).has_value());
    QVERIFY(session.isOpen());
    QCOMPARE(session.databasePath(), path);
    QVERIFY(session.database().isOpen());
    QVERIFY(session.teacherRepository() != nullptr);
    QVERIFY(session.classRepository() != nullptr);
    QVERIFY(session.rosterRepository() != nullptr);
    QVERIFY(session.speakingEvalRepository() != nullptr);

    session.close();
    QVERIFY(!session.isOpen());
    QVERIFY(session.databasePath().isEmpty());
    QVERIFY(session.teacherRepository() == nullptr);
    QVERIFY(session.classRepository() == nullptr);
    QVERIFY(session.rosterRepository() == nullptr);
    QVERIFY(session.speakingEvalRepository() == nullptr);
}

void DataServiceLifecycleTests::
failedReplacementOpenPreservesSessionAndReleasesCandidate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString databaseA =
        directory.filePath(QStringLiteral("active-profile.tps"));
    const QString invalidDatabase =
        directory.filePath(QStringLiteral("invalid-profile.tps"));
    const QString databaseB =
        directory.filePath(QStringLiteral("replacement-profile.tps"));

    DataService service;
    QVERIFY(service.openDatabase(databaseA));
    DatabaseSession* const session = service.databaseSession();
    QVERIFY(session != nullptr);
    SettingsRepository* const settingsRepository =
        session->settingsRepository();
    TeacherRepository* const teacherRepository =
        session->teacherRepository();
    ClassRepository* const classRepository =
        session->classRepository();

    QVERIFY(service.saveSetting(
        QStringLiteral("replacement/sentinel"),
        QStringLiteral("profile A")
        ));
    Teacher teacherA;
    teacherA.teacherEn = QStringLiteral("Profile A Teacher");
    const Result<int> teacherAId = service.createTeacher(teacherA);
    QVERIFY(teacherAId);

    const QString connectionA = session->database().connectionName();
    const QStringList connectionsBeforeFailure =
        QSqlDatabase::connectionNames();
    QFile invalidFile(invalidDatabase);
    QVERIFY(invalidFile.open(QIODevice::WriteOnly));
    QCOMPARE(
        invalidFile.write(QByteArrayLiteral("not a SQLite database")),
        qint64(QByteArrayLiteral("not a SQLite database").size())
        );
    invalidFile.close();

    const Status failedReplacement = service.openDatabase(invalidDatabase);
    QVERIFY(!failedReplacement);
    QVERIFY(failedReplacement.error().contains(
        QStringLiteral("Unable to initialize Teacher Profile")
        ));
    QCOMPARE(service.databaseSession(), session);
    QVERIFY(service.isOpen());
    QCOMPARE(service.currentDatabasePath(), QFileInfo(databaseA).absoluteFilePath());
    QCOMPARE(session->settingsRepository(), settingsRepository);
    QCOMPARE(session->teacherRepository(), teacherRepository);
    QCOMPARE(session->classRepository(), classRepository);
    QCOMPARE(session->database().connectionName(), connectionA);
    QCOMPARE(QSqlDatabase::connectionNames(), connectionsBeforeFailure);

    const Result<QVariant> retainedSetting = service.loadSetting(
        QStringLiteral("replacement/sentinel")
        );
    const Result<Teacher> retainedTeacher = service.getTeacher(*teacherAId);
    QVERIFY(retainedSetting);
    QVERIFY(retainedTeacher);
    QCOMPARE(retainedSetting->toString(), QStringLiteral("profile A"));
    QCOMPARE(retainedTeacher->teacherEn, QStringLiteral("Profile A Teacher"));

    DataService seedReplacement;
    QVERIFY(seedReplacement.openDatabase(databaseB));
    QVERIFY(seedReplacement.saveSetting(
        QStringLiteral("replacement/sentinel"),
        QStringLiteral("profile B")
        ));
    Teacher teacherB;
    teacherB.teacherEn = QStringLiteral("Profile B Teacher");
    QVERIFY(seedReplacement.createTeacher(teacherB));
    seedReplacement.closeDatabase();

    SettingsService featureSettings(session, &service);
    TeacherService featureTeachers(session, &service);
    QVERIFY(service.openDatabase(databaseB));
    QCOMPARE(service.databaseSession(), session);
    QVERIFY(service.isOpen());
    QCOMPARE(service.currentDatabasePath(), QFileInfo(databaseB).absoluteFilePath());

    const QString connectionB = session->database().connectionName();
    QVERIFY(connectionB != connectionA);
    QVERIFY(QSqlDatabase::contains(connectionB));
    QVERIFY(!QSqlDatabase::contains(connectionA));
    QCOMPARE(QSqlDatabase::connectionNames().size(), connectionsBeforeFailure.size());

    const Result<QVariant> replacementSetting = featureSettings.load(
        QStringLiteral("replacement/sentinel")
        );
    const Result<QList<Teacher>> replacementTeachers = featureTeachers.teachers();
    QVERIFY(replacementSetting);
    QVERIFY(replacementTeachers);
    QCOMPARE(replacementSetting->toString(), QStringLiteral("profile B"));
    QCOMPARE(replacementTeachers->size(), 1);
    QCOMPARE(replacementTeachers->first().teacherEn, QStringLiteral("Profile B Teacher"));

    QVERIFY(service.openDatabase(databaseB));
    const QString reopenedConnection = session->database().connectionName();
    QVERIFY(reopenedConnection != connectionB);
    QVERIFY(QSqlDatabase::contains(reopenedConnection));
    QVERIFY(!QSqlDatabase::contains(connectionB));
    QCOMPARE(QSqlDatabase::connectionNames().size(), connectionsBeforeFailure.size());
    QCOMPARE(
        featureSettings.load(QStringLiteral("replacement/sentinel"))->toString(),
        QStringLiteral("profile B")
        );
}

void DataServiceLifecycleTests::featureServicesExposeNarrowOperations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService dataService;
    SettingsService settings(dataService.databaseSession(), &dataService);
    TeacherService teachers(dataService.databaseSession(), &dataService);
    ClassService classes(dataService.databaseSession(), &dataService);
    ScheduleService schedule(dataService.databaseSession(), &dataService);
    CalendarService calendar(dataService.databaseSession(), &dataService);
    RosterService rosters(dataService.databaseSession(), &dataService);
    SpeakingEvaluationService evaluations(
        dataService.databaseSession(), &dataService);

    QVERIFY(!settings.isAvailable());
    QVERIFY(!teachers.isAvailable());
    QVERIFY(!classes.isAvailable());

    const QString path =
        directory.filePath(QStringLiteral("feature-services.db"));
    QVERIFY(dataService.openDatabase(path).has_value());

    QVERIFY(settings.isAvailable());
    QVERIFY(teachers.isAvailable());
    QVERIFY(classes.isAvailable());
    QVERIFY(schedule.isAvailable());
    QVERIFY(calendar.isAvailable());
    QVERIFY(rosters.isAvailable());
    QVERIFY(evaluations.isAvailable());

    QVERIFY(settings.save(
        QStringLiteral("feature-services/value"),
        QStringLiteral("saved")
        ).has_value());
    QCOMPARE(
        settings.load(QStringLiteral("feature-services/value"))->toString(),
        QStringLiteral("saved")
        );

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Narrow Teacher");
    const Result<int> createdTeacher = teachers.create(teacher);
    const Result<int> createdClass =
        classes.create(QStringLiteral("Narrow Class"));
    QVERIFY(createdTeacher);
    QVERIFY(createdClass);
    const int teacherId = *createdTeacher;
    const int classId = *createdClass;
    const Result<QList<ClassConflict>> conflicts = classes.conflicts(
        classId,
        {},
        ScheduleType::Regular
        );
    QVERIFY(conflicts);
    QVERIFY(conflicts->isEmpty());
    QVERIFY(teacherId > 0);
    QVERIFY(classId > 0);
    const Result<Teacher> loadedTeacher = teachers.teacher(teacherId);
    const Result<Classroom> loadedClass = classes.classroom(classId);
    QVERIFY(loadedTeacher);
    QVERIFY(loadedClass);
    QCOMPARE(loadedTeacher->teacherEn,
             QStringLiteral("Narrow Teacher"));
    QCOMPARE(loadedClass->name,
             QStringLiteral("Narrow Class"));

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows.append(
        {QStringLiteral("Student"), QStringLiteral("학생")}
        );
    QVERIFY(rosters.saveRoster(classId, roster).has_value());
    QCOMPARE(rosters.studentCount(classId).value_or(-1), 1);

    dataService.closeDatabase();
    QVERIFY(!teachers.isAvailable());
    QVERIFY(!classes.isAvailable());
}

void DataServiceLifecycleTests::
settingsServiceDoesNotFallBackFromClosedSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService legacyDataService;
    QVERIFY(legacyDataService.openDatabase(
        directory.filePath(QStringLiteral("legacy-settings.db"))
        ).has_value());

    const QString existingKey = QStringLiteral("settings-isolation/existing");
    const QString singleWriteKey = QStringLiteral("settings-isolation/single");
    const QString batchWriteKey = QStringLiteral("settings-isolation/batch");
    const QString originalValue = QStringLiteral("legacy value");
    QVERIFY(legacyDataService.saveSetting(existingKey, originalValue));

    DatabaseSession closedSession;
    QVERIFY(!closedSession.isOpen());
    QVERIFY(closedSession.settingsRepository() == nullptr);

    SettingsService sessionBound(&closedSession, &legacyDataService);
    SettingsService legacyOnly(&legacyDataService);
    QVERIFY(!sessionBound.isAvailable());
    QVERIFY(legacyOnly.isAvailable());

    const Result<QVariant> unavailableLoad = sessionBound.load(existingKey);
    QVERIFY(!unavailableLoad);
    QCOMPARE(
        sessionBound.loadOrDefault(
            existingKey,
            QStringLiteral("session default")
            ).toString(),
        QStringLiteral("session default")
        );
    QVERIFY(!sessionBound.save(existingKey, QStringLiteral("rejected single")));

    QVariantMap sessionBatch;
    sessionBatch.insert(existingKey, QStringLiteral("rejected batch"));
    sessionBatch.insert(batchWriteKey, QStringLiteral("must not be written"));
    QVERIFY(!sessionBound.saveAll(sessionBatch));

    const Result<QVariant> retainedLegacyValue =
        legacyDataService.loadSetting(existingKey);
    QVERIFY(retainedLegacyValue);
    QCOMPARE(retainedLegacyValue->toString(), originalValue);
    const Result<QVariant> untouchedSingleKey =
        legacyDataService.loadSetting(singleWriteKey);
    const Result<QVariant> untouchedBatchKey =
        legacyDataService.loadSetting(batchWriteKey);
    QVERIFY(untouchedSingleKey);
    QVERIFY(untouchedBatchKey);
    QVERIFY(!untouchedSingleKey->isValid());
    QVERIFY(!untouchedBatchKey->isValid());

    QCOMPARE(legacyOnly.load(existingKey)->toString(), originalValue);
    QVERIFY(legacyOnly.save(singleWriteKey, QStringLiteral("legacy single")));
    QVERIFY(legacyOnly.saveAll(
        {{batchWriteKey, QStringLiteral("legacy batch")}}
        ));
    QCOMPARE(
        legacyOnly.load(singleWriteKey)->toString(),
        QStringLiteral("legacy single")
        );
    QCOMPARE(
        legacyOnly.load(batchWriteKey)->toString(),
        QStringLiteral("legacy batch")
    );
}

void DataServiceLifecycleTests::
teacherServiceDoesNotFallBackFromClosedSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService legacyDataService;
    QVERIFY(legacyDataService.openDatabase(
        directory.filePath(QStringLiteral("legacy-teachers.db"))
        ).has_value());

    Teacher seededTeacher;
    seededTeacher.teacherEn = QStringLiteral("Legacy Teacher");
    const Result<int> seededTeacherId =
        legacyDataService.createTeacher(seededTeacher);
    QVERIFY(seededTeacherId);

    DatabaseSession closedSession;
    QVERIFY(!closedSession.isOpen());
    QVERIFY(closedSession.teacherRepository() == nullptr);

    TeacherService sessionBound(&closedSession, &legacyDataService);
    TeacherService legacyOnly(&legacyDataService);
    QVERIFY(!sessionBound.isAvailable());
    QVERIFY(legacyOnly.isAvailable());

    QVERIFY(!sessionBound.teachers());

    Teacher rejectedUpdate = seededTeacher;
    rejectedUpdate.id = *seededTeacherId;
    rejectedUpdate.teacherEn = QStringLiteral("Wrong Update");
    QVERIFY(!sessionBound.update(rejectedUpdate));

    const Result<Teacher> retainedLegacyTeacher =
        legacyDataService.getTeacher(*seededTeacherId);
    QVERIFY(retainedLegacyTeacher);
    QCOMPARE(
        retainedLegacyTeacher->teacherEn,
        QStringLiteral("Legacy Teacher")
        );

    const Result<QList<Teacher>> legacyTeachers = legacyOnly.teachers();
    QVERIFY(legacyTeachers);
    QCOMPARE(legacyTeachers->size(), 1);
    QCOMPARE(legacyTeachers->first().teacherEn,
             QStringLiteral("Legacy Teacher"));

    Teacher legacyUpdate = *retainedLegacyTeacher;
    legacyUpdate.teacherEn = QStringLiteral("Updated Teacher");
    QVERIFY(legacyOnly.update(legacyUpdate));

    const Result<Teacher> updatedLegacyTeacher =
        legacyOnly.teacher(*seededTeacherId);
    QVERIFY(updatedLegacyTeacher);
    QCOMPARE(
        updatedLegacyTeacher->teacherEn,
        QStringLiteral("Updated Teacher")
        );
}

void DataServiceLifecycleTests::
calendarServiceDoesNotFallBackFromClosedSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService legacyDataService;
    QVERIFY(legacyDataService.openDatabase(
        directory.filePath(QStringLiteral("legacy-calendar.db"))
        ).has_value());

    const QDate eventDate(2026, 10, 15);
    const QString repeatSeriesId =
        QStringLiteral("f112-calendar-isolation-series");
    CalendarEvent seededEvent;
    seededEvent.title = QStringLiteral("F112 Calendar Isolation Event");
    seededEvent.eventType = QStringLiteral("Workshop");
    seededEvent.repeatSeriesId = repeatSeriesId;
    seededEvent.startDate = eventDate;
    seededEvent.startTime = QTime(9, 0);
    seededEvent.endDate = eventDate;
    seededEvent.endTime = QTime(10, 0);
    const Result<int> createdEvent =
        legacyDataService.saveCalendarEvent(seededEvent);
    QVERIFY(createdEvent);

    DatabaseSession closedSession;
    QVERIFY(!closedSession.isOpen());
    QVERIFY(closedSession.calendarEventRepository() == nullptr);

    CalendarService sessionBound(&closedSession, &legacyDataService);
    CalendarService legacyOnly(&legacyDataService);
    QVERIFY(!sessionBound.isAvailable());
    QVERIFY(legacyOnly.isAvailable());

    QVERIFY(!sessionBound.eventsForDate(eventDate));
    QVERIFY(!sessionBound.eventsInRange(eventDate, eventDate));
    QVERIFY(!sessionBound.eventDateIntervalsInRange(eventDate, eventDate));
    QVERIFY(!sessionBound.upcomingEvents(eventDate, 10));
    QVERIFY(!sessionBound.event(*createdEvent));
    QVERIFY(!sessionBound.repeatSeriesFromDate(repeatSeriesId, eventDate));

    CalendarEvent rejectedMutationEvent = seededEvent;
    rejectedMutationEvent.title =
        QStringLiteral("F114 Rejected Calendar Mutation Event");
    rejectedMutationEvent.repeatSeriesId =
        QStringLiteral("f114-rejected-calendar-mutation-series");
    rejectedMutationEvent.startTime = QTime(11, 0);
    rejectedMutationEvent.endTime = QTime(12, 0);

    const auto verifySeededLegacyCalendar = [&]()
    {
        const Result<QList<CalendarEvent>> seededLegacyEvents =
            legacyOnly.eventsForDate(eventDate);
        QVERIFY(seededLegacyEvents);
        QCOMPARE(seededLegacyEvents->size(), 1);
        QCOMPARE(
            seededLegacyEvents->first().title,
            QStringLiteral("F112 Calendar Isolation Event")
            );
    };

    QVERIFY(!sessionBound.saveEvents({rejectedMutationEvent}));
    verifySeededLegacyCalendar();
    QVERIFY(!sessionBound.deleteEvent(*createdEvent));
    verifySeededLegacyCalendar();
    QVERIFY(!sessionBound.deleteRepeatSeriesFromDate(
        repeatSeriesId,
        eventDate
        ));
    verifySeededLegacyCalendar();
    QVERIFY(!sessionBound.deleteAllEvents());
    verifySeededLegacyCalendar();

    const Result<QList<CalendarEvent>> retainedAfterRejectedMutations =
        legacyOnly.eventsForDate(eventDate);
    QVERIFY(retainedAfterRejectedMutations);
    QCOMPARE(retainedAfterRejectedMutations->size(), 1);
    QCOMPARE(
        retainedAfterRejectedMutations->first().title,
        QStringLiteral("F112 Calendar Isolation Event")
        );

    const Result<QList<CalendarEvent>> legacyEventsForDate =
        legacyOnly.eventsForDate(eventDate);
    QVERIFY(legacyEventsForDate);
    QCOMPARE(legacyEventsForDate->size(), 1);
    QCOMPARE(
        legacyEventsForDate->first().title,
        QStringLiteral("F112 Calendar Isolation Event")
        );

    const Result<QList<CalendarEvent>> legacyEventsInRange =
        legacyOnly.eventsInRange(eventDate, eventDate);
    QVERIFY(legacyEventsInRange);
    QCOMPARE(legacyEventsInRange->size(), 1);
    QCOMPARE(
        legacyEventsInRange->first().title,
        QStringLiteral("F112 Calendar Isolation Event")
        );

    const Result<QList<CalendarEventDateInterval>> legacyIntervals =
        legacyOnly.eventDateIntervalsInRange(eventDate, eventDate);
    QVERIFY(legacyIntervals);
    QCOMPARE(legacyIntervals->size(), 1);
    QCOMPARE(legacyIntervals->first().eventType, QStringLiteral("Workshop"));
    QCOMPARE(legacyIntervals->first().startDate, eventDate);
    QCOMPARE(legacyIntervals->first().endDate, eventDate);

    const Result<QList<CalendarEvent>> legacyUpcoming =
        legacyOnly.upcomingEvents(eventDate, 10);
    QVERIFY(legacyUpcoming);
    QCOMPARE(legacyUpcoming->size(), 1);
    QCOMPARE(
        legacyUpcoming->first().title,
        QStringLiteral("F112 Calendar Isolation Event")
        );

    const Result<CalendarEvent> legacyEvent = legacyOnly.event(*createdEvent);
    QVERIFY(legacyEvent);
    QCOMPARE(legacyEvent->title, QStringLiteral("F112 Calendar Isolation Event"));

    const Result<QList<CalendarEvent>> legacyRepeatSeries =
        legacyOnly.repeatSeriesFromDate(repeatSeriesId, eventDate);
    QVERIFY(legacyRepeatSeries);
    QCOMPARE(legacyRepeatSeries->size(), 1);
    QCOMPARE(
        legacyRepeatSeries->first().repeatSeriesId,
        repeatSeriesId
        );
    QCOMPARE(
        legacyRepeatSeries->first().title,
        QStringLiteral("F112 Calendar Isolation Event")
        );

    CalendarEvent legacyMutationEvent = seededEvent;
    legacyMutationEvent.title =
        QStringLiteral("F114 Legacy Calendar Mutation");
    legacyMutationEvent.repeatSeriesId =
        QStringLiteral("f114-legacy-calendar-mutation-series");
    legacyMutationEvent.startTime = QTime(11, 0);
    legacyMutationEvent.endTime = QTime(12, 0);
    const Result<QList<qint64>> legacySavedEvents =
        legacyOnly.saveEvents({legacyMutationEvent});
    QVERIFY(legacySavedEvents);
    QCOMPARE(legacySavedEvents->size(), 1);
    QVERIFY(legacyOnly.deleteEvent(legacySavedEvents->first()));
    QVERIFY(legacyOnly.deleteRepeatSeriesFromDate(repeatSeriesId, eventDate));
    QVERIFY(legacyOnly.deleteAllEvents());

    const Result<QList<CalendarEvent>> emptyLegacyCalendar =
        legacyOnly.eventsForDate(eventDate);
    QVERIFY(emptyLegacyCalendar);
    QVERIFY(emptyLegacyCalendar->isEmpty());
}

void DataServiceLifecycleTests::
classNotesServiceDoesNotFallBackFromClosedSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService legacyDataService;
    QVERIFY(legacyDataService.openDatabase(
        directory.filePath(QStringLiteral("legacy-class-notes.db"))
        ).has_value());
    const Result<int> createdClass = legacyDataService.createClass(
        QStringLiteral("F115 Class Notes Isolation")
        );
    QVERIFY(createdClass);

    ClassInfo originalInfo;
    originalInfo.classId = *createdClass;
    originalInfo.notes = QStringLiteral("Original notes");
    originalInfo.timeFillerActivities = QStringLiteral("Original activities");
    QVERIFY(legacyDataService.saveClassInfo(originalInfo));

    DatabaseSession closedSession;
    QVERIFY(!closedSession.isOpen());
    QVERIFY(closedSession.classInfoRepository() == nullptr);

    ClassService sessionBound(&closedSession, &legacyDataService);
    ClassService legacyOnly(&legacyDataService);

    QVERIFY(!sessionBound.saveClassNotes(
        *createdClass,
        QStringLiteral("Rejected notes"),
        QStringLiteral("Rejected activities")
        ));

    const Result<ClassInfo> unchangedInfo =
        legacyDataService.loadClassInfo(*createdClass);
    QVERIFY(unchangedInfo);
    QCOMPARE(unchangedInfo->notes, QStringLiteral("Original notes"));
    QCOMPARE(
        unchangedInfo->timeFillerActivities,
        QStringLiteral("Original activities")
        );

    QVERIFY(legacyOnly.saveClassNotes(
        *createdClass,
        QStringLiteral("Legacy notes"),
        QStringLiteral("Legacy activities")
        ));
    const Result<ClassInfo> legacyUpdatedInfo =
        legacyDataService.loadClassInfo(*createdClass);
    QVERIFY(legacyUpdatedInfo);
    QCOMPARE(legacyUpdatedInfo->notes, QStringLiteral("Legacy notes"));
    QCOMPARE(
        legacyUpdatedInfo->timeFillerActivities,
        QStringLiteral("Legacy activities")
        );
}

void DataServiceLifecycleTests::
subPrepOutputReadsDoNotFallBackFromClosedSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService legacyDataService;
    QVERIFY(legacyDataService.openDatabase(
        directory.filePath(QStringLiteral("legacy-sub-prep-output.db"))
        ).has_value());
    const QString seedPrefix = QStringLiteral("F113 Sub Prep");
    const DatabaseIds seeded = populateDatabase(
        legacyDataService,
        seedPrefix
        );
    QVERIFY(seeded.teacherId > 0);
    QVERIFY(seeded.classId > 0);
    QVERIFY(seeded.rosterSaved);

    DatabaseSession closedSession;
    QVERIFY(!closedSession.isOpen());
    QVERIFY(closedSession.teacherRepository() == nullptr);
    QVERIFY(closedSession.classRepository() == nullptr);
    QVERIFY(closedSession.classInfoRepository() == nullptr);
    QVERIFY(closedSession.rosterRepository() == nullptr);

    TeacherService sessionBoundTeachers(
        &closedSession,
        &legacyDataService
        );
    ClassService sessionBoundClasses(
        &closedSession,
        &legacyDataService
        );
    RosterService sessionBoundRosters(
        &closedSession,
        &legacyDataService
        );
    TeacherService legacyOnlyTeachers(&legacyDataService);
    ClassService legacyOnlyClasses(&legacyDataService);
    RosterService legacyOnlyRosters(&legacyDataService);

    QVERIFY(!sessionBoundTeachers.isAvailable());
    QVERIFY(!sessionBoundClasses.isAvailable());
    QVERIFY(!sessionBoundRosters.isAvailable());
    QVERIFY(legacyOnlyTeachers.isAvailable());
    QVERIFY(legacyOnlyClasses.isAvailable());
    QVERIFY(legacyOnlyRosters.isAvailable());

    QVERIFY(!sessionBoundTeachers.teacher(seeded.teacherId));
    QVERIFY(!sessionBoundClasses.classroom(seeded.classId));
    QVERIFY(!sessionBoundClasses.classInfo(seeded.classId));
    QVERIFY(!sessionBoundRosters.studentCount(seeded.classId));
    QVERIFY(!sessionBoundRosters.rosterForOutput(
        seeded.classId,
        Roster::BaseColumns,
        10,
        60,
        4096
        ));

    const Result<Teacher> legacyTeacher =
        legacyOnlyTeachers.teacher(seeded.teacherId);
    QVERIFY(legacyTeacher);
    QCOMPARE(
        legacyTeacher->teacherEn,
        seedPrefix + QStringLiteral(" Teacher")
        );

    const Result<Classroom> legacyClass =
        legacyOnlyClasses.classroom(seeded.classId);
    QVERIFY(legacyClass);
    QCOMPARE(
        legacyClass->name,
        seedPrefix + QStringLiteral(" Class")
        );

    const Result<ClassInfo> legacyClassInfo =
        legacyOnlyClasses.classInfo(seeded.classId);
    QVERIFY(legacyClassInfo);
    QCOMPARE(
        legacyClassInfo->classGrade,
        seedPrefix + QStringLiteral(" Grade")
        );

    const Result<int> legacyStudentCount =
        legacyOnlyRosters.studentCount(seeded.classId);
    QVERIFY(legacyStudentCount);
    QCOMPARE(*legacyStudentCount, 1);

    const Result<Roster> legacyRosterOutput =
        legacyOnlyRosters.rosterForOutput(
            seeded.classId,
            Roster::BaseColumns,
            10,
            60,
            4096
            );
    QVERIFY(legacyRosterOutput);
    QCOMPARE(legacyRosterOutput->rows.size(), 1);
    QCOMPARE(
        legacyRosterOutput->rows.first().first(),
        seedPrefix + QStringLiteral(" Student")
        );
}

void DataServiceLifecycleTests::closeAndSwitchReleaseEveryRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString databaseA =
        directory.filePath(QStringLiteral("database-a.db"));
    const QString databaseB =
        directory.filePath(QStringLiteral("database-b.db"));

    DataService service;
    QVERIFY(service.openDatabase(databaseA).has_value());
    const DatabaseIds idsA =
        populateDatabase(service, QStringLiteral("Database A"));
    verifyDatabase(service, QStringLiteral("Database A"), idsA);

    QVERIFY(service.openDatabase(databaseB).has_value());
    const Result<QList<Teacher>> emptyTeachers = service.getAllTeachers();
    const Result<QList<Classroom>> emptyClasses = service.getClasses();
    QVERIFY(emptyTeachers);
    QVERIFY(emptyTeachers->isEmpty());
    QVERIFY(emptyClasses);
    QVERIFY(emptyClasses->isEmpty());
    const Result<QList<IntensiveSlotState>> emptyIntensiveSlotStates =
        service.loadIntensiveSlotStates();
    QVERIFY(emptyIntensiveSlotStates);
    QVERIFY(emptyIntensiveSlotStates->isEmpty());
    const Result<QList<TestingBlock>> emptyTestingBlocks =
        service.loadTestingBlocks();
    QVERIFY(emptyTestingBlocks);
    QVERIFY(emptyTestingBlocks->isEmpty());
    QVERIFY(
        service.loadCalendarEventsForDate(
            QDate(2026, 7, 17)
            )->isEmpty()
        );
    const Result<QList<CampusRecord>> emptyCampuses =
        service.getAllCampuses();
    QVERIFY(emptyCampuses);
    QVERIFY(emptyCampuses->isEmpty());
    const Result<QVariant> missingSetting =
        service.loadSetting(QStringLiteral("lifecycle/value"));
    QVERIFY(missingSetting);
    QCOMPARE(missingSetting->toString(), QString());

    const DatabaseIds idsB =
        populateDatabase(service, QStringLiteral("Database B"));
    QCOMPARE(idsB.teacherId, idsA.teacherId);
    QCOMPARE(idsB.classId, idsA.classId);
    verifyDatabase(service, QStringLiteral("Database B"), idsB);

    QVERIFY(service.openDatabase(databaseA).has_value());
    verifyDatabase(service, QStringLiteral("Database A"), idsA);

    service.closeDatabase();
    QVERIFY(!service.isOpen());
    QVERIFY(service.currentDatabasePath().isEmpty());
    QVERIFY(!service.getAllTeachers());
    QVERIFY(!service.getClasses());
    QVERIFY(!service.loadIntensiveSlotStates());
    QVERIFY(!service.getNativeEnglishTeachers());
    QVERIFY(!service.getGsTeamMembers());
    QVERIFY(!service.loadTestingBlocks());
    QVERIFY(!service.getAllCampuses());
    QVERIFY(!service.loadRoster(idsA.classId));
    QVERIFY(!service.loadSpeakingEval(
        idsA.classId,
        QStringLiteral("First Semester")
        ));
}

void DataServiceLifecycleTests
    ::existingTeacherSchemaIsRejectedWithoutPersonalDetailMigration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path =
        directory.filePath(QStringLiteral("legacy.db"));
    const QString connectionName =
        QStringLiteral("legacy-teacher-schema");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(path);
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(R"(
            CREATE TABLE teachers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                teacher_kr TEXT,
                teacher_en TEXT,
                room_number TEXT,
                wifi_name TEXT,
                wifi_password TEXT,
                internet_type TEXT DEFAULT 'WiFi',
                zoom_id TEXT,
                zoom_password TEXT,
                projection_type TEXT DEFAULT 'HDMI',
                notes TEXT
            )
        )"));
        QVERIFY(query.exec(R"(
            INSERT INTO teachers (teacher_en)
            VALUES ('Legacy Teacher')
        )"));

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    QFile beforeFile(path);
    QVERIFY(beforeFile.open(QIODevice::ReadOnly));
    const QByteArray contentsBefore = beforeFile.readAll();
    beforeFile.close();
    DataService service;
    const Status opened = service.openDatabase(path);
    QVERIFY(!opened);
    QVERIFY(opened.error().contains(QStringLiteral("unsupported schema version 0")));
    QVERIFY(!service.isOpen());
    QFile afterFile(path);
    QVERIFY(afterFile.open(QIODevice::ReadOnly));
    QCOMPARE(afterFile.readAll(), contentsBefore);
    QVERIFY(!QFileInfo::exists(path + QStringLiteral(".pre-schema-v4-backup")));
}

void DataServiceLifecycleTests
    ::existingTestingSchemaIsRejectedWithoutAssignmentMigration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path =
        directory.filePath(QStringLiteral("legacy-testing.db"));
    const QString connectionName =
        QStringLiteral("legacy-testing-schema");

    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(path);
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(R"(
            CREATE TABLE schedule_testing_blocks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                day TEXT NOT NULL,
                start_time TEXT NOT NULL,
                room TEXT NOT NULL DEFAULT '',
                UNIQUE(day, start_time)
            )
        )"));
        QVERIFY(query.exec(R"(
            INSERT INTO schedule_testing_blocks (
                day,
                start_time,
                room
            )
            VALUES ('Tuesday', '15:00', 'Legacy Room')
        )"));

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    QFile beforeFile(path);
    QVERIFY(beforeFile.open(QIODevice::ReadOnly));
    const QByteArray contentsBefore = beforeFile.readAll();
    beforeFile.close();
    DataService service;
    const Status opened = service.openDatabase(path);
    QVERIFY(!opened);
    QVERIFY(opened.error().contains(QStringLiteral("unsupported schema version 0")));
    QVERIFY(!service.isOpen());

    {
        const QString verificationConnection =
            connectionName + QStringLiteral("-verification");
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                verificationConnection
                );
        database.setDatabaseName(path);
        QVERIFY(database.open());

        QSqlQuery columnQuery(database);
        QVERIFY(columnQuery.exec(
            QStringLiteral(
                "PRAGMA table_info(schedule_testing_blocks)"
                )
            ));
        bool foundClassId = false;
        while (columnQuery.next())
        {
            foundClassId =
                foundClassId
                || columnQuery.value(1).toString()
                    == QStringLiteral("class_id");
        }
        QVERIFY(!foundClassId);
        database.close();
    }
    QSqlDatabase::removeDatabase(
        connectionName + QStringLiteral("-verification")
        );

    QFile afterFile(path);
    QVERIFY(afterFile.open(QIODevice::ReadOnly));
    QCOMPARE(afterFile.readAll(), contentsBefore);
    QVERIFY(!QFileInfo::exists(path + QStringLiteral(".pre-schema-v4-backup")));
}

QTEST_MAIN(DataServiceLifecycleTests)

#include "data_service_lifecycle_tests.moc"
