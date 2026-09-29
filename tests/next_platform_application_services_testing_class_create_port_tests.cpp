#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/testing_class_create.h"
#include "next/platform/application_services_testing_class_create_port.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("testing-class-create-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::TeacherId teacherId(const int value)
{
    const auto parsed = Domain::TeacherId::fromString(std::to_string(value));
    if (!parsed)
    {
        qFatal("Database teacher ID must have a typed representation.");
    }
    return *parsed;
}

Application::TestingClassCreateRequest request(
    const std::optional<int> teacher = std::optional<int>(17),
    std::optional<std::u16string> day = std::u16string(u"monday"),
    std::optional<std::u16string> startTime = std::u16string(u"16:00")
    )
{
    return {
        .name = u"Created Testing Lab",
        .grade = u"M2",
        .level = u"Song's",
        .room = u"Library 204",
        .teacherId = teacher
            ? std::optional<Domain::TeacherId>(teacherId(*teacher))
            : std::nullopt,
        .classColor = u"#123456",
        .fontColor = u"#FEDCBA",
        .notes = u"Persist every creation field",
        .assignmentDay = std::move(day),
        .assignmentStartTime = std::move(startTime)
    };
}

int createTeacher(TeacherRepository& repository, const QString& name)
{
    Teacher teacher;
    teacher.teacherEn = name;
    teacher.preferredName = name;
    const auto created = repository.createTeacher(teacher);
    return created ? *created : -1;
}

int rowCount(QSqlDatabase database, const QString& table)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

std::array<int, 4> testingClassTableCounts(QSqlDatabase database)
{
    return {
        rowCount(database, QStringLiteral("classes")),
        rowCount(database, QStringLiteral("class_info")),
        rowCount(database, QStringLiteral("testing_classes")),
        rowCount(database, QStringLiteral("schedule_testing_blocks"))
    };
}

int assignmentCount(QSqlDatabase database, const int classId)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM schedule_testing_blocks WHERE class_id=?"
        ));
    query.addBindValue(classId);
    if (!query.exec() || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

int classNameCount(QSqlDatabase database, const QString& name)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM classes WHERE name=?"));
    query.addBindValue(name);
    if (!query.exec() || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

}

class NextPlatformApplicationServicesTestingClassCreatePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void persistsEveryFieldAndReturnsTypedId();
    void persistsPendingAssignmentAndNormalizesWeekday();
    void createsWithoutAssignment();
    void reportsUnavailableSessionsWithoutFallback();
    void rejectsInvalidRequestsWithoutPersistence();
    void reportsSlotConflictAndRollsBackEveryTable();
};

void NextPlatformApplicationServicesTestingClassCreatePortTests::
persistsEveryFieldAndReturnsTypedId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    TestingClassRepository* const repository =
        session->testingClassRepository();
    TeacherRepository* const teachers = session->teacherRepository();
    QVERIFY(repository);
    QVERIFY(teachers);

    const int teacher = createTeacher(
        *teachers,
        QStringLiteral("F146 Teacher")
        );
    QVERIFY(teacher > 0);
    auto createRequest = request(teacher, std::nullopt, std::nullopt);
    Platform::ApplicationServicesTestingClassCreatePort port(services);

    const auto created = port.createTestingClass(createRequest);

    QVERIFY(created);
    QVERIFY(!created.value().value().empty());
    const auto numericClassId = std::stoi(created.value().value());
    QVERIFY(numericClassId > 0);
    const auto stored = repository->loadTestingClass(numericClassId);
    QVERIFY(stored);
    QCOMPARE(stored->name, QStringLiteral("Created Testing Lab"));
    QCOMPARE(stored->grade, QStringLiteral("M2"));
    QCOMPARE(stored->level, QStringLiteral("Song's"));
    QCOMPARE(stored->room, QStringLiteral("Library 204"));
    QCOMPARE(stored->teacherId, teacher);
    QCOMPARE(stored->classColor, QStringLiteral("#123456"));
    QCOMPARE(stored->fontColor, QStringLiteral("#FEDCBA"));
    QCOMPARE(stored->notes, QStringLiteral("Persist every creation field"));
    QCOMPARE(assignmentCount(session->database(), numericClassId), 0);
}

