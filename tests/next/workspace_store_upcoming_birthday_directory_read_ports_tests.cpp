#include "data/database/workspace_store.h"
#include "data/repositories/gs_team_repository.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/gs_team_member.h"
#include "domain/models/native_english_teacher.h"
#include "domain/models/teacher.h"
#include "next/platform/workspace_store_upcoming_birthday_directory_read_ports.h"

#include <QSqlError>
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
        QStringLiteral("upcoming-birthdays-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool seedDirectories(WorkspaceStore& store)
{
    Teacher korean;
    korean.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    korean.teacherEn = QStringLiteral("Kim Teacher");
    korean.preferredRomanization = QStringLiteral("Kim");
    korean.preferredName = QStringLiteral("Kimi");
    korean.birthday = QStringLiteral("02-29");
    if (!store.teacherRepository()->createTeacher(korean))
    {
        return false;
    }

    NativeEnglishTeacher native;
    native.name = QStringLiteral("Alex Native");
    native.position = QStringLiteral("NET");
    native.birthday = QStringLiteral("03-14");
    if (!store.nativeEnglishTeacherRepository()->saveDirectory({native}, {}))
    {
        return false;
    }

    GsTeamMember member;
    member.name = QStringLiteral("GS Member");
    member.koreanName = QStringLiteral("\uAE40\uD558\uB298");
    member.position = QStringLiteral("Coordinator");
    member.birthday = QStringLiteral("04-15");
    return static_cast<bool>(
        store.gsTeamRepository()->saveDirectory({member}, {})
        );
}

} // namespace

class WorkspaceStoreUpcomingBirthdayDirectoryReadPortsTests final : public QObject
{
    Q_OBJECT

private slots:
    void readsTypedBirthdayProjectionsFromTheActiveStore();
    void returnsSuccessfulEmptyProjections();
    void closedStoreIsNotFoundAndRepositoryFailureIsTechnical();
};

void WorkspaceStoreUpcomingBirthdayDirectoryReadPortsTests::
readsTypedBirthdayProjectionsFromTheActiveStore()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WorkspaceStore store;
    QVERIFY(store.open(databasePath(directory)));
    QVERIFY(seedDirectories(store));

    Platform::WorkspaceStoreUpcomingBirthdayDirectoryReadPorts ports(store);
    const auto korean = ports.readKoreanTeacherBirthdayDirectory();
    const auto native = ports.readNativeEnglishTeacherBirthdayDirectory();
    const auto gsTeam = ports.readGsTeamBirthdayDirectory();

    QVERIFY(korean);
    QVERIFY(native);
    QVERIFY(gsTeam);
    QCOMPARE(korean.value().size(), std::size_t(1));
    QCOMPARE(korean.value()[0].birthday, std::u16string(u"02-29"));
    QCOMPARE(korean.value()[0].preferredName, std::u16string(u"Kimi"));
    QCOMPARE(korean.value()[0].teacherEn, std::u16string(u"Kim Teacher"));
    QCOMPARE(native.value().size(), std::size_t(1));
    QCOMPARE(native.value()[0].name, std::u16string(u"Alex Native"));
    QCOMPARE(native.value()[0].position, std::u16string(u"NET"));
    QCOMPARE(native.value()[0].birthday, std::u16string(u"03-14"));
    QCOMPARE(gsTeam.value().size(), std::size_t(1));
    QCOMPARE(gsTeam.value()[0].name, std::u16string(u"GS Member"));
    QCOMPARE(gsTeam.value()[0].koreanName, std::u16string(u"\uAE40\uD558\uB298"));
    QCOMPARE(gsTeam.value()[0].position, std::u16string(u"Coordinator"));
    QCOMPARE(gsTeam.value()[0].birthday, std::u16string(u"04-15"));
}

void WorkspaceStoreUpcomingBirthdayDirectoryReadPortsTests::
returnsSuccessfulEmptyProjections()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WorkspaceStore store;
    QVERIFY(store.open(databasePath(directory)));
    Platform::WorkspaceStoreUpcomingBirthdayDirectoryReadPorts ports(store);

    const auto korean = ports.readKoreanTeacherBirthdayDirectory();
    const auto native = ports.readNativeEnglishTeacherBirthdayDirectory();
    const auto gsTeam = ports.readGsTeamBirthdayDirectory();

    QVERIFY(korean);
    QVERIFY(native);
    QVERIFY(gsTeam);
    QVERIFY(korean.value().empty());
    QVERIFY(native.value().empty());
    QVERIFY(gsTeam.value().empty());
}

void WorkspaceStoreUpcomingBirthdayDirectoryReadPortsTests::
closedStoreIsNotFoundAndRepositoryFailureIsTechnical()
{
    WorkspaceStore closedStore;
    Platform::WorkspaceStoreUpcomingBirthdayDirectoryReadPorts closedPorts(
        closedStore);
    const auto unavailable = closedPorts.readKoreanTeacherBirthdayDirectory();
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error().code, Domain::ErrorCode::NotFound);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WorkspaceStore store;
    QVERIFY(store.open(databasePath(directory)));
    QSqlQuery query(store.database());
    QVERIFY(query.exec(QStringLiteral("DROP TABLE native_english_teachers")));
    Platform::WorkspaceStoreUpcomingBirthdayDirectoryReadPorts ports(store);

    const auto failed = ports.readNativeEnglishTeacherBirthdayDirectory();
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
}

QTEST_GUILESS_MAIN(WorkspaceStoreUpcomingBirthdayDirectoryReadPortsTests)
#include "workspace_store_upcoming_birthday_directory_read_ports_tests.moc"
