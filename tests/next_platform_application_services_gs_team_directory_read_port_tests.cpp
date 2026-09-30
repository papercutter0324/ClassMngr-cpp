#include "core/application_services.h"
#include "data/database/database_session.h"
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
    const Status saved = services.databaseSession()
        ->gsTeamRepository()->saveDirectory(members, {});
    return static_cast<bool>(saved);
}

}

class NextPlatformApplicationServicesGsTeamDirectoryReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsMappedRowsInRepositoryOrderFromActiveSession();
    void reportsUnavailableSessionSeparatelyFromRepositoryFailure();
};

void NextPlatformApplicationServicesGsTeamDirectoryReadPortTests::
readsMappedRowsInRepositoryOrderFromActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    const auto persisted = services.databaseSession()
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

    QSqlQuery query(services.databaseSession()->database());
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

QTEST_MAIN(NextPlatformApplicationServicesGsTeamDirectoryReadPortTests)

#include "next_platform_application_services_gs_team_directory_read_port_tests.moc"
