#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/schedule_testing_class_choices_query.h"
#include "next/platform/application_services_schedule_testing_class_choices_read_port.h"

#include <QSqlError>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <limits>
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
    void rejectsTestingClassIdsOutsidePositiveIntRange();
    void preservesMaximumPositiveTestingClassId();
    void keepsTestingClassesWithoutClassInfoWithBlankGradeAndLevel();
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
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    TestingClassRepository* const repository =
        store->testingClassRepository();
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

    const TestingClassChoicesReadMetrics before =
        repository->testingClassChoicesReadMetrics();

    Platform::ApplicationServicesScheduleTestingClassChoicesReadPort port(
        services
        );
    const auto result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            {},
            port
            );
    const TestingClassChoicesReadMetrics after =
        repository->testingClassChoicesReadMetrics();

    QVERIFY(result);
    QCOMPARE(after.callCount - before.callCount, 1);
    QCOMPARE(after.statementCount - before.statementCount, 1);
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
rejectsTestingClassIdsOutsidePositiveIntRange()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QSqlDatabase database = store->database();

    const qint64 classIds[] = { 1, 4294967297LL };
    const QString classNames[] = {
        QStringLiteral("Narrow ID"),
        QStringLiteral("Wide ID")
    };
    const QString classRooms[] = {
        QStringLiteral("Room 1"),
        QStringLiteral("Room 2")
    };
    for (int index = 0; index < 2; ++index)
    {
        QSqlQuery insertClass(database);
        QVERIFY(insertClass.prepare(QStringLiteral(
            "INSERT INTO classes (id, name) VALUES (?, ?)"
            )));
        insertClass.addBindValue(QVariant::fromValue(classIds[index]));
        insertClass.addBindValue(classNames[index]);
        QVERIFY2(insertClass.exec(), qPrintable(insertClass.lastError().text()));

        QSqlQuery markTestingClass(database);
        QVERIFY(markTestingClass.prepare(QStringLiteral(
            "INSERT INTO testing_classes (class_id, room) VALUES (?, ?)"
            )));
        markTestingClass.addBindValue(QVariant::fromValue(classIds[index]));
        markTestingClass.addBindValue(classRooms[index]);
        QVERIFY2(
            markTestingClass.exec(),
            qPrintable(markTestingClass.lastError().text())
            );
    }

    QSqlQuery persistedIds(database);
    QVERIFY(persistedIds.exec(QStringLiteral(
        "SELECT id, name, typeof(id) AS storage_type "
        "FROM classes ORDER BY id"
        )));
    for (int index = 0; index < 2; ++index)
    {
        QVERIFY(persistedIds.next());
        bool idOk = false;
        QCOMPARE(persistedIds.value(QStringLiteral("id")).toLongLong(&idOk),
                 classIds[index]);
        QVERIFY(idOk);
        QCOMPARE(persistedIds.value(QStringLiteral("name")).toString(),
                 classNames[index]);
        QCOMPARE(persistedIds.value(QStringLiteral("storage_type")).toString(),
                 QStringLiteral("integer"));
    }
    QVERIFY(!persistedIds.next());

    Platform::ApplicationServicesScheduleTestingClassChoicesReadPort port(
        services
        );
    const auto result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QVERIFY(result.error().recoverable);
    QVERIFY(result.error().message.find("Loading testing classes")
            != std::string::npos);
    QVERIFY(result.error().message.find("invalid class identifier")
            != std::string::npos);

    QVERIFY(persistedIds.exec(QStringLiteral(
        "SELECT id, name, typeof(id) AS storage_type "
        "FROM classes ORDER BY id"
        )));
    for (int index = 0; index < 2; ++index)
    {
        QVERIFY(persistedIds.next());
        bool idOk = false;
        QCOMPARE(persistedIds.value(QStringLiteral("id")).toLongLong(&idOk),
                 classIds[index]);
        QVERIFY(idOk);
        QCOMPARE(persistedIds.value(QStringLiteral("name")).toString(),
                 classNames[index]);
        QCOMPARE(persistedIds.value(QStringLiteral("storage_type")).toString(),
                 QStringLiteral("integer"));
    }
    QVERIFY(!persistedIds.next());

    QSqlQuery persistedTestingIds(database);
    QVERIFY(persistedTestingIds.exec(QStringLiteral(
        "SELECT class_id, room, typeof(class_id) AS storage_type "
        "FROM testing_classes ORDER BY class_id"
        )));
    for (int index = 0; index < 2; ++index)
    {
        QVERIFY(persistedTestingIds.next());
        bool idOk = false;
        QCOMPARE(
            persistedTestingIds.value(QStringLiteral("class_id"))
                .toLongLong(&idOk),
            classIds[index]
            );
        QVERIFY(idOk);
        QCOMPARE(
            persistedTestingIds.value(QStringLiteral("room")).toString(),
            classRooms[index]
            );
        QCOMPARE(
            persistedTestingIds.value(QStringLiteral("storage_type"))
                .toString(),
            QStringLiteral("integer")
            );
    }
    QVERIFY(!persistedTestingIds.next());
}

void NextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests::
preservesMaximumPositiveTestingClassId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    QSqlDatabase database = store->database();

    const qint64 maximumClassId = std::numeric_limits<int>::max();
    QSqlQuery insertClass(database);
    QVERIFY(insertClass.prepare(QStringLiteral(
        "INSERT INTO classes (id, name) VALUES (?, ?)"
        )));
    insertClass.addBindValue(QVariant::fromValue(maximumClassId));
    insertClass.addBindValue(QStringLiteral("Maximum Positive ID"));
    QVERIFY2(insertClass.exec(), qPrintable(insertClass.lastError().text()));

    QSqlQuery markTestingClass(database);
    QVERIFY(markTestingClass.prepare(QStringLiteral(
        "INSERT INTO testing_classes (class_id, room) VALUES (?, ?)"
        )));
    markTestingClass.addBindValue(QVariant::fromValue(maximumClassId));
    markTestingClass.addBindValue(QStringLiteral("Top Room"));
    QVERIFY2(
        markTestingClass.exec(),
        qPrintable(markTestingClass.lastError().text())
        );

    Platform::ApplicationServicesScheduleTestingClassChoicesReadPort port(
        services
        );
    const auto result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(result);
    QCOMPARE(result.value().choices.size(), std::size_t(1));
    QCOMPARE(
        result.value().choices[0].classId,
        classId(std::numeric_limits<int>::max())
        );
    QCOMPARE(
        result.value().choices[0].name,
        std::u16string(u"Maximum Positive ID")
        );
    QCOMPARE(result.value().choices[0].room, std::u16string(u"Top Room"));
}

void NextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests::
keepsTestingClassesWithoutClassInfoWithBlankGradeAndLevel()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    WorkspaceStore* const store = services.workspaceStore();
    QVERIFY(store);
    TestingClassRepository* const repository =
        store->testingClassRepository();
    QVERIFY(repository);

    const auto created = repository->createTestingClass(testingClass(
        QStringLiteral("Class Info Removed"),
        QStringLiteral("M1"),
        QStringLiteral("Major"),
        QStringLiteral("Retained Room")
        ));
    QVERIFY(created);

    QSqlQuery removeClassInfo(store->database());
    removeClassInfo.prepare(QStringLiteral(
        "DELETE FROM class_info WHERE class_id=?"
        ));
    removeClassInfo.addBindValue(*created);
    QVERIFY2(
        removeClassInfo.exec(),
        qPrintable(removeClassInfo.lastError().text())
        );

    Platform::ApplicationServicesScheduleTestingClassChoicesReadPort port(
        services
        );
    const auto result =
        Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(result);
    QCOMPARE(result.value().choices.size(), std::size_t(1));
    QCOMPARE(result.value().choices[0].classId, classId(*created));
    QCOMPARE(
        result.value().choices[0].name,
        std::u16string(u"Class Info Removed")
        );
    QVERIFY(result.value().choices[0].grade.empty());
    QVERIFY(result.value().choices[0].level.empty());
    QCOMPARE(
        result.value().choices[0].room,
        std::u16string(u"Retained Room")
        );
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
    QSqlQuery dropTestingClasses(services.workspaceStore()->database());
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
