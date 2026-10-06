#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "next/application/class_delete.h"
#include "next/platform/application_services_class_delete_port.h"

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
        QStringLiteral("class-delete-%1.tps").arg(
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

Application::ClassDeleteRequest request(const int value)
{
    return {.classId = classId(value)};
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
    const QVariant& binding
    )
{
    QSqlQuery query(database);
    query.prepare(sql);
    query.addBindValue(binding);
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

bool insertCascadeRows(QSqlDatabase database, const int classId)
{
    const QString intensiveDay = classId % 2 == 0
        ? QStringLiteral("Wednesday")
        : QStringLiteral("Tuesday");
    if (!execute(
            database,
            QStringLiteral(
                "INSERT INTO class_info "
                "(class_id, teacher_id, class_grade, class_level, notes) "
                "VALUES (?, NULL, 'E4', 'Orion', 'class info')"
                ),
            {classId})
        || !execute(
            database,
            QStringLiteral(
                "INSERT INTO roster_columns "
                "(class_id, name, position, width) VALUES (?, 'Score', 0, 120)"
                ),
            {classId})
        || !execute(
            database,
            QStringLiteral(
                "INSERT INTO roster_data "
                "(class_id, row_index, col_index, value) VALUES (?, 0, 0, 'A')"
                ),
            {classId})
        || !execute(
            database,
            QStringLiteral(
                "INSERT INTO class_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (?, 'Monday', '09:00', '10:00')"
                ),
            {classId})
        || !execute(
            database,
            QStringLiteral(
                "INSERT INTO class_intensive_times "
                "(class_id, day, start_time, end_time) "
                "VALUES (?, ?, '15:00', '17:00')"
                ),
            {classId, intensiveDay}))
    {
        return false;
    }

    QSqlQuery evaluation(database);
    evaluation.prepare(QStringLiteral(
        "INSERT INTO speaking_evaluations (class_id, evaluation_name) "
        "VALUES (?, 'Speaking')"
        ));
    evaluation.addBindValue(classId);
    if (!evaluation.exec())
    {
        return false;
    }

    QSqlQuery evaluationData(database);
    evaluationData.prepare(QStringLiteral(
        "INSERT INTO speaking_eval_data (evaluation_id, row_index, col_0) "
        "VALUES (?, 0, 'Excellent')"
        ));
    evaluationData.addBindValue(evaluation.lastInsertId());
    return evaluationData.exec();
}

int countForClass(
    QSqlDatabase database,
    const QString& table,
    const QString& idColumn,
    const int classId
    )
{
    return scalar(
        database,
        QStringLiteral("SELECT COUNT(*) FROM %1 WHERE %2=?")
            .arg(table, idColumn),
        classId
        );
}

int speakingDataCount(QSqlDatabase database, const int classId)
{
    return scalar(
        database,
        QStringLiteral(
            "SELECT COUNT(*) FROM speaking_eval_data "
            "WHERE evaluation_id IN ("
            "SELECT id FROM speaking_evaluations WHERE class_id=?)"
            ),
        classId
        );
}

using ClassCascadeTable = std::pair<QString, QString>;
const std::array<ClassCascadeTable, 7> ClassCascadeTables{{
    {QStringLiteral("classes"), QStringLiteral("id")},
    {QStringLiteral("class_info"), QStringLiteral("class_id")},
    {QStringLiteral("roster_columns"), QStringLiteral("class_id")},
    {QStringLiteral("roster_data"), QStringLiteral("class_id")},
    {QStringLiteral("class_times"), QStringLiteral("class_id")},
    {QStringLiteral("class_intensive_times"), QStringLiteral("class_id")},
    {QStringLiteral("speaking_evaluations"), QStringLiteral("class_id")}
}};

}

class NextPlatformApplicationServicesClassDeletePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void deletesThroughActiveSessionRepositoryAndPreservesSibling();
    void rollsBackEveryCascadeTableOnRepositoryFailure();
    void reportsNullUnavailableAndClosedSessionsWithoutFallback();
};

