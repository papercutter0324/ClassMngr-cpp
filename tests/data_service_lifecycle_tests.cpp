#include "data/data_service.h"
#include "data/database/database_schema_manager.h"
#include "data/database/database_session.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "features/my_info/data/personal_details_repository.h"

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
struct DatabaseIds
{
    int teacherId = -1;
    int classId = -1;
    int eventId = -1;
    int campusId = -1;
    bool classInfoSaved = false;
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
    teacher.teacherKr = QStringLiteral("홍길동");
    teacher.preferredRomanization = prefix + QStringLiteral(" Roman");
    teacher.preferredName = prefix + QStringLiteral(" Roman");
    teacher.roomNumber = prefix + QStringLiteral(" Room");
    teacher.birthday = QStringLiteral("02-29");
    teacher.phoneNumber = QStringLiteral("010 1234 5678");
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
    classInfo.classGrade = QStringLiteral("E4");
    classInfo.classLevel = QStringLiteral("Theseus");
    classInfo.notes = prefix + QStringLiteral(" Class Notes");
    classInfo.classTimes.append(
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
        );
    ids.classInfoSaved =
        service.saveClassInfo(classInfo).has_value();

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
    QVERIFY(ids.classInfoSaved);
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
    QCOMPARE(teacher->teacherKr, QStringLiteral("홍길동"));
    QCOMPARE(
        teacher->preferredRomanization,
        prefix + QStringLiteral(" Roman")
        );
    QCOMPARE(
        teacher->preferredName,
        prefix + QStringLiteral(" Roman")
        );
    QCOMPARE(teacher->roomNumber, prefix + QStringLiteral(" Room"));
    QCOMPARE(teacher->birthday, QStringLiteral("02-29"));
    QCOMPARE(teacher->phoneNumber, QStringLiteral("010-1234-5678"));
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
        prefix + QStringLiteral(" Roman")
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
    void memoryDatabaseIsCompatibilityOnly();
    void applicationServicesOwnDatabaseFileOperations();
    void featureServicesExposeNarrowOperations();
    void closeAndSwitchReleaseEveryRepository();
    void fileBackedSessionUsesEngineSchemaPipeline();
    void schemaFailureClosesDatabaseSession();
    void classDeleteFailureRollsBackAllChanges();
    void teacherDeleteFailureRollsBackClassAssignments();
    void repositoryWriteFailuresAreObservable();
    void coreLookupReadFailuresAreObservable();
    void compoundAndCollectionReadFailuresAreObservable();
    void legacyRepositoryWriteFailuresRollBack();
    void personalDetailsRepositoryUsesEngineBoundary();
    void existingTeacherSchemaGainsPersonalDetailColumns();
    void existingTestingSchemaGainsClassAssignmentColumn();
};

void DataServiceLifecycleTests::personalDetailsRepositoryUsesEngineBoundary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService dataService;
    const QString path = directory.filePath(
        QStringLiteral("personal-details.db")
        );
    QVERIFY(dataService.openDatabase(path).has_value());

    SettingsService settings(
        dataService.databaseSession()
        );
    PersonalDetailsRepository repository(&settings);

    PersonalDetails expected;
    expected.name = QStringLiteral("홍길동 🧑‍🏫");
    expected.campus = QStringLiteral("서울 캠퍼스");
    expected.zoomLoginId = QStringLiteral("teacher@example.test");
    expected.zoomPassword = QStringLiteral("비밀번호");
    expected.zoomNotAvailable = false;
    expected.signatureMode = SignatureMode::Type;
    expected.typedSignatureText = QStringLiteral("서명 이름");
    expected.typedSignatureFont = 3;

    QVERIFY(repository.save(expected));

    const PersonalDetails loaded = repository.load();
    QCOMPARE(loaded.name, expected.name);
    QCOMPARE(loaded.campus, expected.campus);
    QCOMPARE(loaded.zoomLoginId, expected.zoomLoginId);
    QCOMPARE(loaded.zoomPassword, expected.zoomPassword);
    QCOMPARE(loaded.zoomNotAvailable, expected.zoomNotAvailable);
    QCOMPARE(loaded.signatureMode, expected.signatureMode);
    QCOMPARE(loaded.typedSignatureText, expected.typedSignatureText);
    QCOMPARE(loaded.typedSignatureFont, expected.typedSignatureFont);

    QVERIFY(repository.saveCampus(QStringLiteral("부산 캠퍼스")));
    QCOMPARE(repository.load().campus, QStringLiteral("부산 캠퍼스"));

    QSqlQuery query(dataService.databaseSession()->compatibilityDatabase());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT value FROM app_settings WHERE key='myInfo/name'"
        )));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), expected.name);
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
    DataService* compatibilityAdapter = services.dataService();
    QCOMPARE(
        services.settingsService()->databaseSession(),
        compatibilityAdapter->databaseSession()
        );
    QVERIFY(services.openDatabase(sourcePath).has_value());
    QVERIFY(compatibilityAdapter->saveSetting(
        QStringLiteral("application-services/compatibility"),
        QStringLiteral("shared-session")
        ).has_value());
    QVERIFY(services.settingsService()->save(
        QStringLiteral("application-services/value"),
        QStringLiteral("saved")
        ).has_value());
    QCOMPARE(
        services.settingsService()
            ->load(QStringLiteral("application-services/compatibility"))
            ->toString(),
        QStringLiteral("shared-session")
        );
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
    QVERIFY(!compatibilityAdapter->isOpen());
    QVERIFY(!services.saveDatabaseAs(savedPath).has_value());
    QVERIFY(!services.exportDatabaseAs(exportedPath).has_value());
}

