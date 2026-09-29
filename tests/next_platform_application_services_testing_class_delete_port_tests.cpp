#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_class_repository.h"
#include "domain/models/testing_class.h"
#include "next/application/testing_class_delete.h"
#include "next/platform/application_services_testing_class_delete_port.h"

#include <QSqlDatabase>
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
        QStringLiteral("testing-class-delete-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    const auto parsed = Domain::ClassId::fromString(std::to_string(value));
    if (!parsed)
    {
        qFatal("Database class ID must have a typed representation.");
    }
    return *parsed;
}

Application::TestingClassDeleteRequest request(const int value)
{
    return {.classId = classId(value)};
}

int scalar(
    QSqlDatabase database,
    const QString& sql,
    const QVariant& value
    )
{
    QSqlQuery query(database);
    query.prepare(sql);
    query.addBindValue(value);
    if (!query.exec() || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

int tableCount(QSqlDatabase database, const QString& table)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

bool insertCascadeRows(
    QSqlDatabase database,
    const int classId,
    const QString& day,
    const QString& startTime,
    const QString& evaluationName
    )
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO schedule_testing_blocks (day, start_time, room, class_id) "
        "VALUES (?, ?, 'Library', ?)"
        ));
    query.addBindValue(day);
    query.addBindValue(startTime);
    query.addBindValue(classId);
    if (!query.exec())
    {
        return false;
    }

    query.prepare(QStringLiteral(
        "INSERT INTO roster_columns (class_id, name, position, width) "
        "VALUES (?, 'English', 0, 120)"
        ));
    query.addBindValue(classId);
    if (!query.exec())
    {
        return false;
    }
    query.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, 0, 0, 'A')"
        ));
    query.addBindValue(classId);
    if (!query.exec())
    {
        return false;
    }

    query.prepare(QStringLiteral(
        "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
        "VALUES (?, ?)"
        ));
    query.addBindValue(classId);
    query.addBindValue(evaluationName);
    if (!query.exec())
    {
        return false;
    }
    const QVariant evaluationId = query.lastInsertId();
    query.prepare(QStringLiteral(
        "INSERT INTO speaking_eval_data (evaluation_id, row_index, col_0) "
        "VALUES (?, 0, 'Score')"
        ));
    query.addBindValue(evaluationId);
    if (!query.exec())
    {
        return false;
    }

    query.prepare(QStringLiteral(
        "INSERT INTO class_times (class_id, day, start_time, end_time) "
        "VALUES (?, 'Monday', '09:00', '10:00')"
        ));
    query.addBindValue(classId);
    if (!query.exec())
    {
        return false;
    }
    query.prepare(QStringLiteral(
        "INSERT INTO class_intensive_times (class_id, day, start_time, end_time) "
        "VALUES (?, ?, '15:00', '17:00')"
        ));
    query.addBindValue(classId);
    query.addBindValue(
        day == QStringLiteral("Monday")
            ? QStringLiteral("Tuesday")
            : QStringLiteral("Wednesday")
        );
    return query.exec();
}

TestingClass newTestingClass(const QString& name, const QString& room)
{
    TestingClass value;
    value.name = name;
    value.grade = QStringLiteral("M1");
    value.level = QStringLiteral("Major");
    value.room = room;
    value.teacherId = -1;
    value.classColor = QStringLiteral("#123456");
    value.fontColor = QStringLiteral("#FEDCBA");
    value.notes = QStringLiteral("Cascade test notes");
    return value;
}

}

class NextPlatformApplicationServicesTestingClassDeletePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void deletesThroughActiveSessionRepositoryAndPreservesSibling();
    void reportsUnavailableSessionsWithoutFallback();
    void mapsMissingClassAndRepositoryErrorToTechnical();
};

