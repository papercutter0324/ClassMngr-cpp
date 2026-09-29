#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/testing_teacher_choices_read_query.h"
#include "next/platform/application_services_testing_teacher_choices_read_port.h"

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
        QStringLiteral("testing-teacher-choices-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Teacher teacher(
    const QString& englishName,
    const QString& koreanName,
    const QString& room
    )
{
    Teacher value;
    value.teacherEn = englishName;
    value.teacherKr = koreanName;
    value.roomNumber = room;
    return value;
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

}

class NextPlatformApplicationServicesTestingTeacherChoicesReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsAllChoiceFieldsInRepositoryOrderFromTheActiveSession();
    void preservesSuccessfulEmptyRepositoryResults();
    void treatsUnavailableSessionsAsNotFound();
    void forwardsTeacherRepositoryFailuresWithoutFallback();
};

void NextPlatformApplicationServicesTestingTeacherChoicesReadPortTests::
readsAllChoiceFieldsInRepositoryOrderFromTheActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    TeacherRepository* const repository = session->teacherRepository();
    QVERIFY(repository);

    // Insert in the reverse of the repository's teacher_en order so the
    // assertions observe the query's actual ordering contract.
    const auto zuluId = repository->createTeacher(teacher(
        QStringLiteral("Zulu English"),
        QStringLiteral("  이선생  "),
        QStringLiteral("  Room 12  ")
        ));
    QVERIFY(zuluId);
    const auto alphaId = repository->createTeacher(teacher(
        QStringLiteral("Alpha English"),
        QStringLiteral("  김선생  "),
        QStringLiteral("  실험실 204  ")
        ));
    QVERIFY(alphaId);

    Platform::ApplicationServicesTestingTeacherChoicesReadPort port(
        services
        );
    const auto result =
        Application::TestingTeacherChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(result);
    QCOMPARE(result.value().choices.size(), std::size_t(2));
    QCOMPARE(result.value().choices[0].teacherId, teacherId(*alphaId));
    QCOMPARE(
        result.value().choices[0].name,
        std::u16string(u"  김선생  ")
        );
    QCOMPARE(
        result.value().choices[0].room,
        std::u16string(u"  실험실 204  ")
        );
    QCOMPARE(result.value().choices[1].teacherId, teacherId(*zuluId));
    QCOMPARE(
        result.value().choices[1].name,
        std::u16string(u"  이선생  ")
        );
    QCOMPARE(
        result.value().choices[1].room,
        std::u16string(u"  Room 12  ")
        );
}

void NextPlatformApplicationServicesTestingTeacherChoicesReadPortTests::
preservesSuccessfulEmptyRepositoryResults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Platform::ApplicationServicesTestingTeacherChoicesReadPort port(
        services
        );

    const auto result =
        Application::TestingTeacherChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(result);
    QVERIFY(result.value().choices.empty());
}

void NextPlatformApplicationServicesTestingTeacherChoicesReadPortTests::
treatsUnavailableSessionsAsNotFound()
{
    Platform::ApplicationServicesTestingTeacherChoicesReadPort nullServicesPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    auto result =
        Application::TestingTeacherChoicesReadQueryHandler::execute(
            {},
            nullServicesPort
            );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);

    ApplicationServices unopenedServices;
    Platform::ApplicationServicesTestingTeacherChoicesReadPort unopenedPort(
        unopenedServices
        );
    result = Application::TestingTeacherChoicesReadQueryHandler::execute(
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
    Platform::ApplicationServicesTestingTeacherChoicesReadPort closedPort(
        closedServices
        );
    result = Application::TestingTeacherChoicesReadQueryHandler::execute(
        {},
        closedPort
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesTestingTeacherChoicesReadPortTests::
forwardsTeacherRepositoryFailuresWithoutFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery dropTeachers(services.databaseSession()->database());
    QVERIFY(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")));

    Platform::ApplicationServicesTestingTeacherChoicesReadPort port(
        services
        );
    const auto result =
        Application::TestingTeacherChoicesReadQueryHandler::execute(
            {},
            port
            );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().message.find("Loading teachers")
            != std::string::npos);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesTestingTeacherChoicesReadPortTests
    )

#include "next_platform_application_services_testing_teacher_choices_read_port_tests.moc"
