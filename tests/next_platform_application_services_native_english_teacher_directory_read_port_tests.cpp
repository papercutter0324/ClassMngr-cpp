#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "domain/models/native_english_teacher.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_native_english_teacher_directory_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <type_traits>
#include <vector>

using namespace ClassMngr::Next;

static_assert(!std::is_same_v<
    Domain::NativeEnglishTeacherId,
    Domain::TeacherId>);
static_assert(!std::is_convertible_v<
    Domain::NativeEnglishTeacherId,
    Domain::TeacherId>);

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("native-english-directory-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

NativeEnglishTeacher teacher(
    const QString& name,
    const QString& position,
    const QString& phone,
    const QString& email,
    const QString& birthday,
    const QString& nationality
    )
{
    NativeEnglishTeacher value;
    value.name = name;
    value.position = position;
    value.phoneNumber = phone;
    value.email = email;
    value.birthday = birthday;
    value.nationality = nationality;
    return value;
}

Application::NativeEnglishTeacherDirectoryEntry project(
    const NativeEnglishTeacher& teacher
    )
{
    return {
        .id = Domain::NativeEnglishTeacherId(teacher.id),
        .name = teacher.name.toStdU16String(),
        .position = teacher.position.toStdU16String(),
        .phoneNumber = teacher.phoneNumber.toStdU16String(),
        .email = teacher.email.toStdU16String(),
        .birthday = teacher.birthday.toStdU16String(),
        .nationality = teacher.nationality.toStdU16String()
    };
}

bool seedDirectory(ApplicationServices& services)
{
    const QList<NativeEnglishTeacher> teachers{
        teacher(QStringLiteral("Zulu"), QStringLiteral("NET"),
            QStringLiteral("010-9999-0003"), QStringLiteral("z@example.test"),
            QStringLiteral("12-31"), QStringLiteral("New Zealand")),
        teacher(QStringLiteral("Coordinator"), QStringLiteral("Co-ordinator"),
            QStringLiteral("010-9999-0001"), QStringLiteral("c@example.test"),
            QStringLiteral("01-02"), QStringLiteral("Korea")),
        teacher(QStringLiteral("Alpha"), QStringLiteral("NET"),
            QStringLiteral("010-9999-0002"), QStringLiteral("a@example.test"),
            QStringLiteral("02-03"), QStringLiteral("Australia"))
    };
    const Status saved = services.databaseSession()
        ->nativeEnglishTeacherRepository()->saveDirectory(teachers, {});
    return static_cast<bool>(saved);
}

}

class NextPlatformApplicationServicesNativeEnglishTeacherDirectoryReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsAllFieldsInRepositoryOrderFromTheActiveSession();
    void distinguishesUnavailableSessionFromRepositoryFailureWithoutFallback();
};

void NextPlatformApplicationServicesNativeEnglishTeacherDirectoryReadPortTests::
readsAllFieldsInRepositoryOrderFromTheActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    const auto persisted = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(persisted);
    Application::NativeEnglishTeacherDirectorySnapshot expected;
    expected.reserve(static_cast<std::size_t>(persisted->size()));
    for (const NativeEnglishTeacher& value : *persisted)
    {
        expected.push_back(project(value));
    }

    Platform::ApplicationServicesNativeEnglishTeacherDirectoryReadPort port(
        &services);
    const Application::NativeEnglishTeacherDirectoryReadResult result =
        port.readNativeEnglishTeacherDirectory();

    QVERIFY(result);
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().size(), std::size_t(3));
    QCOMPARE(result.value().at(0).position, std::u16string(u"Co-ordinator"));
    QCOMPARE(result.value().at(1).name, std::u16string(u"Alpha"));
    QCOMPARE(result.value().at(2).name, std::u16string(u"Zulu"));
    QVERIFY(result.value().at(0).id.value() > 0);
}

void NextPlatformApplicationServicesNativeEnglishTeacherDirectoryReadPortTests::
distinguishesUnavailableSessionFromRepositoryFailureWithoutFallback()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());

    Platform::ApplicationServicesNativeEnglishTeacherDirectoryReadPort port(
        &services);
    auto unavailable = port.readNativeEnglishTeacherDirectory();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(unavailable.error().recoverable);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE native_english_teachers")),
        qPrintable(query.lastError().text()));
    const auto repositoryFailure = port.readNativeEnglishTeacherDirectory();
    QVERIFY(!repositoryFailure);
    QCOMPARE(repositoryFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!repositoryFailure.error().message.empty());
    QVERIFY(!repositoryFailure.error().recoverable);

    services.closeDatabase();
    unavailable = port.readNativeEnglishTeacherDirectory();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
}

QTEST_MAIN(
    NextPlatformApplicationServicesNativeEnglishTeacherDirectoryReadPortTests)

#include "next_platform_application_services_native_english_teacher_directory_read_port_tests.moc"
