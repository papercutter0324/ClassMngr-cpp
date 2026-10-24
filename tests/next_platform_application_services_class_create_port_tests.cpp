#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_repository.h"
#include "next/application/class_create_use_case.h"
#include "next/platform/application_services_class_create_port.h"

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
        QStringLiteral("class-create-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool executeSql(ApplicationServices& services, const QString& statement)
{
    WorkspaceStore* const store = services.workspaceStore();
    if (!store || !store->isOpen())
    {
        return false;
    }
    QSqlQuery query(store->database());
    return query.exec(statement);
}

int classCount(ApplicationServices& services)
{
    WorkspaceStore* const store = services.workspaceStore();
    if (!store || !store->isOpen())
    {
        return -1;
    }
    QSqlQuery query(store->database());
    return query.exec(QStringLiteral("SELECT COUNT(*) FROM classes"))
        && query.next()
        ? query.value(0).toInt()
        : -1;
}

}

class NextPlatformApplicationServicesClassCreatePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void createsBlankClassFromOpenSession();
    void rejectsClassIdBeyondIntRangeWithoutRemovingInsertedClass();
    void unavailableAndClosedSessionsReturnStructuredFailures();
    void repositoryFailureDoesNotCreateAClass();
};

void NextPlatformApplicationServicesClassCreatePortTests::
createsBlankClassFromOpenSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Platform::ApplicationServicesClassCreatePort port(services);
    const auto created = Application::ClassCreateUseCase::execute({}, port);

    QVERIFY(created);
    bool idOk = false;
    const int classId = QString::fromStdString(created.value().value()).toInt(&idOk);
    QVERIFY(idOk);
    QVERIFY(classId > 0);
    QCOMPARE(classCount(services), 1);
    const auto classroom = services.workspaceStore()->classRepository()
        ->getClassById(classId);
    QVERIFY(classroom);
    QVERIFY(classroom->name.isEmpty());
}

void NextPlatformApplicationServicesClassCreatePortTests::
rejectsClassIdBeyondIntRangeWithoutRemovingInsertedClass()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(executeSql(
        services,
        QStringLiteral("INSERT INTO classes (name) VALUES ('seed')")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "UPDATE sqlite_sequence SET seq = 4294967296 "
            "WHERE name = 'classes'")
        ));

    Platform::ApplicationServicesClassCreatePort port(services);
    const auto created = Application::ClassCreateUseCase::execute({}, port);

    QVERIFY(!created);
    QCOMPARE(created.error().code, Domain::ErrorCode::Validation);
    QVERIFY(!created.error().recoverable);
    QVERIFY(QString::fromStdString(created.error().message)
        .contains(QStringLiteral("4294967297")));
    QCOMPARE(classCount(services), 2);

    QSqlQuery query(services.workspaceStore()->database());
    QVERIFY(query.exec(QStringLiteral(R"(
        SELECT typeof(id), id
        FROM classes
        WHERE id = 4294967297
    )")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(query.value(1).toLongLong(), qint64{4294967297LL});

    QVERIFY(query.exec(QStringLiteral(R"(
        SELECT typeof(id), id
        FROM classes
        WHERE id = 1
    )")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(query.value(1).toLongLong(), qint64{1});
}

void NextPlatformApplicationServicesClassCreatePortTests::
unavailableAndClosedSessionsReturnStructuredFailures()
{
    {
        Platform::ApplicationServicesClassCreatePort nullPort(
            static_cast<ApplicationServices*>(nullptr)
            );
        const auto result =
            Application::ClassCreateUseCase::execute({}, nullPort);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    }

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    const QString path = databasePath(directory);
    Platform::ApplicationServicesClassCreatePort port(services);
    const auto unopened =
        Application::ClassCreateUseCase::execute({}, port);
    QVERIFY(!unopened);
    QCOMPARE(unopened.error().code, Domain::ErrorCode::NotFound);

    QVERIFY(services.openDatabase(path));
    services.closeDatabase();
    const auto closed = Application::ClassCreateUseCase::execute({}, port);
    QVERIFY(!closed);
    QCOMPARE(closed.error().code, Domain::ErrorCode::NotFound);

    QVERIFY(services.openDatabase(path));
    QCOMPARE(classCount(services), 0);
}

void NextPlatformApplicationServicesClassCreatePortTests::
repositoryFailureDoesNotCreateAClass()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER reject_initial_class_create
            BEFORE INSERT ON classes
            BEGIN
                SELECT RAISE(ABORT, 'forced class create failure');
            END
        )")
        ));

    Platform::ApplicationServicesClassCreatePort port(services);
    const auto result = Application::ClassCreateUseCase::execute({}, port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QCOMPARE(classCount(services), 0);
}

QTEST_MAIN(NextPlatformApplicationServicesClassCreatePortTests)

#include "next_platform_application_services_class_create_port_tests.moc"
