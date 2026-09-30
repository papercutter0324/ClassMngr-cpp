#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/gs_team_repository.h"
#include "domain/models/gs_team_member.h"
#include "next/application/gs_team_directory_save.h"
#include "next/platform/application_services_gs_team_directory_save_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("gs-team-directory-save-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

GsTeamMember member(const QString& name, const QString& koreanName)
{
    GsTeamMember value;
    value.name = name;
    value.koreanName = koreanName;
    value.position = QStringLiteral("M1");
    value.phoneNumber = QStringLiteral("010-1000-0001");
    value.birthday = QStringLiteral("01-02");
    return value;
}

bool seedDirectory(ApplicationServices& services)
{
    return static_cast<bool>(services.databaseSession()
        ->gsTeamRepository()->saveDirectory(
            {
                member(QStringLiteral("Update Target"),
                    QStringLiteral("\uC218\uC815 \uB300\uC0C1")),
                member(QStringLiteral("Delete Target"),
                    QStringLiteral("\uC0AD\uC81C \uB300\uC0C1"))
            },
            {}
            ));
}

Application::GsTeamDirectorySaveRow saveRow(
    const QString& name,
    const QString& koreanName,
    const std::optional<int> id = std::nullopt
    )
{
    Application::GsTeamDirectorySaveRow row;
    if (id)
    {
        row.id = Domain::GsTeamMemberId(*id);
    }
    row.name = name.toStdU16String();
    row.koreanName = koreanName.toStdU16String();
    row.position = QStringLiteral("Team Leader").toStdU16String();
    row.phoneNumber = QStringLiteral("010-2222-3333").toStdU16String();
    row.birthday = QStringLiteral("02-29").toStdU16String();
    row.normalizedEnglishNameKey = name.simplified().toCaseFolded().toStdU16String();
    row.normalizedKoreanNameKey =
        koreanName.simplified().toCaseFolded().toStdU16String();
    row.birthdayIsValid = true;
    return row;
}

GsTeamMember findById(
    const QList<GsTeamMember>& members,
    const int id
    )
{
    for (const GsTeamMember& value : members)
    {
        if (value.id == id)
        {
            return value;
        }
    }
    return {};
}

}

class NextPlatformApplicationServicesGsTeamDirectorySavePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void mapsFiveFieldsTypedIdsAndDeletedIdsThroughTheActiveRepository();
    void reportsMissingAndClosedSessionsAsUnavailableWithoutLegacyFallback();
    void reportsRepositoryFailureAndTransactionRollsBackAllChanges();
};

void NextPlatformApplicationServicesGsTeamDirectorySavePortTests::
mapsFiveFieldsTypedIdsAndDeletedIdsThroughTheActiveRepository()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));
    const auto original = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(original);
    QCOMPARE(original->size(), 2);

    int updateId = -1;
    int deleteId = -1;
    for (const GsTeamMember& value : *original)
    {
        if (value.name == QStringLiteral("Update Target"))
        {
            updateId = value.id;
        }
        else if (value.name == QStringLiteral("Delete Target"))
        {
            deleteId = value.id;
        }
    }
    QVERIFY(updateId > 0);
    QVERIFY(deleteId > 0);

    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        saveRow(QStringLiteral("Updated"),
            QStringLiteral("\uC218\uC815"), updateId),
        saveRow(QStringLiteral("Inserted"),
            QStringLiteral("\uC2E0\uADDC"))
    };
    request.deletedIds = {Domain::GsTeamMemberId(deleteId)};

    Platform::ApplicationServicesGsTeamDirectorySavePort port(&services);
    QVERIFY(port.hasActiveSession());
    const Domain::Result<void> saved = port.saveGsTeamDirectory(request);
    QVERIFY(saved);

    const auto persisted = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 2);
    const GsTeamMember updated = findById(*persisted, updateId);
    QCOMPARE(updated.id, updateId);
    QCOMPARE(updated.name, QStringLiteral("Updated"));
    QCOMPARE(updated.koreanName, QStringLiteral("\uC218\uC815"));
    QCOMPARE(updated.position, QStringLiteral("Team Leader"));
    QCOMPARE(updated.phoneNumber, QStringLiteral("010-2222-3333"));
    QCOMPARE(updated.birthday, QStringLiteral("02-29"));

    bool foundInserted = false;
    for (const GsTeamMember& value : *persisted)
    {
        if (value.name == QStringLiteral("Inserted"))
        {
            foundInserted = true;
            QVERIFY(value.id > 0);
            QVERIFY(value.id != updateId);
            QCOMPARE(value.koreanName, QStringLiteral("\uC2E0\uADDC"));
            QCOMPARE(value.position, QStringLiteral("Team Leader"));
            QCOMPARE(value.phoneNumber, QStringLiteral("010-2222-3333"));
            QCOMPARE(value.birthday, QStringLiteral("02-29"));
        }
        QVERIFY(value.name != QStringLiteral("Delete Target"));
    }
    QVERIFY(foundInserted);
}