void NextPlatformApplicationServicesTestingClassDeletePortTests::
deletesThroughActiveSessionRepositoryAndPreservesSibling()
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

    const auto target = repository->createTestingClass(
        newTestingClass(QStringLiteral("Delete Target"), QStringLiteral("Room A"))
        );
    const auto sibling = repository->createTestingClass(
        newTestingClass(QStringLiteral("Preserved Sibling"), QStringLiteral("Room B"))
        );
    QVERIFY(target);
    QVERIFY(sibling);
    QVERIFY(insertCascadeRows(
        session->database(), *target, QStringLiteral("Monday"),
        QStringLiteral("16:00"), QStringLiteral("Target Speaking")
        ));
    QVERIFY(insertCascadeRows(
        session->database(), *sibling, QStringLiteral("Tuesday"),
        QStringLiteral("17:00"), QStringLiteral("Sibling Speaking")
        ));
    QCOMPARE(tableCount(session->database(), QStringLiteral("speaking_eval_data")), 2);

    Platform::ApplicationServicesTestingClassDeletePort port(services);
    const auto result = port.deleteTestingClass(request(*target));

    QVERIFY(result);
    const std::array<std::pair<QString, QString>, 9> targetTables{{
        {QStringLiteral("classes"), QStringLiteral("id")},
        {QStringLiteral("class_info"), QStringLiteral("class_id")},
        {QStringLiteral("testing_classes"), QStringLiteral("class_id")},
        {QStringLiteral("schedule_testing_blocks"), QStringLiteral("class_id")},
        {QStringLiteral("roster_columns"), QStringLiteral("class_id")},
        {QStringLiteral("roster_data"), QStringLiteral("class_id")},
        {QStringLiteral("speaking_evaluations"), QStringLiteral("class_id")},
        {QStringLiteral("class_times"), QStringLiteral("class_id")},
        {QStringLiteral("class_intensive_times"), QStringLiteral("class_id")}
    }};
    for (const auto& [table, column] : targetTables)
    {
        const QString sql = QStringLiteral("SELECT COUNT(*) FROM %1 WHERE %2=?")
            .arg(table, column);
        QCOMPARE(scalar(session->database(), sql, *target), 0);
        QCOMPARE(scalar(session->database(),
            QStringLiteral("SELECT COUNT(*) FROM %1 WHERE %2=?").arg(table, column),
            *sibling), 1);
    }
    QCOMPARE(scalar(
        session->database(),
        QStringLiteral(
            "SELECT COUNT(*) FROM speaking_eval_data WHERE evaluation_id IN ("
            "SELECT id FROM speaking_evaluations WHERE class_id=?)"
            ),
        *target
        ), 0);
    QCOMPARE(scalar(
        session->database(),
        QStringLiteral(
            "SELECT COUNT(*) FROM speaking_eval_data WHERE evaluation_id IN ("
            "SELECT id FROM speaking_evaluations WHERE class_id=?)"
            ),
        *sibling
        ), 1);
    QCOMPARE(tableCount(session->database(), QStringLiteral("speaking_eval_data")), 1);

    const auto survivingSibling = repository->loadTestingClass(*sibling);
    QVERIFY(survivingSibling);
    QCOMPARE(survivingSibling->name, QStringLiteral("Preserved Sibling"));
    QCOMPARE(survivingSibling->room, QStringLiteral("Room B"));
}

void NextPlatformApplicationServicesTestingClassDeletePortTests::
reportsUnavailableSessionsWithoutFallback()
{
    Platform::ApplicationServicesTestingClassDeletePort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullResult = nullPort.deleteTestingClass(request(42));
    QVERIFY(!nullResult);
    QCOMPARE(nullResult.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    services.closeDatabase();
    Platform::ApplicationServicesTestingClassDeletePort closedPort(services);
    const auto closedResult = closedPort.deleteTestingClass(request(42));
    QVERIFY(!closedResult);
    QCOMPARE(closedResult.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesTestingClassDeletePortTests::
mapsMissingClassAndRepositoryErrorToTechnical()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    auto* const repository = session->testingClassRepository();
    QVERIFY(repository);
    Platform::ApplicationServicesTestingClassDeletePort port(services);

    const auto missing = port.deleteTestingClass(request(987654));
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(
        missing.error().message,
        std::string("The testing class no longer exists.")
        );

    const auto target = repository->createTestingClass(
        newTestingClass(QStringLiteral("Blocked Delete"), QStringLiteral("Room C"))
        );
    QVERIFY(target);
    QSqlQuery trigger(session->database());
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER fail_testing_class_delete "
        "BEFORE DELETE ON classes WHEN OLD.id=%1 "
        "BEGIN SELECT RAISE(ABORT, 'injected final delete failure'); END"
        ).arg(*target)));

    const auto repositoryFailure = port.deleteTestingClass(request(*target));
    QVERIFY(!repositoryFailure);
    QCOMPARE(repositoryFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(repositoryFailure.error().message.find(
        "injected final delete failure"
        ) != std::string::npos);
    const auto stillPresent = repository->loadTestingClass(*target);
    QVERIFY(stillPresent);
}

QTEST_GUILESS_MAIN(NextPlatformApplicationServicesTestingClassDeletePortTests)

#include "next_platform_application_services_testing_class_delete_port_tests.moc"
