#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/testing_class_details_read_query.h"
#include "next/platform/application_services_testing_class_details_read_port.h"

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
        QStringLiteral("testing-class-details-%1.tps").arg(
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

Domain::TeacherId teacherId(const int value)
{
    const auto parsed = Domain::TeacherId::fromString(std::to_string(value));
    if (!parsed)
    {
        qFatal("Database teacher ID must have a typed representation.");
    }
    return *parsed;
}

Application::TestingClassDetailsReadResult readDetails(
    ApplicationServices& services,
    const int id
    )
{
    Platform::ApplicationServicesTestingClassDetailsReadPort port(services);
    return Application::TestingClassDetailsReadQueryHandler::execute(
        {.classId = classId(id)},
        port
        );
}

TestingClass testingClass(const QString& name)
{
    TestingClass value;
    value.name = name;
    value.grade = QStringLiteral("M2");
    value.level = QStringLiteral("Mixed (High)");
    value.room = QStringLiteral("Room 204");
    value.classColor = QStringLiteral("#123456");
    value.fontColor = QStringLiteral("#FEDCBA");
    value.notes = QStringLiteral("Original notes \uD14C\uC2A4\uD2B8");
    return value;
}

void verifyNotFound(
    const Application::TestingClassDetailsReadResult& result
    )
{
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!result.error().message.empty());
}

}

