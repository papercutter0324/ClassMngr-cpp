#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_block_repository.h"
#include "next/application/schedule_testing_assignment_save_use_case.h"
#include "next/platform/application_services_schedule_testing_assignment_save_port.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("testing-assignment-save-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool execute(
    QSqlDatabase database,
    const QString& sql,
    const QVariantList& values = {}
    )
{
    QSqlQuery query(database);
    query.prepare(sql);
    for (const QVariant& value : values)
    {
        query.addBindValue(value);
    }
    return query.exec();
}

int insertTestingClass(QSqlDatabase database)
{
    QSqlQuery classQuery(database);
    classQuery.prepare(QStringLiteral(
        "INSERT INTO classes (name) VALUES (?)"
        ));
    classQuery.addBindValue(QStringLiteral("Writing Lab"));
    if (!classQuery.exec())
    {
        return -1;
    }

    const int classId = classQuery.lastInsertId().toInt();
    if (!execute(
            database,
            QStringLiteral(R"(
                INSERT INTO class_info (
                    class_id, class_grade, class_level,
                    class_color, font_color
                ) VALUES (?, ?, ?, ?, ?)
            )"),
            {
                classId,
                QStringLiteral("M2"),
                QStringLiteral("Mixed (High)"),
                QStringLiteral("#123456"),
                QStringLiteral("#FFFFFF")
            }
            ))
    {
        return -1;
    }

    if (!execute(
            database,
            QStringLiteral(
                "INSERT INTO testing_classes (class_id, room) VALUES (?, ?)"
                ),
            {classId, QStringLiteral("Library")}
            ))
    {
        return -1;
    }

    return classId;
}

Application::ScheduleTestingAssignmentSaveRequest plainRequest(
    const std::u16string day,
    const std::u16string startTime,
    const std::u16string room,
    const bool replaceExisting = false
    )
{
    return {
        .mutation =
            Application::ScheduleTestingAssignmentMutation::SavePlainTesting,
        .day = day,
        .startTime = startTime,
        .room = room,
        .replaceExisting = replaceExisting
    };
}

Application::ScheduleTestingAssignmentSaveRequest removeRequest(
    const std::u16string day,
    const std::u16string startTime
    )
{
    return {
        .mutation =
            Application::ScheduleTestingAssignmentMutation::RemoveAssignment,
        .day = day,
        .startTime = startTime
    };
}

Application::ScheduleTestingAssignmentSaveRequest classRequest(
    const std::u16string day,
    const std::u16string startTime,
    const int classId,
    const bool replaceExisting = false
    )
{
    return {
        .mutation =
            Application::ScheduleTestingAssignmentMutation::AssignTestingClass,
        .day = day,
        .startTime = startTime,
        .classId = Domain::ClassId::fromString(std::to_string(classId)),
        .replaceExisting = replaceExisting
    };
}

QList<TestingAssignment> loadedAssignments(ApplicationServices& services)
{
    return services.databaseSession()
        ->testingBlockRepository()
        ->loadTestingAssignments()
        .value();
}

}

class NextPlatformApplicationServicesScheduleTestingAssignmentSavePortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void savesPlainAssignmentAndRemovesOnlyItsKey();
    void enforcesReplaceExistingForClassAndPlainTransitions();
    void rejectsWritesWithoutAnOpenSessionWithoutFallback();
    void forwardsRepositoryFailureAsStructuredError();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesScheduleTestingAssignmentSavePortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesScheduleTestingAssignmentSavePortTests::
