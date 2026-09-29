#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/schedule_testing_class_choices_query.h"
#include "next/platform/application_services_schedule_testing_class_choices_read_port.h"

#include <QSqlDatabase>
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
        QStringLiteral("testing-class-choices-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

TestingClass testingClass(
    const QString& name,
    const QString& grade,
    const QString& level,
    const QString& room
    )
{
    TestingClass value;
    value.name = name;
    value.grade = grade;
    value.level = level;
    value.room = room;
    return value;
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

}

class NextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsOrderedCompactChoicesFromTheActiveSession();
    void preservesSuccessfulEmptyRepositoryResults();
    void treatsUnavailableSessionsAsNotFoundWithoutFallback();
    void forwardsTestingClassRepositoryFailures();
};

void NextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests::
readsOrderedCompactChoicesFromTheActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    TestingClassRepository* const repository =
        session->testingClassRepository();
    QVERIFY(repository);

    const auto createdM2 = repository->createTestingClass(testingClass(
        QStringLiteral("Zulu M2 Oral"),
        QStringLiteral("M2"),
        QStringLiteral("A Level"),
        QStringLiteral("Library")
        ));
    QVERIFY(createdM2);
    const auto createdE5Upper = repository->createTestingClass(testingClass(
        QStringLiteral("Beta E5 Writing"),
        QStringLiteral("E5"),
        QStringLiteral("Upper"),
        QStringLiteral("Room 204")
        ));
    QVERIFY(createdE5Upper);
    const auto createdE5Lower = repository->createTestingClass(testingClass(
        QStringLiteral("\uC544\uB974\uCE74 E5 Oral"),
        QStringLiteral("E5"),
        QStringLiteral("Lower"),
        QStringLiteral("\uC2E4\uD5D8\uC2E4 1")
        ));
    QVERIFY(createdE5Lower);

    Platform::ApplicationServicesScheduleTestingClassChoicesReadPort port(
        services
        );
    const auto result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(result);
    QCOMPARE(result.value().choices.size(), std::size_t(3));
    QCOMPARE(result.value().choices[0].classId, classId(*createdE5Lower));
    QCOMPARE(result.value().choices[0].name,
             std::u16string(u"\uC544\uB974\uCE74 E5 Oral"));
    QCOMPARE(result.value().choices[0].grade, std::u16string(u"E5"));
    QCOMPARE(result.value().choices[0].level, std::u16string(u"Lower"));
    QCOMPARE(result.value().choices[0].room,
             std::u16string(u"\uC2E4\uD5D8\uC2E4 1"));
    QCOMPARE(result.value().choices[1].classId, classId(*createdE5Upper));
    QCOMPARE(result.value().choices[1].name,
             std::u16string(u"Beta E5 Writing"));
    QCOMPARE(result.value().choices[1].grade, std::u16string(u"E5"));
    QCOMPARE(result.value().choices[1].level, std::u16string(u"Upper"));
    QCOMPARE(result.value().choices[1].room, std::u16string(u"Room 204"));
    QCOMPARE(result.value().choices[2].classId, classId(*createdM2));
    QCOMPARE(result.value().choices[2].name,
             std::u16string(u"Zulu M2 Oral"));
    QCOMPARE(result.value().choices[2].grade, std::u16string(u"M2"));
    QCOMPARE(result.value().choices[2].level, std::u16string(u"A Level"));
    QCOMPARE(result.value().choices[2].room, std::u16string(u"Library"));
}

void NextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests::
preservesSuccessfulEmptyRepositoryResults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesScheduleTestingClassChoicesReadPort port(
        services
        );

    const auto result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(result);
    QVERIFY(result.value().choices.empty());
}

void NextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests::
treatsUnavailableSessionsAsNotFoundWithoutFallback()
{
    Platform::ApplicationServicesScheduleTestingClassChoicesReadPort
        nullServicesPort(static_cast<ApplicationServices*>(nullptr));
    auto result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            {},
            nullServicesPort
            );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);

    ApplicationServices unopenedServices;
    Platform::ApplicationServicesScheduleTestingClassChoicesReadPort
        unopenedPort(unopenedServices);
    result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            {},
            unopenedPort
            );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    closedServices.closeDatabase();
    Platform::ApplicationServicesScheduleTestingClassChoicesReadPort
        closedPort(closedServices);
    result = Application::
        ScheduleTestingClassChoicesReadQueryHandler::execute({}, closedPort);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests::
forwardsTestingClassRepositoryFailures()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery dropTestingClasses(services.databaseSession()->database());
    QVERIFY(dropTestingClasses.exec(QStringLiteral(
        "DROP TABLE testing_classes"
        )));

    Platform::ApplicationServicesScheduleTestingClassChoicesReadPort port(
        services
        );
    const auto result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().message.find("Loading testing classes")
            != std::string::npos);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests
    )

#include "next_platform_application_services_schedule_testing_class_choices_read_port_tests.moc"