void DataServiceLifecycleTests::schemaFailureClosesDatabaseSession()
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
    QVERIFY(status.error().contains(QStringLiteral("campuses")));
    QVERIFY(!session.isOpen());
    QVERIFY(session.databasePath().isEmpty());
    QCOMPARE(session.settingsRepository(), nullptr);
    QCOMPARE(session.campusRecordRepository(), nullptr);
}

void DataServiceLifecycleTests::fileBackedSessionUsesEngineSchemaPipeline()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(
        QStringLiteral("engine-preflight.db")
        );
    const QString connectionName =
        QStringLiteral("engine-schema-seed");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(path);
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE teachers ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "teacher_kr TEXT, "
            "teacher_en TEXT, "
            "room_number TEXT, "
            "wifi_name TEXT, "
            "wifi_password TEXT, "
            "internet_type TEXT DEFAULT 'WiFi', "
            "zoom_id TEXT, "
            "zoom_password TEXT, "
            "projection_type TEXT DEFAULT 'HDMI', "
            "notes TEXT"
            ")"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO teachers (teacher_en) VALUES ('Legacy Teacher')"
            )));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    DatabaseSession session;
    QVERIFY(session.open(path).has_value());
    QCOMPARE(
        session.databasePath(),
        QFileInfo(path).absoluteFilePath()
        );

    QSqlQuery versionQuery(session.compatibilityDatabase());
    QVERIFY(versionQuery.exec(QStringLiteral("PRAGMA user_version")));
    QVERIFY(versionQuery.next());
    QCOMPARE(
        versionQuery.value(0).toInt(),
        DatabaseSchemaManager::LatestSchemaVersion
        );

    QSqlQuery columnQuery(session.compatibilityDatabase());
    QVERIFY(columnQuery.exec(QStringLiteral(
        "SELECT preferred_name FROM teachers"
        )));
    QVERIFY(columnQuery.next());
    QCOMPARE(columnQuery.value(0).toString(), QString());
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

    QSqlDatabase database = service.databaseSession()->compatibilityDatabase();
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

    QSqlDatabase database = service.databaseSession()->compatibilityDatabase();
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

    QSqlDatabase database = service.databaseSession()->compatibilityDatabase();
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

    QSqlDatabase database = service.databaseSession()->compatibilityDatabase();
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

    QSqlDatabase database = service.databaseSession()->compatibilityDatabase();
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

    QSqlDatabase database = service.databaseSession()->compatibilityDatabase();
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
    QVERIFY(session.compatibilityDatabase().isOpen());
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

