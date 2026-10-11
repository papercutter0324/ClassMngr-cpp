#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "next/application/class_by_id_read_query.h"
#include "next/platform/application_services_class_by_id_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-by-id-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

int createClass(ApplicationServices& services, const QString& name)
{
    return services.databaseSession()->classRepository()
        ->createClass(name).value_or(-1);
}

}

class NextPlatformApplicationServicesClassByIdReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsClassIdAndNameFromActiveSession();
    void reportsMissingUnavailableAndRepositoryFailures();
};

void NextPlatformApplicationServicesClassByIdReadPortTests::
readsClassIdAndNameFromActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int id = createClass(services, QStringLiteral("Orion"));
    QVERIFY(id > 0);

    Platform::ApplicationServicesClassByIdReadPort port(&services);
    const Application::ClassByIdReadQuery query(port);
    const auto result = query.execute(classId(id));

    QVERIFY(result);
    QCOMPARE(result.value().classId.value(), std::to_string(id));
    QVERIFY(result.value().className == u"Orion");
}

void NextPlatformApplicationServicesClassByIdReadPortTests::
reportsMissingUnavailableAndRepositoryFailures()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassByIdReadPort port(&services);

    auto result = port.readClassById(classId(1));
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);

    const auto nonCanonical = Domain::ClassId::fromString("01");
    QVERIFY(nonCanonical);
    result = port.readClassById(*nonCanonical);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int id = createClass(services, QStringLiteral("Orion"));
    QVERIFY(id > 0);

    const auto missing = port.readClassById(classId(id + 1000));
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, Domain::ErrorCode::Technical);
    QVERIFY(missing.error().message.find("no matching record")
        != std::string::npos);
    QVERIFY(!missing.error().recoverable);

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE classes")),
        qPrintable(query.lastError().text()));
    const auto repositoryFailure = port.readClassById(classId(id));
    QVERIFY(!repositoryFailure);
    QCOMPARE(repositoryFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!repositoryFailure.error().message.empty());

    services.closeDatabase();
    result = port.readClassById(classId(id));
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
}

QTEST_MAIN(NextPlatformApplicationServicesClassByIdReadPortTests)

#include "next_platform_application_services_class_by_id_read_port_tests.moc"
