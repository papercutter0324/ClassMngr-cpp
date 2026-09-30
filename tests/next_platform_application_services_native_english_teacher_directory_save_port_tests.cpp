#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "domain/models/native_english_teacher.h"
#include "next/application/native_english_teacher_directory_save.h"
#include "next/platform/application_services_native_english_teacher_directory_save_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("native-english-directory-save-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

NativeEnglishTeacher existingTeacher(const QString& name)
{
    NativeEnglishTeacher teacher;
    teacher.name = name;
    teacher.position = QStringLiteral("NET");
    teacher.phoneNumber = QStringLiteral("010-1000-0001");
    teacher.email = QStringLiteral("%1@example.test").arg(name.toLower());
    teacher.birthday = QStringLiteral("01-02");
    teacher.nationality = QStringLiteral("Korea");
    return teacher;
}

bool seedDirectory(ApplicationServices& services)
{
    const QList<NativeEnglishTeacher> teachers{
        existingTeacher(QStringLiteral("Update Target")),
        existingTeacher(QStringLiteral("Delete Target"))
    };
    return static_cast<bool>(services.databaseSession()
        ->nativeEnglishTeacherRepository()->saveDirectory(teachers, {}));
}

Application::NativeEnglishTeacherDirectorySaveRow saveRow(
    const QString& name,
    const std::optional<int> id = std::nullopt
    )
{
    Application::NativeEnglishTeacherDirectorySaveRow row;
    if (id)
    {
        row.id = Domain::NativeEnglishTeacherId(*id);
    }
    row.name = name.toStdU16String();
    row.position = QStringLiteral("Team Leader").toStdU16String();
    row.phoneNumber = QStringLiteral("010-2222-3333").toStdU16String();
    row.email = QStringLiteral("%1@example.test").arg(name.toLower()).toStdU16String();
    row.birthday = QStringLiteral("02-29").toStdU16String();
    row.nationality = QStringLiteral("Australia").toStdU16String();
    row.normalizedNameKey = name.toCaseFolded().toStdU16String();
    row.birthdayIsBlank = false;
    row.birthdayIsValid = true;
    return row;
}

int scalarInt(QSqlDatabase database, const QString& sql)
{
    QSqlQuery query(database);
    if (!query.exec(sql) || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

}

class NextPlatformApplicationServicesNativeEnglishTeacherDirectorySavePortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void mapsTypedRowsAndDeletesThroughTheActiveSessionRepository();
    void unavailableSessionDoesNotUseTheDataServiceFallback();
    void repositoryFailureRollsBackTheEntireSaveTransaction();
};

void NextPlatformApplicationServicesNativeEnglishTeacherDirectorySavePortTests::
mapsTypedRowsAndDeletesThroughTheActiveSessionRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    const auto before = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(before);
    QCOMPARE(before->size(), 2);
    int updateId = -1;
    int deleteId = -1;
    for (const NativeEnglishTeacher& teacher : *before)
    {
        if (teacher.name == QStringLiteral("Update Target"))
        {
            updateId = teacher.id;
        }
        else if (teacher.name == QStringLiteral("Delete Target"))
        {
            deleteId = teacher.id;
        }
    }
    QVERIFY(updateId > 0);
    QVERIFY(deleteId > 0);

    Application::NativeEnglishTeacherDirectorySaveRequest request;
    request.rows = {
        saveRow(QStringLiteral("First Insert")),
        saveRow(QStringLiteral("Updated Target"), updateId),
        saveRow(QStringLiteral("Second Insert"))
    };
    request.deletedIds = {Domain::NativeEnglishTeacherId(deleteId)};

    Platform::ApplicationServicesNativeEnglishTeacherDirectorySavePort port(
        &services);
    const auto result = port.saveNativeEnglishTeacherDirectory(request);

    QVERIFY(result);
    QCOMPARE(scalarInt(
        services.databaseSession()->database(),
        QStringLiteral("SELECT COUNT(*) FROM native_english_teachers")), 3);
    QCOMPARE(scalarInt(
        services.databaseSession()->database(),
        QStringLiteral("SELECT COUNT(*) FROM native_english_teachers WHERE id=%1")
            .arg(deleteId)), 0);

    QSqlQuery rows(services.databaseSession()->database());
    QVERIFY2(rows.exec(QStringLiteral(
        "SELECT id, name, position, phone_number, email, birthday, nationality "
        "FROM native_english_teachers WHERE name IN "
        "('First Insert', 'Updated Target', 'Second Insert') ORDER BY id")),
        qPrintable(rows.lastError().text()));
    QVERIFY(rows.next());
    QCOMPARE(rows.value(0).toInt(), updateId);
    QCOMPARE(rows.value(1).toString(), QStringLiteral("Updated Target"));
    QCOMPARE(rows.value(2).toString(), QStringLiteral("Team Leader"));
    QCOMPARE(rows.value(3).toString(), QStringLiteral("010-2222-3333"));
    QCOMPARE(rows.value(4).toString(),
        QStringLiteral("updated target@example.test"));
    QCOMPARE(rows.value(5).toString(), QStringLiteral("02-29"));
    QCOMPARE(rows.value(6).toString(), QStringLiteral("Australia"));

    QVERIFY(rows.next());
    const int firstInsertId = rows.value(0).toInt();
    QCOMPARE(rows.value(1).toString(), QStringLiteral("First Insert"));
    QVERIFY(firstInsertId > updateId);
    QVERIFY(rows.next());
    const int secondInsertId = rows.value(0).toInt();
    QCOMPARE(rows.value(1).toString(), QStringLiteral("Second Insert"));
    QVERIFY(secondInsertId > firstInsertId);
    QVERIFY(!rows.next());
}

