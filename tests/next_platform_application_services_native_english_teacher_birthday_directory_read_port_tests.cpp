#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "next/application/native_english_teacher_birthday_directory_read_query.h"
#include "next/platform/application_services_native_english_teacher_birthday_directory_read_port.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>
#include <QtTest/QtTest>

#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("native-english-birthday-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

struct RawTeacher final
{
    QVariant name;
    QVariant position;
    QVariant birthday;
};

bool seedTeachers(
    QSqlDatabase database,
    const std::vector<RawTeacher>& teachers
    )
{
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO native_english_teachers "
            "(name, position, birthday) VALUES (?, ?, ?)"
            )))
    {
        return false;
    }
    for (const RawTeacher& teacher : teachers)
    {
        query.bindValue(0, teacher.name);
        query.bindValue(1, teacher.position);
        query.bindValue(2, teacher.birthday);
        if (!query.exec())
        {
            return false;
        }
    }
    return true;
}

bool keepOnlyBirthdayFields(QSqlDatabase database)
{
    QSqlQuery query(database);
    return query.exec(QStringLiteral("DROP TABLE native_english_teachers"))
        && query.exec(QStringLiteral(R"(
            CREATE TABLE native_english_teachers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT,
                position TEXT,
                birthday TEXT
            )
        )"));
}

Application::NativeEnglishTeacherBirthdayDirectoryReadResult readDirectory(
    ApplicationServices& services
    )
{
    Platform::ApplicationServicesNativeEnglishTeacherBirthdayDirectoryReadPort
        port(&services);
    const Application::NativeEnglishTeacherBirthdayDirectoryReadQuery query(
        port);
    return query.execute();
}

}

class NextPlatformApplicationServicesNativeEnglishTeacherBirthdayDirectoryReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsOnlyRawBirthdayFieldsInRepositoryOrder();
    void returnsAnEmptySuccessfulProjection();
    void distinguishesUnavailableSessionFromRepositoryFailure();
};

void NextPlatformApplicationServicesNativeEnglishTeacherBirthdayDirectoryReadPortTests::
readsOnlyRawBirthdayFieldsInRepositoryOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(keepOnlyBirthdayFields(services.databaseSession()->database()));
    QVERIFY(seedTeachers(
        services.databaseSession()->database(),
        {
            {QStringLiteral("Zulu "), QStringLiteral("NET"),
                QStringLiteral(" 12-31 ")},
            {QStringLiteral("Alpha "), QStringLiteral("NET"),
                QStringLiteral("02-03")},
            {QStringLiteral("Alpha "), QStringLiteral("NET"),
                QStringLiteral(" 02-04 ")},
            {QStringLiteral("Coordinator "), QStringLiteral("Co-ordinator"),
                QStringLiteral(" 01-02 ")},
            {QVariant{}, QVariant{}, QVariant{}}
        }
        ));

    const auto result = readDirectory(services);

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(5));
    QCOMPARE(result.value()[0].name, std::u16string(u"Coordinator "));
    QCOMPARE(result.value()[0].position, std::u16string(u"Co-ordinator"));
    QCOMPARE(result.value()[0].birthday, std::u16string(u" 01-02 "));
    QCOMPARE(result.value()[1].name, std::u16string(u"Alpha "));
    QCOMPARE(result.value()[1].birthday, std::u16string(u"02-03"));
    QCOMPARE(result.value()[2].name, std::u16string(u"Alpha "));
    QCOMPARE(result.value()[2].birthday, std::u16string(u" 02-04 "));
    QCOMPARE(result.value()[3].name, std::u16string(u"Zulu "));
    QVERIFY(result.value()[4].name.empty());
    QVERIFY(result.value()[4].position.empty());
    QVERIFY(result.value()[4].birthday.empty());
}

void NextPlatformApplicationServicesNativeEnglishTeacherBirthdayDirectoryReadPortTests::
returnsAnEmptySuccessfulProjection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(keepOnlyBirthdayFields(services.databaseSession()->database()));

    const auto result = readDirectory(services);

    QVERIFY(result);
    QVERIFY(result.value().empty());
}

void NextPlatformApplicationServicesNativeEnglishTeacherBirthdayDirectoryReadPortTests::
distinguishesUnavailableSessionFromRepositoryFailure()
{
    ApplicationServices services;
    Platform::ApplicationServicesNativeEnglishTeacherBirthdayDirectoryReadPort
        port(&services);
    auto unavailable = port.readNativeEnglishTeacherBirthdayDirectory();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(unavailable.error().recoverable);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery dropTable(services.databaseSession()->database());
    QVERIFY2(
        dropTable.exec(QStringLiteral("DROP TABLE native_english_teachers")),
        qPrintable(dropTable.lastError().text())
        );
    const auto failed = port.readNativeEnglishTeacherBirthdayDirectory();
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());
    QVERIFY(!failed.error().recoverable);

    services.closeDatabase();
    unavailable = port.readNativeEnglishTeacherBirthdayDirectory();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
}

QTEST_MAIN(
    NextPlatformApplicationServicesNativeEnglishTeacherBirthdayDirectoryReadPortTests)

#include "next_platform_application_services_native_english_teacher_birthday_directory_read_port_tests.moc"
