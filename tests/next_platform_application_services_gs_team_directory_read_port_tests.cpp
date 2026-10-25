#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/gs_team_repository.h"
#include "domain/models/gs_team_member.h"
#include "next/domain/domain_types.h"
#include "next/domain/native_english_teacher_id.h"
#include "next/platform/application_services_gs_team_directory_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <type_traits>
#include <vector>

using namespace ClassMngr::Next;

static_assert(!std::is_same_v<Domain::GsTeamMemberId, Domain::TeacherId>);
static_assert(!std::is_convertible_v<
    Domain::GsTeamMemberId,
    Domain::TeacherId>);
static_assert(!std::is_same_v<
    Domain::GsTeamMemberId,
    Domain::NativeEnglishTeacherId>);
static_assert(!std::is_convertible_v<
    Domain::GsTeamMemberId,
    Domain::NativeEnglishTeacherId>);

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("gs-team-directory-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

GsTeamMember member(
    const QString& name,
    const QString& koreanName,
    const QString& position,
    const QString& phone,
    const QString& birthday
    )
{
    GsTeamMember value;
    value.name = name;
    value.koreanName = koreanName;
    value.position = position;
    value.phoneNumber = phone;
    value.birthday = birthday;
    return value;
}

Application::GsTeamDirectoryEntry project(const GsTeamMember& value)
{
    return {
        .id = Domain::GsTeamMemberId(value.id),
        .name = value.name.toStdU16String(),
        .koreanName = value.koreanName.toStdU16String(),
        .position = value.position.toStdU16String(),
        .phoneNumber = value.phoneNumber.toStdU16String(),
        .birthday = value.birthday.toStdU16String()
    };
}

bool seedDirectory(ApplicationServices& services)
{
    const QList<GsTeamMember> members{
        member(QStringLiteral("Beta"), QStringLiteral("\uB098"),
            QStringLiteral("M1"), QStringLiteral("010-2000-0003"),
            QStringLiteral("03-14")),
        member(QStringLiteral("Alex"), QStringLiteral("\uC54C\uB809\uC2A4"),
            QStringLiteral("Branch Manager"), QStringLiteral("010-2000-0001"),
            QStringLiteral("01-02")),
        member(QStringLiteral(""), QStringLiteral("\uAE40\uD558\uB298"), QStringLiteral("M1"),
            QStringLiteral("010-2000-0004"), QStringLiteral("04-15")),
        member(QStringLiteral("Alpha"), QStringLiteral("\uAC00"),
            QStringLiteral("M1"), QStringLiteral("010-2000-0002"),
            QStringLiteral("02-03"))
    };
    const RepositoryStatus saved = services.workspaceStore()
        ->gsTeamRepository()->saveDirectory(members, {});
    return static_cast<bool>(saved);
}

QString persistedWideIdRowsMismatch(ApplicationServices& services)
{
    QSqlQuery query(services.workspaceStore()->database());
    if (!query.exec(QStringLiteral(R"(
        SELECT id, typeof(id) AS id_type, name, korean_name, position,
               phone_number, birthday
        FROM gs_team
        ORDER BY id
    )")))
    {
        return query.lastError().text();
    }

    const auto matches = [&query](
        qlonglong id,
        const QString& name,
        const QString& koreanName,
        const QString& position,
        const QString& phone,
        const QString& birthday
        )
    {
        return query.value(QStringLiteral("id")).toLongLong() == id
            && query.value(QStringLiteral("id_type")).toString()
                == QStringLiteral("integer")
            && query.value(QStringLiteral("name")).toString() == name
            && query.value(QStringLiteral("korean_name")).toString()
                == koreanName
            && query.value(QStringLiteral("position")).toString() == position
            && query.value(QStringLiteral("phone_number")).toString() == phone
            && query.value(QStringLiteral("birthday")).toString() == birthday;
    };

    if (!query.next() || !matches(
            1,
            QStringLiteral("Seed Member"),
            QStringLiteral("Seed Korean"),
            QStringLiteral("Branch Manager"),
            QStringLiteral("010-2000-1001"),
            QStringLiteral("01-02")))
    {
        return QStringLiteral("The ID 1 row or its fields changed.");
    }
    if (!query.next() || !matches(
            4294967297LL,
            QStringLiteral("Wide Member"),
            QStringLiteral("Wide Korean"),
            QStringLiteral("C1"),
            QStringLiteral("010-2000-1002"),
            QStringLiteral("02-03")))
    {
        return QStringLiteral("The ID 4294967297 row or its fields changed.");
    }
    if (query.next())
    {
        return QStringLiteral("The database returned an unexpected extra row.");
    }
    if (query.lastError().isValid())
    {
        return query.lastError().text();
    }

    return {};
}

}

class NextPlatformApplicationServicesGsTeamDirectoryReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsMappedRowsInRepositoryOrderFromActiveSession();
    void reportsUnavailableSessionSeparatelyFromRepositoryFailure();
    void rejectsTerminalCursorFailureWithoutPartialSnapshot();
    void rejectsWidePersistedIdsWithoutChangingStoredRows();
};

void NextPlatformApplicationServicesGsTeamDirectoryReadPortTests::
readsMappedRowsInRepositoryOrderFromActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    const auto persisted = services.workspaceStore()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);
    Application::GsTeamDirectorySnapshot expected;
    expected.reserve(static_cast<std::size_t>(persisted->size()));
    for (const GsTeamMember& value : *persisted)
    {
        expected.push_back(project(value));
    }

    Platform::ApplicationServicesGsTeamDirectoryReadPort port(&services);
    const Application::GsTeamDirectoryReadResult result =
        port.readGsTeamDirectory();

    QVERIFY(result);
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().size(), std::size_t(4));
    QCOMPARE(result.value().at(0).position,
        std::u16string(u"Branch Manager"));
    QCOMPARE(result.value().at(0).name, std::u16string(u"Alex"));
    QCOMPARE(result.value().at(1).name, std::u16string(u"Alpha"));
    QCOMPARE(result.value().at(2).name, std::u16string(u"Beta"));
    QCOMPARE(result.value().at(3).name, std::u16string());
    QVERIFY(result.value().at(0).id.value() > 0);
    QCOMPARE(result.value().at(0).koreanName,
        std::u16string(u"\uC54C\uB809\uC2A4"));
    QCOMPARE(result.value().at(0).phoneNumber,
        std::u16string(u"010-2000-0001"));
    QCOMPARE(result.value().at(0).birthday, std::u16string(u"01-02"));
}

