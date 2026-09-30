#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "next/application/classes_list_read_query.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_classes_list_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <type_traits>

using namespace ClassMngr::Next;

static_assert(!std::is_same_v<Domain::ClassId, Domain::TeacherId>);
static_assert(!std::is_convertible_v<Domain::ClassId, Domain::TeacherId>);

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("classes-list-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClass(ApplicationServices& services, const QString& name)
{
    return services.databaseSession()->classRepository()
        ->createClass(name).value_or(-1);
}

}

class NextPlatformApplicationServicesClassesListReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void mapsRepositoryOrderTypedIdsAndNames();
    void preservesEmptyRepositoryResult();
    void unavailableSessionAndRepositoryFailureDoNotFallBack();
};

void NextPlatformApplicationServicesClassesListReadPortTests::
mapsRepositoryOrderTypedIdsAndNames()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int zuluId = createClass(services, QStringLiteral("Zulu"));
    const int koreanId = createClass(
        services,
        QStringLiteral("\uAC00\uB098\uB2E4")
        );
    const int alphaId = createClass(services, QStringLiteral("Alpha"));
    QVERIFY(zuluId > 0);
    QVERIFY(koreanId > 0);
    QVERIFY(alphaId > 0);

    const Result<QList<Classroom>> legacyResult =
        services.classService()->classes();
    QVERIFY(legacyResult);

    Platform::ApplicationServicesClassesListReadPort port(&services);
    const Application::ClassesListReadQuery query(port);
    const auto result = query.execute();

    QVERIFY(result);
    QCOMPARE(result.value().classes.size(),
        static_cast<std::size_t>(legacyResult->size()));
    for (std::size_t index = 0; index < result.value().classes.size(); ++index)
    {
        const Classroom& legacy = legacyResult->at(
            static_cast<qsizetype>(index)
            );
        const auto& typed = result.value().classes.at(index);
        QCOMPARE(typed.classId.value(), std::to_string(legacy.id));
        QCOMPARE(typed.className, legacy.name.toStdU16String());
    }
    QCOMPARE(result.value().classes.size(), std::size_t(3));
    QCOMPARE(result.value().classes.at(0).className,
        std::u16string(u"Alpha"));
    QVERIFY(!result.value().classes.at(0).classId.value().empty());
    QCOMPARE(result.value().classes.at(1).className,
        std::u16string(u"Zulu"));
    QCOMPARE(result.value().classes.at(2).className,
        std::u16string(u"\uAC00\uB098\uB2E4"));
}

void NextPlatformApplicationServicesClassesListReadPortTests::
preservesEmptyRepositoryResult()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Platform::ApplicationServicesClassesListReadPort port(&services);
    const auto result = port.readClassesList();

    QVERIFY(result);
    QVERIFY(result.value().classes.empty());
}

void NextPlatformApplicationServicesClassesListReadPortTests::
unavailableSessionAndRepositoryFailureDoNotFallBack()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());

    Platform::ApplicationServicesClassesListReadPort port(&services);
    auto unavailable = port.readClassesList();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(unavailable.error().recoverable);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE classes")),
        qPrintable(query.lastError().text()));

    const auto repositoryFailure = port.readClassesList();
    QVERIFY(!repositoryFailure);
    QCOMPARE(repositoryFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!repositoryFailure.error().message.empty());
    QVERIFY(!repositoryFailure.error().recoverable);

    services.closeDatabase();
    unavailable = port.readClassesList();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
}

QTEST_MAIN(NextPlatformApplicationServicesClassesListReadPortTests)

#include "next_platform_application_services_classes_list_read_port_tests.moc"
