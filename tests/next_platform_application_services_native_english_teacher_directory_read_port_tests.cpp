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
    void rejectsUnrepresentableIdAsTechnicalFailureWithoutChangingStoredRow();
    void rejectsTerminalCursorErrorWithoutReturningPartialDirectory();
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

void NextPlatformApplicationServicesNativeEnglishTeacherDirectoryReadPortTests::
rejectsUnrepresentableIdAsTechnicalFailureWithoutChangingStoredRow()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QSqlQuery insert(services.databaseSession()->database());
    QVERIFY2(insert.exec(QStringLiteral(R"(
        INSERT INTO native_english_teachers
            (id, name, position, phone_number, birthday, nationality, email)
        VALUES
            (2147483648, 'Overflow', 'NET', '010-9999-0004', '03-04',
             'Canada', 'overflow@example.test')
    )")), qPrintable(insert.lastError().text()));

    const QString storedRowSql = QStringLiteral(R"(
        SELECT typeof(id), id, name, position, phone_number,
               birthday, nationality, email
        FROM native_english_teachers
        WHERE id=2147483648
    )");
    QSqlQuery before(services.databaseSession()->database());
    QVERIFY2(before.exec(storedRowSql), qPrintable(before.lastError().text()));
    QVERIFY(before.next());
    QCOMPARE(before.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(before.value(1).toLongLong(), qlonglong(2147483648LL));
    QCOMPARE(before.value(2).toString(), QStringLiteral("Overflow"));
    QCOMPARE(before.value(3).toString(), QStringLiteral("NET"));
    QCOMPARE(before.value(4).toString(), QStringLiteral("010-9999-0004"));
    QCOMPARE(before.value(5).toString(), QStringLiteral("03-04"));
    QCOMPARE(before.value(6).toString(), QStringLiteral("Canada"));
    QCOMPARE(before.value(7).toString(), QStringLiteral("overflow@example.test"));
    const QVariantList storedRowBefore{
        before.value(0), before.value(1), before.value(2), before.value(3),
        before.value(4), before.value(5), before.value(6), before.value(7)
    };
    QVERIFY(!before.next());

    Platform::ApplicationServicesNativeEnglishTeacherDirectoryReadPort port(
        &services);
    const auto result = port.readNativeEnglishTeacherDirectory();
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().recoverable);
    QVERIFY(!result.error().message.empty());

    QSqlQuery after(services.databaseSession()->database());
    QVERIFY2(after.exec(storedRowSql), qPrintable(after.lastError().text()));
    QVERIFY(after.next());
    const QVariantList storedRowAfter{
        after.value(0), after.value(1), after.value(2), after.value(3),
        after.value(4), after.value(5), after.value(6), after.value(7)
    };
    QVERIFY(!after.next());
    QCOMPARE(storedRowAfter, storedRowBefore);
}

void NextPlatformApplicationServicesNativeEnglishTeacherDirectoryReadPortTests::
rejectsTerminalCursorErrorWithoutReturningPartialDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QSqlDatabase database = services.databaseSession()->database();
    QSqlQuery setup(database);
    QVERIFY2(setup.exec(QStringLiteral("DROP TABLE native_english_teachers")),
        qPrintable(setup.lastError().text()));
    QVERIFY2(setup.exec(QStringLiteral(R"(
        CREATE TABLE native_english_teachers_json_backing (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            position TEXT NOT NULL,
            phone_number TEXT NOT NULL,
            birthday_json TEXT NOT NULL,
            nationality TEXT NOT NULL,
            email TEXT NOT NULL
        )
    )")), qPrintable(setup.lastError().text()));

    QSqlQuery insert(database);
    QVERIFY2(insert.prepare(QStringLiteral(R"(
        INSERT INTO native_english_teachers_json_backing
            (id, name, position, phone_number, birthday_json, nationality, email)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )")), qPrintable(insert.lastError().text()));
    insert.addBindValue(1);
    insert.addBindValue(QStringLiteral("Valid Native"));
    insert.addBindValue(QStringLiteral("Co-ordinator"));
    insert.addBindValue(QStringLiteral("010-9999-0001"));
    insert.addBindValue(QStringLiteral(R"({"birthday":"01-02"})"));
    insert.addBindValue(QStringLiteral("Korea"));
    insert.addBindValue(QStringLiteral("valid@example.test"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    insert.bindValue(0, 2);
    insert.bindValue(1, QStringLiteral("Broken Native"));
    insert.bindValue(2, QStringLiteral("NET"));
    insert.bindValue(3, QStringLiteral("010-9999-0002"));
    insert.bindValue(4, QStringLiteral("{"));
    insert.bindValue(5, QStringLiteral("Canada"));
    insert.bindValue(6, QStringLiteral("broken@example.test"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    QVERIFY2(setup.exec(QStringLiteral(R"(
        CREATE INDEX native_english_teachers_cursor_error_order_idx
        ON native_english_teachers_json_backing (
            CASE position
                WHEN 'Co-ordinator' THEN 1
                WHEN 'Team Leader' THEN 2
                WHEN 'M3 Song''s' THEN 3
                WHEN 'M2 Song''s' THEN 4
                WHEN 'M1 Song''s' THEN 5
                WHEN 'E6 Song''s' THEN 6
                WHEN 'E5 Athena' THEN 7
                WHEN 'NET' THEN 8
                ELSE 9
            END,
            name COLLATE NOCASE,
            id
        )
    )")), qPrintable(setup.lastError().text()));
    QVERIFY2(setup.exec(QStringLiteral(R"(
        CREATE VIEW native_english_teachers AS
        SELECT
            id,
            name,
            position,
            phone_number,
            json_extract(birthday_json, '$.birthday') AS birthday,
            nationality,
            email
        FROM native_english_teachers_json_backing
    )")), qPrintable(setup.lastError().text()));

    QSqlQuery cursorProbe(database);
    QVERIFY2(cursorProbe.exec(QStringLiteral(R"(
        SELECT id, name, position, phone_number, birthday, nationality, email
        FROM native_english_teachers
        ORDER BY CASE position
            WHEN 'Co-ordinator' THEN 1
            WHEN 'Team Leader' THEN 2
            WHEN 'M3 Song''s' THEN 3
            WHEN 'M2 Song''s' THEN 4
            WHEN 'M1 Song''s' THEN 5
            WHEN 'E6 Song''s' THEN 6
            WHEN 'E5 Athena' THEN 7
            WHEN 'NET' THEN 8
            ELSE 9
        END, name COLLATE NOCASE, id
    )")), qPrintable(cursorProbe.lastError().text()));
    QVERIFY(cursorProbe.next());
    QCOMPARE(cursorProbe.value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(cursorProbe.value(QStringLiteral("name")).toString(),
        QStringLiteral("Valid Native"));
    QCOMPARE(cursorProbe.value(QStringLiteral("birthday")).toString(),
        QStringLiteral("01-02"));
    QVERIFY(!cursorProbe.next());
    QVERIFY(cursorProbe.lastError().isValid());
    const QString cursorError = cursorProbe.lastError().text().trimmed();
    QVERIFY(!cursorError.isEmpty());
    QVERIFY(cursorError.contains(QStringLiteral("malformed"), Qt::CaseInsensitive));

    const auto directResult = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(!directResult);
    QVERIFY(directResult.error().contains(
        QStringLiteral("Loading Native English Teacher directory")
        ));
    QVERIFY(directResult.error().contains(cursorError));

    Platform::ApplicationServicesNativeEnglishTeacherDirectoryReadPort port(
        &services);
    const auto portResult = port.readNativeEnglishTeacherDirectory();
    QVERIFY(!portResult);
    QCOMPARE(portResult.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!portResult.error().recoverable);
    QVERIFY(!portResult.error().message.empty());
}

QTEST_MAIN(
    NextPlatformApplicationServicesNativeEnglishTeacherDirectoryReadPortTests)

#include "next_platform_application_services_native_english_teacher_directory_read_port_tests.moc"
