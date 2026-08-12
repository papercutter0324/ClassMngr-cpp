#include "data/data_service.h"
#include "data/database/database_session.h"
#include "app/services/feature_services.h"

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
    bool testingBlockSaved = false;
};

DatabaseIds populateDatabase(
    DataService& service,
    const QString& prefix
    )
{
    service.saveSetting(
        QStringLiteral("lifecycle/value"),
        prefix
        );

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
    ids.teacherId = service.createTeacher(teacher);
    ids.classId = service.createClass(
        prefix + QStringLiteral(" Class")
        );

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
        service.saveClassInfo(classInfo);
    Q_UNUSED(classInfoSaved);

    service.saveIntensiveSlotState(
        QStringLiteral("Tuesday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("lunch"),
        QStringLiteral("essay")
        );
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
    ids.eventId = service.saveCalendarEvent(event);

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows.append(
        {
            prefix + QStringLiteral(" Student"),
            prefix + QStringLiteral(" 학생")
        }
        );
    service.saveRoster(ids.classId, roster);

    SpeakingEvalRows evaluation =
        SpeakingEval::emptyRows();
    evaluation[0][SpeakingEval::toInt(
        SpeakingEvalColumn::EnglishName
        )] = prefix + QStringLiteral(" Student");
    const bool evaluationSaved =
        service.saveSpeakingEval(
            ids.classId,
            QStringLiteral("First Semester"),
            evaluation
        );
    Q_UNUSED(evaluationSaved);

    CampusRecord campus;
    campus.name = prefix + QStringLiteral(" Campus");
    campus.officeNumber = prefix + QStringLiteral(" Office");
    ids.campusId = service.saveCampus(campus);

    return ids;
}

void verifyDatabase(
    DataService& service,
    const QString& prefix,
    const DatabaseIds& ids
    )
{
    QVERIFY(ids.testingBlockSaved);
    QCOMPARE(
        service.loadSetting(
            QStringLiteral("lifecycle/value")
            ).toString(),
        prefix
        );
    const Teacher teacher = service.getTeacher(ids.teacherId);
    QCOMPARE(teacher.teacherEn, prefix + QStringLiteral(" Teacher"));
    QCOMPARE(
        teacher.preferredRomanization,
        prefix + QStringLiteral(" Romanization")
        );
    QCOMPARE(
        teacher.preferredName,
        prefix + QStringLiteral(" Preferred Name")
        );
    QCOMPARE(teacher.roomNumber, prefix + QStringLiteral(" Room"));
    QCOMPARE(teacher.birthday, QStringLiteral("02-29"));
    QCOMPARE(teacher.phoneNumber, prefix + QStringLiteral(" Phone"));
    QCOMPARE(
        service.getClassById(ids.classId).name,
        prefix + QStringLiteral(" Class")
        );
    QCOMPARE(
        service.loadClassInfo(ids.classId).notes,
        prefix + QStringLiteral(" Class Notes")
        );
    QCOMPARE(
        service.loadClassInfo(ids.classId).teacherPreferredName,
        prefix + QStringLiteral(" Preferred Name")
        );
    QCOMPARE(service.loadIntensiveSlotStates().size(), 1);
    const Result<QList<TestingBlock>> testingBlocks =
        service.loadTestingBlocks();
    QVERIFY(testingBlocks);
    QCOMPARE(testingBlocks->size(), 1);
    QCOMPARE(
        testingBlocks->first().room,
        prefix + QStringLiteral(" Testing Room")
        );
    QCOMPARE(
        service.getCalendarEvent(ids.eventId).title,
        prefix + QStringLiteral(" Event")
        );
    const Roster roster =
        service.loadRoster(ids.classId);
    QVERIFY(!roster.rows.isEmpty());
    QVERIFY(!roster.rows.first().isEmpty());
    QCOMPARE(
        roster.rows.first().first(),
        prefix + QStringLiteral(" Student")
        );
    const SpeakingEvalRows evaluation =
        service.loadSpeakingEval(
            ids.classId,
            QStringLiteral("First Semester")
            );
    QVERIFY(!evaluation.isEmpty());
    QVERIFY(
        evaluation.first().size()
        > SpeakingEval::toInt(
            SpeakingEvalColumn::EnglishName
            )
        );
    QCOMPARE(
        evaluation[0][SpeakingEval::toInt(
                SpeakingEvalColumn::EnglishName
                )],
        prefix + QStringLiteral(" Student")
        );
    QCOMPARE(
        service.getCampus(ids.campusId).name,
        prefix + QStringLiteral(" Campus")
        );
}
}

