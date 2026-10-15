#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/testing_teacher_choices_read_query.h"
#include "next/platform/application_services_testing_teacher_choices_read_port.h"
#include "teacher_choices_cursor_error_fixture.h"

#include <QSqlDatabase>
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

Domain::TeacherId teacherId(const qint64 value)
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
    void readsProjectedChoiceFieldsInRepositoryOrderFromTheActiveSession();
    void preservesWideTeacherIdentityInChoiceProjection();
    void preservesSuccessfulEmptyRepositoryResults();
    void treatsUnavailableSessionsAsNotFound();
    void forwardsTeacherRepositoryFailuresWithoutFallback();
    void rejectsPartialTeacherChoicesAfterCursorError();
};

void NextPlatformApplicationServicesTestingTeacherChoicesReadPortTests::
readsProjectedChoiceFieldsInRepositoryOrderFromTheActiveSession()
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
preservesWideTeacherIdentityInChoiceProjection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    constexpr qint64 wideTeacherId = 2'147'483'648LL;
    QSqlQuery insertTeacher(session->database());
    QVERIFY(insertTeacher.prepare(QStringLiteral(
        "INSERT INTO teachers (id, teacher_en, teacher_kr, room_number) "
        "VALUES (?, 'Wide Teacher', 'Wide Korean Teacher', 'Room Wide')"
        )));
    insertTeacher.addBindValue(wideTeacherId);
    QVERIFY(insertTeacher.exec());

    const auto records = session->teacherRepository()
        ->loadTestingTeacherChoiceRecords();
    QVERIFY(records);
    QCOMPARE(records->size(), 1);
    QCOMPARE(records->first().teacherId, wideTeacherId);

    Platform::ApplicationServicesTestingTeacherChoicesReadPort port(services);
    const auto result = Application::TestingTeacherChoicesReadQueryHandler::execute(
        {}, port
        );
    QVERIFY(result);
    QCOMPARE(result.value().choices.size(), std::size_t(1));
    QCOMPARE(result.value().choices.front().teacherId, teacherId(wideTeacherId));
    QCOMPARE(result.value().choices.front().name,
             std::u16string(u"Wide Korean Teacher"));
    QCOMPARE(result.value().choices.front().room, std::u16string(u"Room Wide"));
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

void NextPlatformApplicationServicesTestingTeacherChoicesReadPortTests::
rejectsPartialTeacherChoicesAfterCursorError()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QSqlDatabase database = session->database();

    const QString setupError =
        TeacherChoicesCursorErrorFixture::install(database);
    QVERIFY2(setupError.isEmpty(), qPrintable(setupError));

    const QString selectSql = QStringLiteral(R"(
        SELECT
            id,
            teacher_kr,
            room_number
        FROM teachers
        ORDER BY teacher_en
    )");
    const auto plan = TeacherChoicesCursorErrorFixture::explain(
        database,
        selectSql
        );
    QVERIFY2(plan.executed, qPrintable(plan.error));
    QVERIFY2(
        plan.details.join(QLatin1Char('\n')).contains(
            QStringLiteral("teacher_choices_cursor_teacher_en_idx")
            ),
        qPrintable(plan.details.join(QLatin1Char('\n')))
        );

    const auto probe = TeacherChoicesCursorErrorFixture::probe(
        database,
        selectSql
        );
    QVERIFY2(probe.executed, qPrintable(probe.terminalError.text()));
    QVERIFY(probe.firstRowFound);
    QCOMPARE(probe.firstTeacherId, qint64(1));
    QCOMPARE(probe.firstTeacherKr, QStringLiteral("A Alpha"));
    QVERIFY(!probe.secondRowFound);
    QVERIFY(probe.terminalError.isValid());
    QVERIFY(probe.terminalError.text().contains(
        QStringLiteral("malformed"), Qt::CaseInsensitive
        ));

    TeacherRepository* const repository = session->teacherRepository();
    QVERIFY(repository);
    const auto records = repository->loadTestingTeacherChoiceRecords();
    QVERIFY(!records);
    QVERIFY(records.error().contains(QStringLiteral("malformed"), Qt::CaseInsensitive));

    Platform::ApplicationServicesTestingTeacherChoicesReadPort port(services);
    const auto result = port.readTestingTeacherChoices(
        Application::TestingTeacherChoicesReadQuery{}
        );
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().recoverable);
    QVERIFY(result.error().message.find("malformed") != std::string::npos);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesTestingTeacherChoicesReadPortTests
    )

#include "next_platform_application_services_testing_teacher_choices_read_port_tests.moc"