savesPlainAssignmentAndRemovesOnlyItsKey()
{
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(m_directory)));
    Platform::ApplicationServicesScheduleTestingAssignmentSavePort port(
        services
        );

    const auto saved = Application::ScheduleTestingAssignmentSaveUseCase::
        execute(
            plainRequest(u"Thursday", u"13:05", u"  Library  "),
            port
            );
    QVERIFY(saved);

    QSqlQuery keyQuery(services.databaseSession()->database());
    QVERIFY(keyQuery.exec(QStringLiteral(R"(
        SELECT day, start_time, room, class_id
        FROM schedule_testing_blocks
        ORDER BY day, start_time
    )")));
    QVERIFY(keyQuery.next());
    QCOMPARE(keyQuery.value(QStringLiteral("day")).toString(),
             QStringLiteral("Thursday"));
    QCOMPARE(keyQuery.value(QStringLiteral("start_time")).toString(),
             QStringLiteral("13:05"));
    QCOMPARE(keyQuery.value(QStringLiteral("room")).toString(),
             QStringLiteral("Library"));
    QVERIFY(keyQuery.value(QStringLiteral("class_id")).isNull());
    QVERIFY(!keyQuery.next());

    QVERIFY(Application::ScheduleTestingAssignmentSaveUseCase::execute(
        plainRequest(u"Thursday", u"14:05", u"Room 2"),
        port
        ));
    const auto removed = Application::ScheduleTestingAssignmentSaveUseCase::
        execute(removeRequest(u"Thursday", u"13:05"), port);
    QVERIFY(removed);

    const QList<TestingAssignment> assignments = loadedAssignments(services);
    QCOMPARE(assignments.size(), 1);
    QCOMPARE(assignments.first().day, QStringLiteral("Thursday"));
    QCOMPARE(assignments.first().startTime, QStringLiteral("14:05"));
    QCOMPARE(assignments.first().room, QStringLiteral("Room 2"));
    QCOMPARE(assignments.first().kind, TestingAssignmentKind::PlainTesting);
}

void NextPlatformApplicationServicesScheduleTestingAssignmentSavePortTests::
enforcesReplaceExistingForClassAndPlainTransitions()
{
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(m_directory)));
    const int classId = insertTestingClass(
        services.databaseSession()->database()
        );
    QVERIFY(classId > 0);

    Platform::ApplicationServicesScheduleTestingAssignmentSavePort port(
        services
        );
    auto result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        classRequest(u"Monday", u"09:00", classId),
        port
        );
    QVERIFY(result);

    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        plainRequest(u"Monday", u"09:00", u"Room 5"),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
    const QList<TestingAssignment> unchangedClassAssignment =
        loadedAssignments(services);
    QCOMPARE(unchangedClassAssignment.size(), 1);
    QCOMPARE(unchangedClassAssignment.first().classId, classId);

    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        plainRequest(u"Monday", u"09:00", u"  Room 5  ", true),
        port
        );
    QVERIFY(result);
    auto assignments = loadedAssignments(services);
    QCOMPARE(assignments.size(), 1);
    QCOMPARE(assignments.first().day, QStringLiteral("Monday"));
    QCOMPARE(assignments.first().startTime, QStringLiteral("09:00"));
    QCOMPARE(assignments.first().room, QStringLiteral("Room 5"));
    QCOMPARE(assignments.first().classId, -1);
    QCOMPARE(assignments.first().kind, TestingAssignmentKind::PlainTesting);

    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        classRequest(u"Monday", u"09:00", classId),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    assignments = loadedAssignments(services);
    QCOMPARE(assignments.size(), 1);
    QCOMPARE(assignments.first().classId, -1);
    QCOMPARE(assignments.first().room, QStringLiteral("Room 5"));

    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        classRequest(u"Monday", u"09:00", classId, true),
        port
        );
    QVERIFY(result);
    assignments = loadedAssignments(services);
    QCOMPARE(assignments.size(), 1);
    QCOMPARE(assignments.first().day, QStringLiteral("Monday"));
    QCOMPARE(assignments.first().startTime, QStringLiteral("09:00"));
    QCOMPARE(assignments.first().room, QString());
    QCOMPARE(assignments.first().classId, classId);
    QCOMPARE(assignments.first().kind, TestingAssignmentKind::SpecialClass);

    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        classRequest(u"Tuesday", u"10:00", classId),
        port
        );
    QVERIFY(result);
    assignments = loadedAssignments(services);
    QCOMPARE(assignments.size(), 2);
    QCOMPARE(assignments.last().day, QStringLiteral("Tuesday"));
    QCOMPARE(assignments.last().startTime, QStringLiteral("10:00"));
    QCOMPARE(assignments.last().classId, classId);
}

void NextPlatformApplicationServicesScheduleTestingAssignmentSavePortTests::
rejectsWritesWithoutAnOpenSessionWithoutFallback()
{
    Platform::ApplicationServicesScheduleTestingAssignmentSavePort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    auto result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        plainRequest(u"Wednesday", u"11:00", u"Room 9"),
        nullPort
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!result.error().recoverable);

    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));
    Platform::ApplicationServicesScheduleTestingAssignmentSavePort port(
        services
        );
    QVERIFY(Application::ScheduleTestingAssignmentSaveUseCase::execute(
        plainRequest(u"Wednesday", u"11:00", u"Room 9"),
        port
        ));
    services.closeDatabase();

    result = Application::ScheduleTestingAssignmentSaveUseCase::execute(
        plainRequest(u"Wednesday", u"12:00", u"Room 8"),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!result.error().recoverable);

    QVERIFY(services.openDatabase(path));
    const QList<TestingAssignment> assignments = loadedAssignments(services);
    QCOMPARE(assignments.size(), 1);
    QCOMPARE(assignments.first().day, QStringLiteral("Wednesday"));
    QCOMPARE(assignments.first().startTime, QStringLiteral("11:00"));
    QCOMPARE(assignments.first().room, QStringLiteral("Room 9"));
}

void NextPlatformApplicationServicesScheduleTestingAssignmentSavePortTests::
forwardsRepositoryFailureAsStructuredError()
{
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(m_directory)));
    QVERIFY(execute(
        services.databaseSession()->database(),
        QStringLiteral("DROP TABLE schedule_testing_blocks")
        ));

    Platform::ApplicationServicesScheduleTestingAssignmentSavePort port(
        services
        );
    const auto result = Application::ScheduleTestingAssignmentSaveUseCase::
        execute(
            plainRequest(u"Friday", u"10:00", u"Room 7"),
            port
            );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
    QVERIFY(!result.error().message.empty());
}

QTEST_MAIN(
    NextPlatformApplicationServicesScheduleTestingAssignmentSavePortTests
    )

#include "next_platform_application_services_schedule_testing_assignment_save_port_tests.moc"
