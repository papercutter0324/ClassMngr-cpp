#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/testing_class_details_update_use_case.h"
#include "next/platform/application_services_testing_class_details_update_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("testing-class-details-update-%1.tps").arg(
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

TestingClass storedClass(const QString& name)
{
    TestingClass value;
    value.name = name;
    value.grade = QStringLiteral("M1");
    value.level = QStringLiteral("Major");
    value.room = QStringLiteral("Original Room");
    value.teacherId = -1;
    value.classColor = QStringLiteral("#010203");
    value.fontColor = QStringLiteral("#A0B0C0");
    value.notes = QStringLiteral("Original notes");
    return value;
}

Application::TestingClassDetailsUpdateRequest requestForClass(
    const int id,
    const std::optional<int>& teacher = std::optional<int>(17)
    )
{
    return {
        .classId = classId(id),
        .name = u"Updated Testing Lab 실험",
        .grade = u"E5",
        .level = u"Song's",
        .room = u"Library 204",
        .teacherId = teacher
            ? std::optional<Domain::TeacherId>(teacherId(*teacher))
            : std::nullopt,
        .classColor = u"#123456",
        .fontColor = u"#FEDCBA",
        .notes = u"Updated notes 상세"
    };
}

Application::TestingClassDetailsUpdateResult update(
    ApplicationServices& services,
    const Application::TestingClassDetailsUpdateRequest& request
    )
{
    Platform::ApplicationServicesTestingClassDetailsUpdatePort port(services);
    return Application::TestingClassDetailsUpdateUseCase::execute(
        request,
        port
        );
}

int createTeacher(TeacherRepository& repository, const QString& name)
{
    Teacher teacher;
    teacher.teacherEn = name;
    teacher.preferredName = name;
    const auto created = repository.createTeacher(teacher);
    return created ? *created : -1;
}

}

class NextPlatformApplicationServicesTestingClassDetailsUpdatePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void updatesEveryFieldThroughTheActiveSessionRepository();
    void reportsUnavailableSessionsAsNotFound();
    void rejectsAnOrdinaryClassWithoutLegacyFallback();
    void forwardsRepositoryFailuresAsTechnicalErrors();
    void rejectsNoncanonicalIdentifiers();
};

void NextPlatformApplicationServicesTestingClassDetailsUpdatePortTests::
updatesEveryFieldThroughTheActiveSessionRepository()
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
    TeacherRepository* const teacherRepository =
        session->teacherRepository();
    QVERIFY(teacherRepository);

    const int firstTeacher = createTeacher(
        *teacherRepository,
        QStringLiteral("Original Teacher")
        );
    const int updatedTeacher = createTeacher(
        *teacherRepository,
        QStringLiteral("Updated Teacher")
        );
    QVERIFY(firstTeacher > 0);
    QVERIFY(updatedTeacher > 0);

    TestingClass original = storedClass(QStringLiteral("Original Name"));
    original.grade = QStringLiteral("M1");
    original.level = QStringLiteral("Major");
    original.room = QStringLiteral("Old Room");
    original.teacherId = firstTeacher;
    original.classColor = QStringLiteral("#010203");
    original.fontColor = QStringLiteral("#A0B0C0");
    original.notes = QStringLiteral("Old notes");
    const auto createdClass = repository->createTestingClass(original);
    QVERIFY(createdClass);

    const auto request = requestForClass(*createdClass, updatedTeacher);
    const auto result = update(services, request);

    QVERIFY(result);
    auto saved = repository->loadTestingClass(*createdClass);
    QVERIFY(saved);
    QCOMPARE(saved->classId, *createdClass);
    QCOMPARE(saved->name, QStringLiteral("Updated Testing Lab 실험"));
    QCOMPARE(saved->grade, QStringLiteral("E5"));
    QCOMPARE(saved->level, QStringLiteral("Song's"));
    QCOMPARE(saved->room, QStringLiteral("Library 204"));
    QCOMPARE(saved->teacherId, updatedTeacher);
    QCOMPARE(saved->classColor, QStringLiteral("#123456"));
    QCOMPARE(saved->fontColor, QStringLiteral("#FEDCBA"));
    QCOMPARE(saved->notes, QStringLiteral("Updated notes 상세"));

    auto unassignedRequest = requestForClass(*createdClass, std::nullopt);
    unassignedRequest.name = u"Unassigned Testing Lab";
    const auto unassignedResult = update(services, unassignedRequest);
    QVERIFY(unassignedResult);
    saved = repository->loadTestingClass(*createdClass);
    QVERIFY(saved);
    QCOMPARE(saved->name, QStringLiteral("Unassigned Testing Lab"));
    QCOMPARE(saved->teacherId, -1);
}

