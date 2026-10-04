#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/gs_team_repository.h"
#include "next/application/gs_team_birthday_directory_read_query.h"
#include "next/platform/application_services_gs_team_birthday_directory_read_port.h"

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
        QStringLiteral("gs-team-birthday-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

struct RawMember final
{
    QVariant name;
    QVariant koreanName;
    QVariant position;
    QVariant birthday;
};

bool seedMembers(
    QSqlDatabase database,
    const std::vector<RawMember>& members
    )
{
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO gs_team (name, korean_name, position, birthday) "
            "VALUES (?, ?, ?, ?)"
            )))
    {
        return false;
    }
    for (const RawMember& member : members)
    {
        query.bindValue(0, member.name);
        query.bindValue(1, member.koreanName);
        query.bindValue(2, member.position);
        query.bindValue(3, member.birthday);
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
    return query.exec(QStringLiteral("DROP TABLE gs_team"))
        && query.exec(QStringLiteral(R"(
            CREATE TABLE gs_team (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT,
                korean_name TEXT,
                position TEXT,
                birthday TEXT
            )
        )"));
}

Application::GsTeamBirthdayDirectoryReadResult readDirectory(
    ApplicationServices& services
    )
{
    Platform::ApplicationServicesGsTeamBirthdayDirectoryReadPort port(&services);
    const Application::GsTeamBirthdayDirectoryReadQuery query(port);
    return query.execute();
}

}

class NextPlatformApplicationServicesGsTeamBirthdayDirectoryReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsRawBirthdayFieldsWithExactLegacySortOrder();
    void returnsAnEmptySuccessfulProjection();
    void distinguishesUnavailableSessionFromRepositoryFailure();
};

void NextPlatformApplicationServicesGsTeamBirthdayDirectoryReadPortTests::
readsRawBirthdayFieldsWithExactLegacySortOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(keepOnlyBirthdayFields(services.databaseSession()->database()));
    QVERIFY(seedMembers(
        services.databaseSession()->database(),
        {
            {QStringLiteral("Beta "), QStringLiteral("Korean Beta"),
                QStringLiteral("M1"), QStringLiteral(" 03-14 ")},
            {QStringLiteral(""), QStringLiteral("Zulu Fallback"),
                QStringLiteral("M1"), QStringLiteral("04-15")},
            {QStringLiteral("Alpha "), QStringLiteral("Korean Alpha"),
                QStringLiteral("M1"), QStringLiteral("02-03")},
            {QStringLiteral("alpha "), QStringLiteral("Korean Alpha 2"),
                QStringLiteral("M1"), QStringLiteral(" 02-04 ")},
            {QVariant{}, QStringLiteral("Null Name"),
                QStringLiteral("M1"), QVariant{}},
            {QStringLiteral("Alex "), QStringLiteral("Korean Alex"),
                QStringLiteral("Branch Manager"), QStringLiteral(" 01-02 ")}
        }
        ));

    const auto result = readDirectory(services);

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(6));
    QCOMPARE(result.value()[0].name, std::u16string(u"Alex "));
    QCOMPARE(result.value()[0].position, std::u16string(u"Branch Manager"));
    QCOMPARE(result.value()[0].birthday, std::u16string(u" 01-02 "));
    QVERIFY(result.value()[1].name.empty());
    QCOMPARE(result.value()[1].koreanName, std::u16string(u"Null Name"));
    QVERIFY(result.value()[1].birthday.empty());
    QCOMPARE(result.value()[2].name, std::u16string(u"Alpha "));
    QCOMPARE(result.value()[3].name, std::u16string(u"alpha "));
    QCOMPARE(result.value()[3].birthday, std::u16string(u" 02-04 "));
    QCOMPARE(result.value()[4].name, std::u16string(u"Beta "));
    QVERIFY(result.value()[5].name.empty());
    QCOMPARE(result.value()[5].koreanName,
        std::u16string(u"Zulu Fallback"));
    QCOMPARE(result.value()[5].position, std::u16string(u"M1"));
    QCOMPARE(result.value()[5].birthday, std::u16string(u"04-15"));
}

void NextPlatformApplicationServicesGsTeamBirthdayDirectoryReadPortTests::
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

void NextPlatformApplicationServicesGsTeamBirthdayDirectoryReadPortTests::
distinguishesUnavailableSessionFromRepositoryFailure()
{
    ApplicationServices services;
    Platform::ApplicationServicesGsTeamBirthdayDirectoryReadPort port(&services);
    auto unavailable = port.readGsTeamBirthdayDirectory();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(unavailable.error().recoverable);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery dropTable(services.databaseSession()->database());
    QVERIFY2(
        dropTable.exec(QStringLiteral("DROP TABLE gs_team")),
        qPrintable(dropTable.lastError().text())
        );
    const auto failed = port.readGsTeamBirthdayDirectory();
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());
    QVERIFY(!failed.error().recoverable);

    services.closeDatabase();
    unavailable = port.readGsTeamBirthdayDirectory();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);
}

QTEST_MAIN(
    NextPlatformApplicationServicesGsTeamBirthdayDirectoryReadPortTests)

#include "next_platform_application_services_gs_team_birthday_directory_read_port_tests.moc"