class DataServiceLifecycleTests : public QObject
{
    Q_OBJECT

private slots:
    void databaseSessionOwnsRepositoryLifetime();
    void featureServicesExposeNarrowOperations();
    void closeAndSwitchReleaseEveryRepository();
    void existingTeacherSchemaGainsPersonalDetailColumns();
    void existingTestingSchemaGainsClassAssignmentColumn();
};

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

    settings.save(
        QStringLiteral("feature-services/value"),
        QStringLiteral("saved")
        );
    QCOMPARE(
        settings.load(QStringLiteral("feature-services/value")).toString(),
        QStringLiteral("saved")
        );

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Narrow Teacher");
    const int teacherId = teachers.create(teacher);
    const int classId = classes.create(QStringLiteral("Narrow Class"));
    QVERIFY(teacherId > 0);
    QVERIFY(classId > 0);
    QCOMPARE(teachers.teacher(teacherId).teacherEn,
             QStringLiteral("Narrow Teacher"));
    QCOMPARE(classes.classroom(classId).name,
             QStringLiteral("Narrow Class"));

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows.append(
        {QStringLiteral("Student"), QStringLiteral("학생")}
        );
    rosters.saveRoster(classId, roster);
    QCOMPARE(rosters.studentCount(classId), 1);

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
    QVERIFY(service.getAllTeachers().isEmpty());
    QVERIFY(service.getClasses().isEmpty());
    QVERIFY(service.loadIntensiveSlotStates().isEmpty());
    const Result<QList<TestingBlock>> emptyTestingBlocks =
        service.loadTestingBlocks();
    QVERIFY(emptyTestingBlocks);
    QVERIFY(emptyTestingBlocks->isEmpty());
    QVERIFY(
        service.loadCalendarEventsForDate(
            QDate(2026, 7, 17)
            ).isEmpty()
        );
    QVERIFY(service.getAllCampuses().isEmpty());
    QCOMPARE(
        service.loadSetting(
            QStringLiteral("lifecycle/value"),
            QStringLiteral("missing")
            ).toString(),
        QStringLiteral("missing")
        );

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
    QVERIFY(service.getAllTeachers().isEmpty());
    QVERIFY(service.getClasses().isEmpty());
    QVERIFY(service.loadIntensiveSlotStates().isEmpty());
    QVERIFY(!service.loadTestingBlocks());
    QVERIFY(service.getAllCampuses().isEmpty());
    QVERIFY(service.loadRoster(idsA.classId).rows.isEmpty());
    QVERIFY(
        service.loadSpeakingEval(
            idsA.classId,
            QStringLiteral("First Semester")
            ).isEmpty()
    );
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

    const QList<Teacher> teachers = service.getAllTeachers();
    QCOMPARE(teachers.size(), 1);

    Teacher teacher = teachers.first();
    QVERIFY(teacher.preferredRomanization.isEmpty());
    QVERIFY(teacher.preferredName.isEmpty());
    QVERIFY(teacher.birthday.isEmpty());
    QVERIFY(teacher.phoneNumber.isEmpty());

    teacher.preferredRomanization = QStringLiteral("Legacy Teacheo");
    teacher.preferredName = QStringLiteral("Legacy Teacher");
    teacher.birthday = QStringLiteral("12-31");
    teacher.phoneNumber = QStringLiteral("010-0000-0000");
    service.updateTeacher(teacher);

    const Teacher reloaded = service.getTeacher(teacher.id);
    QCOMPARE(
        reloaded.preferredRomanization,
        QStringLiteral("Legacy Teacheo")
        );
    QCOMPARE(reloaded.preferredName, QStringLiteral("Legacy Teacher"));
    QCOMPARE(reloaded.birthday, QStringLiteral("12-31"));
    QCOMPARE(reloaded.phoneNumber, QStringLiteral("010-0000-0000"));

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