void NextPlatformApplicationServicesTestingClassDetailsUpdatePortTests::
reportsUnavailableSessionsAsNotFound()
{
    const auto request = requestForClass(42);

    Platform::ApplicationServicesTestingClassDetailsUpdatePort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullResult =
        Application::TestingClassDetailsUpdateUseCase::execute(
            request,
            nullPort
            );
    QVERIFY(!nullResult);
    QCOMPARE(nullResult.error().code, Domain::ErrorCode::NotFound);

    ApplicationServices unopenedServices;
    Platform::ApplicationServicesTestingClassDetailsUpdatePort unopenedPort(
        unopenedServices
        );
    const auto unopenedResult =
        Application::TestingClassDetailsUpdateUseCase::execute(
            request,
            unopenedPort
            );
    QVERIFY(!unopenedResult);
    QCOMPARE(unopenedResult.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices closedServices;
    QVERIFY(closedServices.openDatabase(databasePath(directory)));
    closedServices.closeDatabase();
    Platform::ApplicationServicesTestingClassDetailsUpdatePort closedPort(
        closedServices
        );
    const auto closedResult =
        Application::TestingClassDetailsUpdateUseCase::execute(
            request,
            closedPort
            );
    QVERIFY(!closedResult);
    QCOMPARE(closedResult.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesTestingClassDetailsUpdatePortTests::
rejectsAnOrdinaryClassWithoutLegacyFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Ordinary Class Must Not Be Updated")
        );
    QVERIFY(createdClass);
    auto originalInfo = services.classService()->classInfo(*createdClass);
    QVERIFY(originalInfo);
    originalInfo->classGrade = QStringLiteral("E4");
    originalInfo->classLevel = QStringLiteral("Theseus");
    originalInfo->classColor = QStringLiteral("#112233");
    originalInfo->fontColor = QStringLiteral("#445566");
    originalInfo->notes = QStringLiteral("Preserve ordinary class details");
    QVERIFY(services.classService()->saveClassInfo(*originalInfo));

    QSqlQuery classNameQuery(services.databaseSession()->database());
    classNameQuery.prepare(QStringLiteral("SELECT name FROM classes WHERE id=?"));
    classNameQuery.addBindValue(*createdClass);
    QVERIFY2(classNameQuery.exec(), qPrintable(classNameQuery.lastError().text()));
    QVERIFY(classNameQuery.next());
    const QString originalName = classNameQuery.value(0).toString();

    const auto result = update(
        services,
        requestForClass(*createdClass)
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const auto unchangedInfo = services.classService()->classInfo(*createdClass);
    QVERIFY(unchangedInfo);
    QCOMPARE(unchangedInfo->classGrade, originalInfo->classGrade);
    QCOMPARE(unchangedInfo->classLevel, originalInfo->classLevel);
    QCOMPARE(unchangedInfo->classColor, originalInfo->classColor);
    QCOMPARE(unchangedInfo->fontColor, originalInfo->fontColor);
    QCOMPARE(unchangedInfo->notes, originalInfo->notes);

    classNameQuery.prepare(QStringLiteral("SELECT name FROM classes WHERE id=?"));
    classNameQuery.addBindValue(*createdClass);
    QVERIFY2(classNameQuery.exec(), qPrintable(classNameQuery.lastError().text()));
    QVERIFY(classNameQuery.next());
    QCOMPARE(classNameQuery.value(0).toString(), originalName);
}

void NextPlatformApplicationServicesTestingClassDetailsUpdatePortTests::
forwardsRepositoryFailuresAsTechnicalErrors()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClassRepository* const repository =
        services.databaseSession()->testingClassRepository();
    QVERIFY(repository);
    const auto createdClass = repository->createTestingClass(
        storedClass(QStringLiteral("Repository Failure Source"))
        );
    QVERIFY(createdClass);

    QSqlQuery dropTestingClasses(services.databaseSession()->database());
    QVERIFY2(dropTestingClasses.exec(QStringLiteral("DROP TABLE testing_classes")),
             qPrintable(dropTestingClasses.lastError().text()));

    const auto result = update(services, requestForClass(*createdClass));

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
}

void NextPlatformApplicationServicesTestingClassDetailsUpdatePortTests::
rejectsNoncanonicalIdentifiers()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesTestingClassDetailsUpdatePort port(services);

    auto invalidClassId = requestForClass(42);
    const auto parsedClassId = Domain::ClassId::fromString("042");
    QVERIFY(parsedClassId.has_value());
    invalidClassId.classId = *parsedClassId;
    const auto classResult = port.updateTestingClassDetails(invalidClassId);
    QVERIFY(!classResult);
    QCOMPARE(classResult.error().code, Domain::ErrorCode::InvalidInput);

    auto invalidTeacherId = requestForClass(42);
    const auto parsedTeacherId = Domain::TeacherId::fromString("017");
    QVERIFY(parsedTeacherId.has_value());
    invalidTeacherId.teacherId = *parsedTeacherId;
    const auto teacherResult = port.updateTestingClassDetails(invalidTeacherId);
    QVERIFY(!teacherResult);
    QCOMPARE(teacherResult.error().code, Domain::ErrorCode::InvalidInput);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesTestingClassDetailsUpdatePortTests
    )

#include "next_platform_application_services_testing_class_details_update_port_tests.moc"
