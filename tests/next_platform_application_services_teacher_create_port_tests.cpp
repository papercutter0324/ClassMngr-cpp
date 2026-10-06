#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/teacher_create_use_case.h"
#include "next/platform/application_services_teacher_create_port.h"

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
        QStringLiteral("teacher-create-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool executeSql(ApplicationServices& services, const QString& statement)
{
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return false;
    }
    QSqlQuery query(session->database());
    return query.exec(statement);
}

int teacherCount(ApplicationServices& services)
{
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return -1;
    }
    QSqlQuery query(session->database());
    return query.exec(QStringLiteral("SELECT COUNT(*) FROM teachers"))
        && query.next()
        ? query.value(0).toInt()
        : -1;
}

Application::TeacherCreateRequest validRequest()
{
    Domain::TeacherProfileFields fields;
    fields.teacherKr = u"  \uAE40\uC120\uC0DD  ";
    fields.teacherEn = u"  Alex   Smith  ";
    fields.preferredRomanization = u"  Alex Smith  ";
    fields.preferredName = u" alex smith ";
    fields.roomNumber = u" Room 9 ";
    fields.birthday = u" 02-29 ";
    fields.phoneNumber = u"01012345678";
    fields.wifiName = u" Campus WiFi ";
    fields.wifiPassword = u" secret ";
    fields.internetType = u"wifi";
    fields.zoomId = u" alex.zoom ";
    fields.zoomPassword = u" zoom-secret ";
    fields.projectionType = u"hdmi";
    fields.notes = u" Initial setup ";
    return {.fields = std::move(fields)};
}

}

class NextPlatformApplicationServicesTeacherCreatePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void createsNormalizedTeacherAndMapsId();
    void invalidTeacherIsRejectedBeforeInsert();
    void unavailableAndClosedSessionsReturnStructuredFailures();
    void repositoryFailureDoesNotCreateTeacher();
};

void NextPlatformApplicationServicesTeacherCreatePortTests::
createsNormalizedTeacherAndMapsId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Platform::ApplicationServicesTeacherCreatePort port(services);
    const auto created = Application::TeacherCreateUseCase::execute(
        validRequest(), port);

    if (!created)
    {
        QFAIL(qPrintable(QString::fromStdString(created.error().message)));
    }
    bool idOk = false;
    const int teacherId = QString::fromStdString(
        created.value().value()
        ).toInt(&idOk);
    QVERIFY(idOk);
    QVERIFY(teacherId > 0);
    QCOMPARE(teacherCount(services), 1);
    const auto saved = services.databaseSession()->teacherRepository()
        ->getTeacher(teacherId);
    QVERIFY(saved);
    QCOMPARE(saved->teacherKr, QString::fromUtf16(u"\uAE40\uC120\uC0DD"));
    QCOMPARE(saved->teacherEn, QStringLiteral("Alex Smith"));
    QCOMPARE(saved->preferredRomanization, QStringLiteral("Alex Smith"));
    QCOMPARE(saved->preferredName, QStringLiteral("Alex Smith"));
    QCOMPARE(saved->roomNumber, QStringLiteral("Room 9"));
    QCOMPARE(saved->birthday, QStringLiteral("02-29"));
    QCOMPARE(saved->phoneNumber, QStringLiteral("010-1234-5678"));
    QCOMPARE(saved->wifiName, QStringLiteral("Campus WiFi"));
    QCOMPARE(saved->wifiPassword, QStringLiteral("secret"));
    QCOMPARE(saved->internetType, QStringLiteral("WiFi"));
    QCOMPARE(saved->zoomId, QStringLiteral("alex.zoom"));
    QCOMPARE(saved->zoomPassword, QStringLiteral("zoom-secret"));
    QCOMPARE(saved->projectionType, QStringLiteral("HDMI"));
    QCOMPARE(saved->notes, QStringLiteral("Initial setup"));
}

void NextPlatformApplicationServicesTeacherCreatePortTests::
invalidTeacherIsRejectedBeforeInsert()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Application::TeacherCreateRequest request;
    Platform::ApplicationServicesTeacherCreatePort port(services);
    const auto result =
        Application::TeacherCreateUseCase::execute(request, port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QVERIFY(result.error().message.find("teacher.name.required")
        != std::string::npos);
    QCOMPARE(teacherCount(services), 0);
}

void NextPlatformApplicationServicesTeacherCreatePortTests::
unavailableAndClosedSessionsReturnStructuredFailures()
{
    const auto request = validRequest();
    {
        Platform::ApplicationServicesTeacherCreatePort nullPort(
            static_cast<ApplicationServices*>(nullptr)
            );
        const auto result =
            Application::TeacherCreateUseCase::execute(request, nullPort);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    }

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    const QString path = databasePath(directory);
    Platform::ApplicationServicesTeacherCreatePort port(services);
    const auto unopened =
        Application::TeacherCreateUseCase::execute(request, port);
    QVERIFY(!unopened);
    QCOMPARE(unopened.error().code, Domain::ErrorCode::NotFound);

    QVERIFY(services.openDatabase(path));
    services.closeDatabase();
    const auto closed =
        Application::TeacherCreateUseCase::execute(request, port);
    QVERIFY(!closed);
    QCOMPARE(closed.error().code, Domain::ErrorCode::NotFound);

    QVERIFY(services.openDatabase(path));
    QCOMPARE(teacherCount(services), 0);
}

void NextPlatformApplicationServicesTeacherCreatePortTests::
repositoryFailureDoesNotCreateTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER reject_teacher_create
            BEFORE INSERT ON teachers
            BEGIN
                SELECT RAISE(ABORT, 'forced teacher create failure');
            END
        )")
        ));

    Platform::ApplicationServicesTeacherCreatePort port(services);
    const auto result = Application::TeacherCreateUseCase::execute(
        validRequest(), port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QCOMPARE(teacherCount(services), 0);
}

QTEST_MAIN(NextPlatformApplicationServicesTeacherCreatePortTests)

#include "next_platform_application_services_teacher_create_port_tests.moc"