void NextPlatformApplicationServicesTestingClassCreatePortTests::
persistsPendingAssignmentAndNormalizesWeekday()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    Platform::ApplicationServicesTestingClassCreatePort port(services);

    const auto created = port.createTestingClass(request(
        std::nullopt,
        std::u16string(u"monday"),
        std::u16string(u"16:00")
        ));

    QVERIFY(created);
    const int numericClassId = std::stoi(created.value().value());
    QSqlQuery query(session->database());
    query.prepare(QStringLiteral(
        "SELECT day, start_time, class_id FROM schedule_testing_blocks "
        "WHERE class_id=?"
        ));
    query.addBindValue(numericClassId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("Monday"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("16:00"));
    QCOMPARE(query.value(2).toInt(), numericClassId);
    QVERIFY(!query.next());
}

void NextPlatformApplicationServicesTestingClassCreatePortTests::
createsWithoutAssignment()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    Platform::ApplicationServicesTestingClassCreatePort port(services);

    const auto created = port.createTestingClass(
        request(std::nullopt, std::nullopt, std::nullopt)
        );

    QVERIFY(created);
    const int numericClassId = std::stoi(created.value().value());
    QCOMPARE(assignmentCount(session->database(), numericClassId), 0);
}

void NextPlatformApplicationServicesTestingClassCreatePortTests::
reportsUnavailableSessionsWithoutFallback()
{
    Platform::ApplicationServicesTestingClassCreatePort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullResult = nullPort.createTestingClass(
        request(std::nullopt, std::nullopt, std::nullopt)
        );
    QVERIFY(!nullResult);
    QCOMPARE(nullResult.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    closedServices.closeDatabase();
    Platform::ApplicationServicesTestingClassCreatePort closedPort(
        closedServices
        );
    const auto closedResult = closedPort.createTestingClass(
        request(std::nullopt, std::nullopt, std::nullopt)
        );
    QVERIFY(!closedResult);
    QCOMPARE(closedResult.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesTestingClassCreatePortTests::
rejectsInvalidRequestsWithoutPersistence()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    Platform::ApplicationServicesTestingClassCreatePort port(services);
    const auto baseline = testingClassTableCounts(session->database());
    QVERIFY(std::all_of(baseline.begin(), baseline.end(), [](int value) {
        return value == 0;
    }));

    auto badTeacher = request(std::nullopt, std::nullopt, std::nullopt);
    const auto parsedTeacher = Domain::TeacherId::fromString("017");
    QVERIFY(parsedTeacher.has_value());
    badTeacher.teacherId = *parsedTeacher;
    auto halfAssignment = request();
    halfAssignment.assignmentStartTime.reset();

    for (const auto& invalidRequest : {badTeacher, halfAssignment})
    {
        const auto result = port.createTestingClass(invalidRequest);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }

    QCOMPARE(testingClassTableCounts(session->database()), baseline);
}

void NextPlatformApplicationServicesTestingClassCreatePortTests::
reportsSlotConflictAndRollsBackEveryTable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    TestingClassRepository* const repository =
        session->testingClassRepository();
    QVERIFY(repository);

    TestingClass existing;
    existing.name = QStringLiteral("Existing Slot Owner");
    existing.grade = QStringLiteral("M1");
    existing.level = QStringLiteral("Major");
    existing.room = QStringLiteral("Room A");
    const auto existingId = repository->createTestingClass(
        existing,
        QStringLiteral("Monday"),
        QStringLiteral("16:00")
        );
    QVERIFY(existingId);

    const auto before = testingClassTableCounts(session->database());
    QVERIFY(std::all_of(before.begin(), before.end(), [](int value) {
        return value >= 0;
    }));
    auto conflictingRequest = request(
        std::nullopt,
        std::u16string(u"Monday"),
        std::u16string(u"16:00")
        );
    conflictingRequest.name = u"Must Roll Back";
    Platform::ApplicationServicesTestingClassCreatePort port(services);

    const auto result = port.createTestingClass(conflictingRequest);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(testingClassTableCounts(session->database()), before);
    QCOMPARE(classNameCount(session->database(), QStringLiteral("Must Roll Back")), 0);
}

QTEST_GUILESS_MAIN(NextPlatformApplicationServicesTestingClassCreatePortTests)

#include "next_platform_application_services_testing_class_create_port_tests.moc"