void NextPlatformApplicationServicesGsTeamDirectoryReadPortTests::
reportsUnavailableSessionSeparatelyFromRepositoryFailure()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());

    Platform::ApplicationServicesGsTeamDirectoryReadPort port(&services);
    auto unavailable = port.readGsTeamDirectory();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(unavailable.error().recoverable);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    QSqlQuery query(services.workspaceStore()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE gs_team")),
        qPrintable(query.lastError().text()));
    const auto repositoryFailure = port.readGsTeamDirectory();
    QVERIFY(!repositoryFailure);
    QCOMPARE(repositoryFailure.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!repositoryFailure.error().message.empty());
    QVERIFY(!repositoryFailure.error().recoverable);

    services.closeDatabase();
    unavailable = port.readGsTeamDirectory();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
}

void NextPlatformApplicationServicesGsTeamDirectoryReadPortTests::
rejectsTerminalCursorFailureWithoutPartialSnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const QSqlDatabase database = services.workspaceStore()->database();

    QSqlQuery schema(database);
    QVERIFY2(schema.exec(QStringLiteral("DROP TABLE gs_team")),
        qPrintable(schema.lastError().text()));
    QVERIFY2(schema.exec(QStringLiteral(R"(
        CREATE TABLE gs_team_cursor_backing (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            korean_name TEXT NOT NULL,
            position TEXT NOT NULL,
            phone_number TEXT NOT NULL,
            birthday_json TEXT NOT NULL
        )
    )")), qPrintable(schema.lastError().text()));
    QVERIFY2(schema.exec(QStringLiteral(R"(
        CREATE INDEX gs_team_cursor_order_idx
        ON gs_team_cursor_backing (
            CASE position
                WHEN 'Branch Manager' THEN 1
                WHEN 'M3' THEN 2
                WHEN 'M2' THEN 3
                WHEN 'M1' THEN 4
                WHEN 'C3' THEN 5
                WHEN 'C2' THEN 6
                WHEN 'C1' THEN 7
                ELSE 8
            END,
            CASE WHEN name='' THEN korean_name ELSE name END COLLATE NOCASE,
            id
        )
    )")), qPrintable(schema.lastError().text()));

    QSqlQuery insert(database);
    QVERIFY2(insert.prepare(QStringLiteral(R"(
        INSERT INTO gs_team_cursor_backing
            (id, name, korean_name, position, phone_number, birthday_json)
        VALUES (?, ?, ?, ?, ?, ?)
    )")), qPrintable(insert.lastError().text()));
    insert.addBindValue(1);
    insert.addBindValue(QStringLiteral("Valid GS"));
    insert.addBindValue(QStringLiteral("Valid Korean"));
    insert.addBindValue(QStringLiteral("M1"));
    insert.addBindValue(QStringLiteral("010-2000-0001"));
    insert.addBindValue(QStringLiteral(R"({"birthday":"01-02"})"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    insert.bindValue(0, 2);
    insert.bindValue(1, QStringLiteral("z Malformed GS"));
    insert.bindValue(2, QStringLiteral("Malformed Korean"));
    insert.bindValue(3, QStringLiteral("M1"));
    insert.bindValue(4, QStringLiteral("010-2000-0002"));
    insert.bindValue(5, QStringLiteral("{"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    QVERIFY2(schema.exec(QStringLiteral(R"(
        CREATE VIEW gs_team AS
        SELECT
            id,
            name,
            korean_name,
            position,
            phone_number,
            json_extract(birthday_json, '$.birthday') AS birthday
        FROM gs_team_cursor_backing
    )")), qPrintable(schema.lastError().text()));

    // Keep this SELECT identical to GsTeamRepository::getAll() so this probe
    // proves execution succeeds before malformed JSON stops the ordered cursor.
    QSqlQuery rawQuery(database);
    QVERIFY2(rawQuery.exec(QStringLiteral(R"(
        SELECT id, name, korean_name, position, phone_number, birthday
        FROM gs_team
        ORDER BY CASE position
            WHEN 'Branch Manager' THEN 1
            WHEN 'M3' THEN 2
            WHEN 'M2' THEN 3
            WHEN 'M1' THEN 4
            WHEN 'C3' THEN 5
            WHEN 'C2' THEN 6
            WHEN 'C1' THEN 7
            ELSE 8
        END,
        CASE WHEN name='' THEN korean_name ELSE name END COLLATE NOCASE,
        id
    )")), qPrintable(rawQuery.lastError().text()));
    QVERIFY2(rawQuery.next(),
        "the first production-query row must be the valid GS Team member");
    QCOMPARE(rawQuery.value(QStringLiteral("name")).toString(),
        QStringLiteral("Valid GS"));
    QVERIFY2(!rawQuery.next(),
        "the malformed birthday JSON must terminate the cursor after one row");
    const QSqlError rawCursorError = rawQuery.lastError();
    QVERIFY2(rawCursorError.isValid(),
        "the terminal cursor error must be valid");
    QVERIFY2(rawCursorError.text().contains(
            QStringLiteral("malformed JSON"), Qt::CaseInsensitive),
        qPrintable(rawCursorError.text()));
    rawQuery.finish();

    const auto repositoryResult = services.workspaceStore()
        ->gsTeamRepository()->getAll();
    QVERIFY(!repositoryResult);
    QCOMPARE(repositoryResult.error().code, RepositoryErrorCode::Technical);
    QVERIFY(repositoryResult.error().diagnostic.contains(
        QStringLiteral("Loading GS Team directory failed")));
    QVERIFY(repositoryResult.error().diagnostic.contains(
        QStringLiteral("malformed JSON"), Qt::CaseInsensitive));

    Platform::ApplicationServicesGsTeamDirectoryReadPort port(&services);
    const Application::GsTeamDirectoryReadResult portResult =
        port.readGsTeamDirectory();
    QVERIFY(!portResult);
    QCOMPARE(portResult.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!portResult.error().recoverable);
    QVERIFY(!portResult.error().message.empty());
    QVERIFY(portResult.error().message.find("malformed JSON")
        != std::string::npos);
}

void NextPlatformApplicationServicesGsTeamDirectoryReadPortTests::
rejectsWidePersistedIdsWithoutChangingStoredRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QSqlQuery insert(services.workspaceStore()->database());
    QVERIFY2(insert.exec(QStringLiteral(R"(
        INSERT INTO gs_team
            (id, name, korean_name, position, phone_number, birthday)
        VALUES
            (1, 'Seed Member', 'Seed Korean', 'Branch Manager',
             '010-2000-1001', '01-02')
    )")), qPrintable(insert.lastError().text()));
    QVERIFY2(insert.exec(QStringLiteral(R"(
        INSERT INTO gs_team
            (id, name, korean_name, position, phone_number, birthday)
        VALUES
            (4294967297, 'Wide Member', 'Wide Korean', 'C1',
             '010-2000-1002', '02-03')
    )")), qPrintable(insert.lastError().text()));

    const QString beforeReadMismatch = persistedWideIdRowsMismatch(services);
    QVERIFY2(beforeReadMismatch.isEmpty(), qPrintable(beforeReadMismatch));

    const auto repositoryResult = services.workspaceStore()
        ->gsTeamRepository()->getAll();
    QVERIFY(!repositoryResult);
    QCOMPARE(repositoryResult.error().code, RepositoryErrorCode::Validation);
    QCOMPARE(repositoryResult.error().diagnostic, QStringLiteral(
        "Loading GS Team directory failed: member id 4294967297 is outside "
        "the supported range."));

    Platform::ApplicationServicesGsTeamDirectoryReadPort port(&services);
    const Application::GsTeamDirectoryReadResult result =
        port.readGsTeamDirectory();

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QVERIFY(!result.error().message.empty());
    QVERIFY(!result.error().recoverable);

    const QString afterReadMismatch = persistedWideIdRowsMismatch(services);
    QVERIFY2(afterReadMismatch.isEmpty(), qPrintable(afterReadMismatch));
}

QTEST_MAIN(NextPlatformApplicationServicesGsTeamDirectoryReadPortTests)

#include "next_platform_application_services_gs_team_directory_read_port_tests.moc"
