#include "core/application_services.h"
#include "data/database/workspace_store.h"
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
    WorkspaceStore* const store = services.workspaceStore();
    if (!store || !store->isOpen())
    {
        return false;
    }
    QSqlQuery query(store->database());
    return query.exec(statement);
}

int teacherCount(ApplicationServices& services)
{
    WorkspaceStore* const store = services.workspaceStore();
    if (!store || !store->isOpen())
    {
        return -1;
    }
    QSqlQuery query(store->database());
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
    void createsTeacherWithWideIdAndPreservesProfile();
    void legacyCreateRejectsUnrepresentableIdWithoutWrapping();
    void invalidTeacherIsRejectedBeforeInsert();
    void unavailableAndClosedStoresReturnStructuredFailures();
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
    Platform::TeacherValidatorTeacherCreateValidationPolicy validation;
    const auto created = Application::TeacherCreateUseCase::execute(
        validRequest(), validation, port);

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
    const auto saved = services.workspaceStore()->teacherRepository()
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
createsTeacherWithWideIdAndPreservesProfile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO teachers (teacher_en) VALUES ('sequence seed')")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "UPDATE sqlite_sequence SET seq = 2147483647 "
            "WHERE name = 'teachers'")
        ));

    Platform::ApplicationServicesTeacherCreatePort port(services);
    Platform::TeacherValidatorTeacherCreateValidationPolicy validation;
    const auto created = Application::TeacherCreateUseCase::execute(
        validRequest(), validation, port);

    if (!created)
    {
        QFAIL(qPrintable(QString::fromStdString(created.error().message)));
    }
    const auto expectedId = Domain::TeacherId::fromString("2147483648");
    QVERIFY(expectedId.has_value());
    QVERIFY(created.value() == *expectedId);
    QCOMPARE(
        QString::fromStdString(created.value().value()),
        QStringLiteral("2147483648")
        );
    QCOMPARE(teacherCount(services), 2);

    QSqlQuery query(services.workspaceStore()->database());
    QVERIFY(query.exec(QStringLiteral(R"(
        SELECT
            typeof(id), id, teacher_kr, teacher_en,
            preferred_romanization, preferred_name, room_number, birthday,
            phone_number, wifi_name, wifi_password, internet_type, zoom_id,
            zoom_password, projection_type, notes
        FROM teachers
        WHERE id = 2147483648
    )")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(query.value(1).toLongLong(), qint64{2147483648});
    QCOMPARE(query.value(2).toString(), QString::fromUtf16(u"\uAE40\uC120\uC0DD"));
    QCOMPARE(query.value(3).toString(), QStringLiteral("Alex Smith"));
    QCOMPARE(query.value(4).toString(), QStringLiteral("Alex Smith"));
    QCOMPARE(query.value(5).toString(), QStringLiteral("Alex Smith"));
    QCOMPARE(query.value(6).toString(), QStringLiteral("Room 9"));
    QCOMPARE(query.value(7).toString(), QStringLiteral("02-29"));
    QCOMPARE(query.value(8).toString(), QStringLiteral("010-1234-5678"));
    QCOMPARE(query.value(9).toString(), QStringLiteral("Campus WiFi"));
    QCOMPARE(query.value(10).toString(), QStringLiteral("secret"));
    QCOMPARE(query.value(11).toString(), QStringLiteral("WiFi"));
    QCOMPARE(query.value(12).toString(), QStringLiteral("alex.zoom"));
    QCOMPARE(query.value(13).toString(), QStringLiteral("zoom-secret"));
    QCOMPARE(query.value(14).toString(), QStringLiteral("HDMI"));
    QCOMPARE(query.value(15).toString(), QStringLiteral("Initial setup"));
}

void NextPlatformApplicationServicesTeacherCreatePortTests::
legacyCreateRejectsUnrepresentableIdWithoutWrapping()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "INSERT INTO teachers (teacher_en) VALUES ('sequence seed')")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(
            "UPDATE sqlite_sequence SET seq = 2147483647 "
            "WHERE name = 'teachers'")
        ));

    Teacher legacyTeacher;
    legacyTeacher.teacherEn = QStringLiteral("Legacy Teacher");
    const auto created = services.workspaceStore()->teacherRepository()
        ->createTeacher(legacyTeacher);

    QVERIFY(!created);
    QVERIFY(created.error().contains(QStringLiteral("2147483648")));
    QCOMPARE(teacherCount(services), 2);

    QSqlQuery query(services.workspaceStore()->database());
    QVERIFY(query.exec(QStringLiteral(R"(
        SELECT typeof(id), id
        FROM teachers
        WHERE id = 2147483648
    )")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(query.value(1).toLongLong(), qint64{2147483648});
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
    Platform::TeacherValidatorTeacherCreateValidationPolicy validation;
    const auto result =
        Application::TeacherCreateUseCase::execute(request, validation, port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(
        result.error().message,
        std::string("Teacher validation failed: teacherEn: teacher.name.required")
        );
    QVERIFY(result.error().recoverable);
    QCOMPARE(teacherCount(services), 0);
}

void NextPlatformApplicationServicesTeacherCreatePortTests::
unavailableAndClosedStoresReturnStructuredFailures()
{
    const Application::TeacherCreateRequest invalidRequest;
    Platform::TeacherValidatorTeacherCreateValidationPolicy validation;
    {
        Platform::ApplicationServicesTeacherCreatePort nullPort(
            static_cast<ApplicationServices*>(nullptr)
            );
        const auto result =
            Application::TeacherCreateUseCase::execute(
                invalidRequest, validation, nullPort);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
        QCOMPARE(
            result.error().message,
            std::string("The active database session for teachers is unavailable.")
            );
    }

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    const QString path = databasePath(directory);
    Platform::ApplicationServicesTeacherCreatePort port(services);
    const auto unopened =
        Application::TeacherCreateUseCase::execute(
            invalidRequest, validation, port);
    QVERIFY(!unopened);
    QCOMPARE(unopened.error().code, Domain::ErrorCode::NotFound);

    QVERIFY(services.openDatabase(path));
    services.closeDatabase();
    const auto closed =
        Application::TeacherCreateUseCase::execute(
            invalidRequest, validation, port);
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
    Platform::TeacherValidatorTeacherCreateValidationPolicy validation;
    const auto result = Application::TeacherCreateUseCase::execute(
        validRequest(), validation, port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QCOMPARE(teacherCount(services), 0);
}

QTEST_MAIN(NextPlatformApplicationServicesTeacherCreatePortTests)

#include "next_platform_application_services_teacher_create_port_tests.moc"
