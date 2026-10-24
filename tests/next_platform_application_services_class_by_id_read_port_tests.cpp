#include "core/application_services.h"
#include "data/database/workspace_store.h"
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
    return services.workspaceStore()->classRepository()
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

    ClassRepository* const repository =
        services.workspaceStore()->classRepository();
    const auto lookup = repository->lookupClassById(id);
    QCOMPARE(
        lookup.code,
        ClassRepository::ClassByIdLookupCode::Found
        );
    QCOMPARE(lookup.classroom.id, id);
    QCOMPARE(lookup.classroom.name, QStringLiteral("Orion"));

    const Result<Classroom> legacy = repository->getClassById(id);
    QVERIFY(legacy);
    QCOMPARE(legacy->id, id);
    QCOMPARE(legacy->name, QStringLiteral("Orion"));

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
    QCOMPARE(
        result.error().message,
        std::string("The active database session for classes is unavailable.")
        );
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

    ClassRepository* const repository =
        services.workspaceStore()->classRepository();
    const int missingId = id + 1000;
    const QString missingDiagnostic = QObject::tr(
        "Loading class failed for %1: no matching record exists."
        ).arg(QObject::tr("class id %1").arg(missingId));
    const auto missingLookup = repository->lookupClassById(missingId);
    QCOMPARE(
        missingLookup.code,
        ClassRepository::ClassByIdLookupCode::Missing
        );
    QCOMPARE(missingLookup.diagnostic, missingDiagnostic);

    const Result<Classroom> legacyMissing =
        repository->getClassById(missingId);
    QVERIFY(!legacyMissing);
    QCOMPARE(legacyMissing.error(), missingDiagnostic);

    const auto missing = port.readClassById(classId(missingId));
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(missing.error().message, missingDiagnostic.toStdString());
    QVERIFY(missing.error().recoverable);

    QSqlQuery query(services.workspaceStore()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE classes")),
        qPrintable(query.lastError().text()));
    const auto failedLookup = repository->lookupClassById(id);
    QCOMPARE(
        failedLookup.code,
        ClassRepository::ClassByIdLookupCode::Failure
        );
    QVERIFY(!failedLookup.diagnostic.isEmpty());

    const auto repositoryFailure = port.readClassById(classId(id));
    QVERIFY(!repositoryFailure);
    QCOMPARE(repositoryFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!repositoryFailure.error().message.empty());

    services.closeDatabase();
    result = port.readClassById(classId(id));
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(
        result.error().message,
        std::string("The active database session for classes is unavailable.")
        );
    QVERIFY(result.error().recoverable);
}

QTEST_MAIN(NextPlatformApplicationServicesClassByIdReadPortTests)

#include "next_platform_application_services_class_by_id_read_port_tests.moc"
