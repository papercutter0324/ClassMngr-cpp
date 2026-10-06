#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/testing_block_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "domain/models/testing_class.h"
#include "next/application/schedule_testing_layout_clear_port.h"
#include "next/platform/application_services_schedule_testing_layout_clear_port.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("testing-layout-clear-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool execute(
    QSqlDatabase database,
    const QString& sql,
    const QList<QVariant>& bindings = {}
    )
{
    QSqlQuery query(database);
    if (!query.prepare(sql))
    {
        return false;
    }
    for (const QVariant& binding : bindings)
    {
        query.addBindValue(binding);
    }
    return query.exec();
}

int scalar(
    QSqlDatabase database,
    const QString& sql,
    const QVariant& binding = {}
    )
{
    QSqlQuery query(database);
    if (!query.prepare(sql))
    {
        return -1;
    }
    if (binding.isValid())
    {
        query.addBindValue(binding);
    }
    if (!query.exec() || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

bool insertRoster(QSqlDatabase database, const int classId)
{
    return execute(
               database,
               QStringLiteral(
                   "INSERT INTO roster_columns "
                   "(class_id, name, position, width) "
                   "VALUES (?, 'Student', 0, 140)"
                   ),
               {classId}
               )
        && execute(
            database,
            QStringLiteral(
                "INSERT INTO roster_data "
                "(class_id, row_index, col_index, value) "
                "VALUES (?, 0, 0, 'Saved Student')"
                ),
            {classId}
            );
}

int createTestingClass(
    TestingClassRepository* repository,
    const QString& name,
    const QString& room
    )
{
    if (!repository)
    {
        return -1;
    }
    TestingClass testingClass;
    testingClass.name = name;
    testingClass.grade = QStringLiteral("M1");
    testingClass.level = QStringLiteral("Solis");
    testingClass.room = room;
    const auto created = repository->createTestingClass(testingClass);
    return created ? *created : -1;
}

QStringList blockRows(QSqlDatabase database)
{
    QStringList rows;
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT day || '|' || start_time || '|' || room || '|' || "
            "COALESCE(class_id, -1) FROM schedule_testing_blocks "
            "ORDER BY day, start_time"
            )))
    {
        return {QStringLiteral("query-failed")};
    }
    while (query.next())
    {
        rows.append(query.value(0).toString());
    }
    return rows;
}

}

class NextPlatformApplicationServicesScheduleTestingLayoutClearPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void reportsUnavailableAndClosedSessionsStructurally();
    void clearsLayoutAndPreservesSavedRecords();
    void preservesEveryLayoutRowWhenDeleteFails();
};