void NextPlatformApplicationServicesNativeEnglishTeacherDirectorySavePortTests::
unavailableSessionDoesNotUseTheDataServiceFallback()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());

    Application::NativeEnglishTeacherDirectorySaveRequest request;
    request.rows = {saveRow(QStringLiteral("No Session"))};
    Platform::ApplicationServicesNativeEnglishTeacherDirectorySavePort port(
        &services);
    QVERIFY(!port.hasActiveSession());
    const auto result = port.saveNativeEnglishTeacherDirectory(request);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
    QVERIFY(!services.hasOpenDatabase());

    Platform::ApplicationServicesNativeEnglishTeacherDirectorySavePort
        nullServicesPort(nullptr);
    QVERIFY(!nullServicesPort.hasActiveSession());
    const auto nullServicesResult =
        nullServicesPort.saveNativeEnglishTeacherDirectory(request);
    QVERIFY(!nullServicesResult);
    QCOMPARE(nullServicesResult.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesNativeEnglishTeacherDirectorySavePortTests::
repositoryFailureRollsBackTheEntireSaveTransaction()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    const auto before = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(before);
    int updateId = -1;
    int deleteId = -1;
    for (const NativeEnglishTeacher& teacher : *before)
    {
        if (teacher.name == QStringLiteral("Update Target"))
        {
            updateId = teacher.id;
        }
        else if (teacher.name == QStringLiteral("Delete Target"))
        {
            deleteId = teacher.id;
        }
    }
    QVERIFY(updateId > 0);
    QVERIFY(deleteId > 0);

    QSqlQuery trigger(services.databaseSession()->database());
    QVERIFY2(trigger.exec(QStringLiteral(
        "CREATE TRIGGER reject_directory_insert "
        "BEFORE INSERT ON native_english_teachers "
        "BEGIN SELECT RAISE(ABORT, 'insert rejected'); END")),
        qPrintable(trigger.lastError().text()));

    Application::NativeEnglishTeacherDirectorySaveRequest request;
    request.rows = {
        saveRow(QStringLiteral("Updated Target"), updateId),
        saveRow(QStringLiteral("Rejected Insert"))
    };
    request.deletedIds = {Domain::NativeEnglishTeacherId(deleteId)};
    Platform::ApplicationServicesNativeEnglishTeacherDirectorySavePort port(
        &services);
    const auto result = port.saveNativeEnglishTeacherDirectory(request);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
    QVERIFY(!result.error().message.empty());

    const auto after = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(after);
    QCOMPARE(after->size(), 2);
    bool originalUpdatePresent = false;
    bool deletedRowRestored = false;
    bool failedInsertPresent = false;
    for (const NativeEnglishTeacher& teacher : *after)
    {
        originalUpdatePresent |= teacher.id == updateId
            && teacher.name == QStringLiteral("Update Target");
        deletedRowRestored |= teacher.id == deleteId
            && teacher.name == QStringLiteral("Delete Target");
        failedInsertPresent |= teacher.name == QStringLiteral("Rejected Insert");
    }
    QVERIFY(originalUpdatePresent);
    QVERIFY(deletedRowRestored);
    QVERIFY(!failedInsertPresent);
}

QTEST_MAIN(
    NextPlatformApplicationServicesNativeEnglishTeacherDirectorySavePortTests)

#include "next_platform_application_services_native_english_teacher_directory_save_port_tests.moc"