class NextPlatformApplicationServicesTestingClassDetailsReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void mapsEveryFieldFromTheActiveSessionRepository();
    void mapsAnAbsentTeacherAsAnEmptyOptional();
    void mapsStoredZeroTeacherIdAsUnassigned();
    void reportsUnavailableSessionsAsNotFound();
    void reportsMissingTestingClassesAsWarningsWithoutFallback();
    void forwardsRepositoryFailuresAsNonNotFoundErrors();
    void rejectsNonCanonicalLegacyIdentifiers();
};

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
mapsEveryFieldFromTheActiveSessionRepository()
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

    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD\uB2D8");
    teacher.teacherEn = QStringLiteral("Test Teacher");
    teacher.roomNumber = QStringLiteral("Teacher Room");
    const auto createdTeacher = session->teacherRepository()->createTeacher(
        teacher
        );
    QVERIFY(createdTeacher);

    TestingClass value = testingClass(
        QStringLiteral("Writing Lab \uC2E4\uD5D8")
        );
    value.grade = QStringLiteral("E5");
    value.level = QStringLiteral("Song's");
    value.room = QStringLiteral("Library \uC2E4\uD5D8\uC2E4");
    value.teacherId = *createdTeacher;
    value.notes = QStringLiteral("Keep exact notes \uC0C1\uC138 \uC8FC\uC758");
    const auto createdClass = repository->createTestingClass(value);
    QVERIFY(createdClass);

    const auto result = readDetails(services, *createdClass);

    QVERIFY(result);
    const Application::TestingClassDetailsSnapshot expected{
        .classId = classId(*createdClass),
        .name = u"Writing Lab \uC2E4\uD5D8",
        .grade = u"E5",
        .level = u"Song's",
        .room = u"Library \uC2E4\uD5D8\uC2E4",
        .teacherId = teacherId(*createdTeacher),
        .classColor = u"#123456",
        .fontColor = u"#FEDCBA",
        .notes = u"Keep exact notes \uC0C1\uC138 \uC8FC\uC758"
    };
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().classId, classId(*createdClass));
    QCOMPARE(result.value().name, std::u16string(u"Writing Lab \uC2E4\uD5D8"));
    QCOMPARE(result.value().grade, std::u16string(u"E5"));
    QCOMPARE(result.value().level, std::u16string(u"Song's"));
    QCOMPARE(
        result.value().room,
        std::u16string(u"Library \uC2E4\uD5D8\uC2E4")
        );
    QVERIFY(result.value().teacherId.has_value());
    QCOMPARE(result.value().teacherId.value(), teacherId(*createdTeacher));
    QCOMPARE(result.value().classColor, std::u16string(u"#123456"));
    QCOMPARE(result.value().fontColor, std::u16string(u"#FEDCBA"));
    QCOMPARE(
        result.value().notes,
        std::u16string(u"Keep exact notes \uC0C1\uC138 \uC8FC\uC758")
        );
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
mapsAnAbsentTeacherAsAnEmptyOptional()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClassRepository* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        testingClass(QStringLiteral("No Teacher Class"))
        );
    QVERIFY(createdClass);

    const auto result = readDetails(services, *createdClass);

    QVERIFY(result);
    QVERIFY(!result.value().teacherId.has_value());
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
mapsStoredZeroTeacherIdAsUnassigned()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClassRepository* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        testingClass(QStringLiteral("Zero Teacher ID Class"))
        );
    QVERIFY(createdClass);

    QSqlQuery addLegacyZeroTeacher(
        services.databaseSession()->database()
        );
    QVERIFY(addLegacyZeroTeacher.exec(QStringLiteral(
        "INSERT INTO teachers (id, teacher_en) "
        "VALUES (0, 'Legacy Unassigned')"
        )));

    QSqlQuery setLegacyZeroTeacherId(
        services.databaseSession()->database()
        );
    setLegacyZeroTeacherId.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=0 WHERE class_id=?"
        ));
    setLegacyZeroTeacherId.addBindValue(*createdClass);
    QVERIFY2(setLegacyZeroTeacherId.exec(),
             qPrintable(setLegacyZeroTeacherId.lastError().text()));

    const auto legacyRead = repository->loadTestingClass(*createdClass);
    QVERIFY(legacyRead);
    QCOMPARE(legacyRead->teacherId, 0);

    const auto result = readDetails(services, *createdClass);

    QVERIFY(result);
    QVERIFY(!result.value().teacherId.has_value());
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
reportsUnavailableSessionsAsNotFound()
{
    Platform::ApplicationServicesTestingClassDetailsReadPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullResult =
        Application::TestingClassDetailsReadQueryHandler::execute(
            {.classId = classId(42)},
            nullPort
            );
    verifyNotFound(nullResult);

    ApplicationServices unopenedServices;
    Platform::ApplicationServicesTestingClassDetailsReadPort unopenedPort(
        unopenedServices
        );
    const auto unopenedResult =
        Application::TestingClassDetailsReadQueryHandler::execute(
            {.classId = classId(42)},
            unopenedPort
            );
    verifyNotFound(unopenedResult);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    closedServices.closeDatabase();
    Platform::ApplicationServicesTestingClassDetailsReadPort closedPort(
        closedServices
        );
    const auto closedResult =
        Application::TestingClassDetailsReadQueryHandler::execute(
            {.classId = classId(42)},
            closedPort
            );
    verifyNotFound(closedResult);
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
reportsMissingTestingClassesAsWarningsWithoutFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClassRepository* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        testingClass(QStringLiteral("Testing Class Removed From Roster"))
        );
    QVERIFY(createdClass);

    QSqlQuery removeTestingClass(
        services.databaseSession()->database()
        );
    removeTestingClass.prepare(QStringLiteral(
        "DELETE FROM testing_classes WHERE class_id=?"
        ));
    removeTestingClass.addBindValue(*createdClass);
    QVERIFY(removeTestingClass.exec());

    const auto result = readDetails(services, *createdClass);

    QVERIFY(!result);
    QVERIFY(result.error().code != Domain::ErrorCode::NotFound);
    const QString errorText = QString::fromStdString(result.error().message);
    QVERIFY(errorText.contains(QStringLiteral("not found"), Qt::CaseInsensitive));
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
forwardsRepositoryFailuresAsNonNotFoundErrors()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClassRepository* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        testingClass(QStringLiteral("Repository Failure Source"))
        );
    QVERIFY(createdClass);

    QSqlQuery dropTestingClasses(
        services.databaseSession()->database()
        );
    QVERIFY(dropTestingClasses.exec(QStringLiteral(
        "DROP TABLE testing_classes"
        )));

    const auto result = readDetails(services, *createdClass);

    QVERIFY(!result);
    QVERIFY(result.error().code != Domain::ErrorCode::NotFound);
    QVERIFY(!result.error().message.empty());
}

void NextPlatformApplicationServicesTestingClassDetailsReadPortTests::
rejectsNonCanonicalLegacyIdentifiers()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesTestingClassDetailsReadPort port(services);

    const auto result =
        Application::TestingClassDetailsReadQueryHandler::execute(
            {
                .classId = *Domain::ClassId::fromString("042")
            },
            port
            );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesTestingClassDetailsReadPortTests
    )

#include "next_platform_application_services_testing_class_details_read_port_tests.moc"