void NextPlatformApplicationServicesGsTeamDirectorySavePortTests::
reportsMissingAndClosedSessionsAsUnavailableWithoutLegacyFallback()
{
    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        saveRow(QStringLiteral("Valid"), QStringLiteral("\uAC00"))
    };

    ApplicationServices services;
    // The compatibility facades remain available on ApplicationServices,
    // but this port must require the active session directly.
    QVERIFY(services.dataService());
    QVERIFY(services.teacherService());
    Platform::ApplicationServicesGsTeamDirectorySavePort port(&services);
    QVERIFY(!port.hasActiveSession());
    auto result = port.saveGsTeamDirectory(request);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(port.hasActiveSession());
    services.closeDatabase();
    QVERIFY(!port.hasActiveSession());

    result = port.saveGsTeamDirectory(request);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
}

void NextPlatformApplicationServicesGsTeamDirectorySavePortTests::
reportsRepositoryFailureAndTransactionRollsBackAllChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));
    const auto original = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(original);

    int updateId = -1;
    int deleteId = -1;
    for (const GsTeamMember& value : *original)
    {
        if (value.name == QStringLiteral("Update Target"))
        {
            updateId = value.id;
        }
        else if (value.name == QStringLiteral("Delete Target"))
        {
            deleteId = value.id;
        }
    }
    QVERIFY(updateId > 0);
    QVERIFY(deleteId > 0);

    QSqlQuery createTrigger(services.databaseSession()->database());
    QVERIFY2(createTrigger.exec(QStringLiteral(
        "CREATE TRIGGER reject_gs_team_insert BEFORE INSERT ON gs_team "
        "BEGIN SELECT RAISE(ABORT, 'reject insert'); END")),
        qPrintable(createTrigger.lastError().text()));

    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        saveRow(QStringLiteral("Updated"), QStringLiteral("\u66F4\u65B0"),
            updateId),
        saveRow(QStringLiteral("Inserted"), QStringLiteral("\u65B0\u898F"))
    };
    request.deletedIds = {Domain::GsTeamMemberId(deleteId)};

    Platform::ApplicationServicesGsTeamDirectorySavePort port(&services);
    const Domain::Result<void> saved = port.saveGsTeamDirectory(request);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!saved.error().message.empty());
    QVERIFY(!saved.error().recoverable);

    const auto after = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(after);
    QCOMPARE(after->size(), original->size());
    const GsTeamMember restoredUpdate = findById(*after, updateId);
    const GsTeamMember restoredDelete = findById(*after, deleteId);
    QCOMPARE(restoredUpdate.name, QStringLiteral("Update Target"));
    QCOMPARE(restoredUpdate.koreanName,
        QStringLiteral("\uC218\uC815 \uB300\uC0C1"));
    QCOMPARE(restoredDelete.name, QStringLiteral("Delete Target"));
    QVERIFY(findById(*after, deleteId).id > 0);
}

QTEST_MAIN(NextPlatformApplicationServicesGsTeamDirectorySavePortTests)

#include "next_platform_application_services_gs_team_directory_save_port_tests.moc"