void NextPlatformApplicationServicesScheduleTestingLayoutClearPortTests::
reportsUnavailableAndClosedSessionsStructurally()
{
    Platform::ApplicationServicesScheduleTestingLayoutClearPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    QVERIFY(!nullPort.isAvailable());
    const auto nullResult = nullPort.clearTestingLayout();
    QVERIFY(!nullResult);
    QCOMPARE(nullResult.error().code, Domain::ErrorCode::NotFound);

    ApplicationServices unavailableServices;
    QVERIFY(unavailableServices.dataService());
    Platform::ApplicationServicesScheduleTestingLayoutClearPort unavailablePort(
        unavailableServices
        );
    QVERIFY(!unavailablePort.isAvailable());
    const auto unavailableResult = unavailablePort.clearTestingLayout();
    QVERIFY(!unavailableResult);
    QCOMPARE(unavailableResult.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesScheduleTestingLayoutClearPort closedPort(
        closedServices
        );
    QVERIFY(closedPort.isAvailable());
    closedServices.closeDatabase();
    QVERIFY(!closedPort.isAvailable());
    const auto closedResult = closedPort.clearTestingLayout();
    QVERIFY(!closedResult);
    QCOMPARE(closedResult.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesScheduleTestingLayoutClearPortTests::
clearsLayoutAndPreservesSavedRecords()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    TestingBlockRepository* const blocks = session->testingBlockRepository();
    TestingClassRepository* const testingClasses =
        session->testingClassRepository();
    ClassRepository* const classes = session->classRepository();
    QVERIFY(blocks);
    QVERIFY(testingClasses);
    QVERIFY(classes);

    const int firstTestingClass = createTestingClass(
        testingClasses,
        QStringLiteral("Saved Testing A"),
        QStringLiteral("Room 101")
        );
    const int secondTestingClass = createTestingClass(
        testingClasses,
        QStringLiteral("Saved Testing B"),
        QStringLiteral("Room 202")
        );
    const auto unrelatedClass = classes->createClass(
        QStringLiteral("Unrelated Class")
        );
    QVERIFY(firstTestingClass > 0);
    QVERIFY(secondTestingClass > 0);
    QVERIFY(unrelatedClass);
    const int unrelatedClassId = *unrelatedClass;

    QVERIFY(insertRoster(session->database(), firstTestingClass));
    QVERIFY(insertRoster(session->database(), secondTestingClass));
    QVERIFY(insertRoster(session->database(), unrelatedClassId));
    QVERIFY(execute(
        session->database(),
        QStringLiteral(
            "INSERT INTO class_info "
            "(class_id, class_grade, class_level, notes) "
            "VALUES (?, 'E4', 'Orion', 'Keep unrelated class info')"
            ),
        {unrelatedClassId}
        ));
    QVERIFY(execute(
        session->database(),
        QStringLiteral(
            "INSERT INTO class_times "
            "(class_id, day, start_time, end_time) "
            "VALUES (?, 'Friday', '09:00', '10:00')"
            ),
        {unrelatedClassId}
        ));

    QVERIFY(blocks->saveTestingBlock(
        QStringLiteral("Monday"),
        QStringLiteral("16:00"),
        QStringLiteral("Oral Room")
        ));
    QVERIFY(blocks->assignTestingClass(
        QStringLiteral("Tuesday"),
        QStringLiteral("16:00"),
        firstTestingClass
        ));
    QVERIFY(blocks->assignTestingClass(
        QStringLiteral("Wednesday"),
        QStringLiteral("16:00"),
        secondTestingClass
        ));
    QCOMPARE(scalar(
                 session->database(),
                 QStringLiteral("SELECT COUNT(*) FROM schedule_testing_blocks")
                 ), 3);

    Platform::ApplicationServicesScheduleTestingLayoutClearPort port(services);
    QVERIFY(port.isAvailable());
    const auto result = port.clearTestingLayout();

    QVERIFY(result);
    QCOMPARE(scalar(
                 session->database(),
                 QStringLiteral("SELECT COUNT(*) FROM schedule_testing_blocks")
                 ), 0);
    QCOMPARE(scalar(
                 session->database(),
                 QStringLiteral("SELECT COUNT(*) FROM testing_classes")
                 ), 2);
    QCOMPARE(scalar(
                 session->database(),
                 QStringLiteral("SELECT COUNT(*) FROM classes")
                 ), 3);
    QCOMPARE(scalar(
                 session->database(),
                 QStringLiteral("SELECT COUNT(*) FROM class_info WHERE class_id=?"),
                 unrelatedClassId
                 ), 1);
    QCOMPARE(scalar(
                 session->database(),
                 QStringLiteral("SELECT COUNT(*) FROM class_times WHERE class_id=?"),
                 unrelatedClassId
                 ), 1);
    QCOMPARE(scalar(
                 session->database(),
                 QStringLiteral("SELECT COUNT(*) FROM roster_columns")
                 ), 3);
    QCOMPARE(scalar(
                 session->database(),
                 QStringLiteral("SELECT COUNT(*) FROM roster_data")
                 ), 3);

    const auto savedFirst = testingClasses->loadTestingClass(firstTestingClass);
    const auto savedSecond = testingClasses->loadTestingClass(secondTestingClass);
    QVERIFY(savedFirst);
    QVERIFY(savedSecond);
    QCOMPARE(savedFirst->name, QStringLiteral("Saved Testing A"));
    QCOMPARE(savedFirst->room, QStringLiteral("Room 101"));
    QCOMPARE(savedSecond->name, QStringLiteral("Saved Testing B"));
    QCOMPARE(savedSecond->room, QStringLiteral("Room 202"));
}

void NextPlatformApplicationServicesScheduleTestingLayoutClearPortTests::
preservesEveryLayoutRowWhenDeleteFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    TestingBlockRepository* const repository =
        session->testingBlockRepository();
    QVERIFY(repository);
    QVERIFY(repository->saveTestingBlock(
        QStringLiteral("Monday"),
        QStringLiteral("15:00"),
        QStringLiteral("Room A")
        ));
    QVERIFY(repository->saveTestingBlock(
        QStringLiteral("Tuesday"),
        QStringLiteral("16:00"),
        QStringLiteral("Room B")
        ));
    const QStringList expectedRows{
        QStringLiteral("Monday|15:00|Room A|-1"),
        QStringLiteral("Tuesday|16:00|Room B|-1")
    };
    QCOMPARE(blockRows(session->database()), expectedRows);
    QVERIFY(execute(
        session->database(),
        QStringLiteral(
            "CREATE TRIGGER fail_testing_layout_clear "
            "BEFORE DELETE ON schedule_testing_blocks "
            "BEGIN SELECT RAISE(ABORT, 'F364 injected layout clear failure'); END"
            )
        ));

    Platform::ApplicationServicesScheduleTestingLayoutClearPort port(services);
    const auto result = port.clearTestingLayout();

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
    QVERIFY(result.error().message.find("F364 injected layout clear failure")
            != std::string::npos);
    QCOMPARE(blockRows(session->database()), expectedRows);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesScheduleTestingLayoutClearPortTests
    )

#include "next_platform_application_services_schedule_testing_layout_clear_port_tests.moc"