void DataServiceLifecycleTests::memoryDatabaseIsCompatibilityOnly()
{
    DatabaseSession session;

    QVERIFY(session.open(QStringLiteral(":memory:")).has_value());
    QVERIFY(session.isOpen());
    QVERIFY(!session.isEngineBacked());
    QVERIFY(session.compatibilityDatabase().isOpen());
    QCOMPARE(session.settingsRepository(), nullptr);
    QCOMPARE(session.classRepository(), nullptr);
    QCOMPARE(session.calendarEventRepository(), nullptr);

    SettingsService settings(&session);
    QVERIFY(!settings.isAvailable());

    ApplicationServices services;
    QVERIFY(services.openDatabase(QStringLiteral(":memory:")).has_value());
    QVERIFY(!services.hasOpenDatabase());

    session.close();
    QVERIFY(!session.isOpen());
    QVERIFY(!session.isEngineBacked());
}

void DataServiceLifecycleTests::featureServicesExposeNarrowOperations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService dataService;
    SettingsService settings(dataService.databaseSession());
    TeacherService teachers(dataService.databaseSession());
    ClassService classes(dataService.databaseSession());
    ScheduleService schedule(dataService.databaseSession());
    CalendarService calendar(dataService.databaseSession());
    RosterService rosters(dataService.databaseSession());
    SpeakingEvaluationService evaluations(dataService.databaseSession());

    QVERIFY(!settings.isAvailable());
    QVERIFY(!teachers.isAvailable());
    QVERIFY(!classes.isAvailable());
    QCOMPARE(settings.databaseSession(), dataService.databaseSession());

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
    ::existingTeacherSchemaGainsPersonalDetailColumns()
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

    DataService service;
    QVERIFY(service.openDatabase(path).has_value());

    const Result<QList<Teacher>> teachers = service.getAllTeachers();
    QVERIFY(teachers);
    QCOMPARE(teachers->size(), 1);

    Teacher teacher = teachers->first();
    QVERIFY(teacher.preferredRomanization.isEmpty());
    QVERIFY(teacher.preferredName.isEmpty());
    QVERIFY(teacher.birthday.isEmpty());
    QVERIFY(teacher.phoneNumber.isEmpty());

    teacher.preferredRomanization = QStringLiteral("Legacy Teacheo");
    teacher.preferredName = QStringLiteral("Legacy Teacher");
    teacher.birthday = QStringLiteral("12-31");
    teacher.phoneNumber = QStringLiteral("010-0000-0000");
    QVERIFY(service.updateTeacher(teacher).has_value());

    const Result<Teacher> reloaded = service.getTeacher(teacher.id);
    QVERIFY(reloaded);
    QCOMPARE(
        reloaded->preferredRomanization,
        QStringLiteral("Legacy Teacheo")
        );
    QCOMPARE(reloaded->preferredName, QStringLiteral("Legacy Teacher"));
    QCOMPARE(reloaded->birthday, QStringLiteral("12-31"));
    QCOMPARE(reloaded->phoneNumber, QStringLiteral("010-0000-0000"));

    QVERIFY(
        service.saveTestingBlock(
            QStringLiteral("Thursday"),
            QStringLiteral("17:00"),
            QStringLiteral("Legacy Room")
            )
        );
    const Result<QList<TestingBlock>> testingBlocks =
        service.loadTestingBlocks();
    QVERIFY(testingBlocks);
    QCOMPARE(testingBlocks->size(), 1);
    QCOMPARE(
        testingBlocks->first().room,
        QStringLiteral("Legacy Room")
        );
}

void DataServiceLifecycleTests
    ::existingTestingSchemaGainsClassAssignmentColumn()
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

    DataService service;
    QVERIFY(service.openDatabase(path).has_value());

    Result<QList<TestingAssignment>> assignments =
        service.loadTestingAssignments();
    QVERIFY(assignments);
    QCOMPARE(assignments->size(), 1);
    QCOMPARE(
        assignments->first().kind,
        TestingAssignmentKind::PlainTesting
        );
    QCOMPARE(
        assignments->first().room,
        QStringLiteral("Legacy Room")
        );
    QCOMPARE(assignments->first().classId, -1);

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
        QVERIFY(foundClassId);
        database.close();
    }
    QSqlDatabase::removeDatabase(
        connectionName + QStringLiteral("-verification")
        );

    QVERIFY(service.openDatabase(path).has_value());
    assignments = service.loadTestingAssignments();
    QVERIFY(assignments);
    QCOMPARE(assignments->size(), 1);
    QCOMPARE(
        assignments->first().room,
        QStringLiteral("Legacy Room")
        );
}

QTEST_MAIN(DataServiceLifecycleTests)

#include "data_service_lifecycle_tests.moc"
