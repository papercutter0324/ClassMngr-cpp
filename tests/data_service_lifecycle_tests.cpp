#include "data/data_service.h"

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
    QCOMPARE(
        service.loadSetting(
            QStringLiteral("lifecycle/value")
            ).toString(),
        prefix
        );
    QCOMPARE(
        service.getTeacher(ids.teacherId).teacherEn,
        prefix + QStringLiteral(" Teacher")
        );
    QCOMPARE(
        service.getClassById(ids.classId).name,
        prefix + QStringLiteral(" Class")
        );
    QCOMPARE(
        service.loadClassInfo(ids.classId).notes,
        prefix + QStringLiteral(" Class Notes")
        );
    QCOMPARE(service.loadIntensiveSlotStates().size(), 1);
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
    void closeAndSwitchReleaseEveryRepository();
};

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
    QVERIFY(service.getAllCampuses().isEmpty());
    QVERIFY(service.loadRoster(idsA.classId).rows.isEmpty());
    QVERIFY(
        service.loadSpeakingEval(
            idsA.classId,
            QStringLiteral("First Semester")
            ).isEmpty()
        );
}

QTEST_MAIN(DataServiceLifecycleTests)

#include "data_service_lifecycle_tests.moc"