void NextPlatformApplicationServicesClassDeletePortTests::
deletesThroughActiveSessionRepositoryAndPreservesSibling()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    ClassRepository* const repository = session->classRepository();
    QVERIFY(repository);

    const auto target = repository->createClass(QStringLiteral("Delete Target"));
    const auto sibling = repository->createClass(QStringLiteral("Keep Sibling"));
    QVERIFY(target);
    QVERIFY(sibling);
    QVERIFY(insertCascadeRows(session->database(), *target));
    QVERIFY(insertCascadeRows(session->database(), *sibling));
    QCOMPARE(tableCount(session->database(), QStringLiteral("speaking_eval_data")), 2);

    Platform::ApplicationServicesClassDeletePort port(services);
    const auto result = port.deleteClass(request(*target));

    QVERIFY(result);
    for (const auto& [table, column] : ClassCascadeTables)
    {
        QCOMPARE(countForClass(session->database(), table, column, *target), 0);
        QCOMPARE(countForClass(session->database(), table, column, *sibling), 1);
    }
    QCOMPARE(speakingDataCount(session->database(), *target), 0);
    QCOMPARE(speakingDataCount(session->database(), *sibling), 1);
    QCOMPARE(tableCount(session->database(), QStringLiteral("speaking_eval_data")), 1);

    const auto survivingSibling = repository->getClassById(*sibling);
    QVERIFY(survivingSibling);
    QCOMPARE(survivingSibling->name, QStringLiteral("Keep Sibling"));
}

void NextPlatformApplicationServicesClassDeletePortTests::
rollsBackEveryCascadeTableOnRepositoryFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    ClassRepository* const repository = session->classRepository();
    QVERIFY(repository);

    const auto target = repository->createClass(QStringLiteral("Rollback Target"));
    const auto sibling = repository->createClass(QStringLiteral("Rollback Sibling"));
    QVERIFY(target);
    QVERIFY(sibling);
    QVERIFY(insertCascadeRows(session->database(), *target));
    QVERIFY(insertCascadeRows(session->database(), *sibling));
    QVERIFY(execute(
        session->database(),
        QStringLiteral(
            "CREATE TRIGGER fail_class_delete "
            "BEFORE DELETE ON classes WHEN OLD.id=%1 "
            "BEGIN SELECT RAISE(ABORT, 'F362 injected class delete failure'); END"
            ).arg(*target)
        ));

    Platform::ApplicationServicesClassDeletePort port(services);
    const auto result = port.deleteClass(request(*target));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
    QVERIFY(result.error().message.find("F362 injected class delete failure")
            != std::string::npos);
    for (const auto& [table, column] : ClassCascadeTables)
    {
        QCOMPARE(countForClass(session->database(), table, column, *target), 1);
        QCOMPARE(countForClass(session->database(), table, column, *sibling), 1);
    }
    QCOMPARE(speakingDataCount(session->database(), *target), 1);
    QCOMPARE(speakingDataCount(session->database(), *sibling), 1);
    QCOMPARE(tableCount(session->database(), QStringLiteral("speaking_eval_data")), 2);
    QVERIFY(repository->getClassById(*target));
    QVERIFY(repository->getClassById(*sibling));
}

void NextPlatformApplicationServicesClassDeletePortTests::
reportsNullUnavailableAndClosedSessionsWithoutFallback()
{
    Platform::ApplicationServicesClassDeletePort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullResult = nullPort.deleteClass(request(42));
    QVERIFY(!nullResult);
    QCOMPARE(nullResult.error().code, Domain::ErrorCode::NotFound);

    ApplicationServices unavailableServices;
    Platform::ApplicationServicesClassDeletePort unavailablePort(
        unavailableServices
        );
    const auto unavailableResult = unavailablePort.deleteClass(request(42));
    QVERIFY(!unavailableResult);
    QCOMPARE(unavailableResult.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    closedServices.closeDatabase();
    Platform::ApplicationServicesClassDeletePort closedPort(closedServices);
    const auto closedResult = closedPort.deleteClass(request(42));
    QVERIFY(!closedResult);
    QCOMPARE(closedResult.error().code, Domain::ErrorCode::NotFound);
}

QTEST_GUILESS_MAIN(NextPlatformApplicationServicesClassDeletePortTests)

#include "next_platform_application_services_class_delete_port_tests.moc"
