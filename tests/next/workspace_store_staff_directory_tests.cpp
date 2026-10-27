#include "next/platform/workspace_store_gateway.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

namespace
{

bool executeSql(const QString& path, const QString& statement)
{
    const QString connectionName = QStringLiteral("staff-directory-test-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool executed = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            executed = query.exec(statement);
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return executed;
}

[[nodiscard]] Application::WorkspaceLocation location(const QString& path)
{
    return Application::WorkspaceLocation(path.toUtf8().toStdString());
}

} // namespace

class WorkspaceStoreStaffDirectoryTests final : public QObject
{
    Q_OBJECT

private slots:
    void readsBothBranchesInRepositoryOrderWithTheirOwnFields();
    void readFailureCanBeRetriedWithoutReplacingTheWorkspaceSession();
    void nativeValidationAndTransactionRetryDoNotTouchGsRows();
    void gsValidationAndPersistenceDoNotTouchNativeRows();
    void gsReadFailureCanBeRetriedWithoutReplacingTheWorkspaceSession();
    void gsTransactionalSaveFailurePreservesRowsAndCanRetry();
};

void WorkspaceStoreStaffDirectoryTests::
readsBothBranchesInRepositoryOrderWithTheirOwnFields()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("staff.tps"));
    Platform::WorkspaceStoreGateway gateway;
    QVERIFY(gateway.createWorkspace({.location = location(path)}));
    QVERIFY(executeSql(path, QStringLiteral(
        "INSERT INTO native_english_teachers "
        "(id,name,position,phone_number,birthday,nationality,email) VALUES "
        "(1,'Zoe','NET','010-1111','01-02','CA','zoe@example.test'),"
        "(2,'Amy','Co-ordinator','010-2222','02-03','US','amy@example.test')")));
    QVERIFY(executeSql(path, QStringLiteral(
        "INSERT INTO gs_team "
        "(id,name,korean_name,position,phone_number,birthday) VALUES "
        "(1,'Gina','지나','C1','010-3333','03-04'),"
        "(2,'Ari','아리','Branch Manager','010-4444','04-05')")));

    const auto native = gateway.readNativeEnglishTeacherDirectory();
    QVERIFY(native);
    QCOMPARE(native.value().size(), std::size_t(2));
    QCOMPARE(native.value()[0].id, Domain::NativeEnglishTeacherId(2));
    QCOMPARE(native.value()[0].name, std::u16string(u"Amy"));
    QCOMPARE(native.value()[0].email, std::u16string(u"amy@example.test"));
    QCOMPARE(native.value()[1].id, Domain::NativeEnglishTeacherId(1));
    QCOMPARE(native.value()[1].position, std::u16string(u"NET"));

    const auto gs = gateway.readGsTeamDirectory();
    QVERIFY(gs);
    QCOMPARE(gs.value().size(), std::size_t(2));
    QCOMPARE(gs.value()[0].id, Domain::GsTeamMemberId(2));
    QCOMPARE(gs.value()[0].name, std::u16string(u"Ari"));
    QCOMPARE(gs.value()[0].koreanName, std::u16string(u"아리"));
    QCOMPARE(gs.value()[1].id, Domain::GsTeamMemberId(1));
    QCOMPARE(gs.value()[1].position, std::u16string(u"C1"));
}

void WorkspaceStoreStaffDirectoryTests::
readFailureCanBeRetriedWithoutReplacingTheWorkspaceSession()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("read-retry.tps"));
    Platform::WorkspaceStoreGateway gateway;
    const auto opened = gateway.createWorkspace({.location = location(path)});
    QVERIFY(opened);
    QVERIFY(executeSql(path, QStringLiteral(
        "INSERT INTO native_english_teachers "
        "(id,name,position,phone_number,birthday,nationality,email) VALUES "
        "(1,'Retry Row','NET','010-1111','01-02','CA','retry@example.test')")));

    const auto initial = gateway.readNativeEnglishTeacherDirectory();
    QVERIFY(initial);
    QCOMPARE(initial.value().size(), std::size_t(1));
    QVERIFY(executeSql(path, QStringLiteral(
        "ALTER TABLE native_english_teachers RENAME TO native_english_teachers_hold")));
    const auto failed = gateway.readNativeEnglishTeacherDirectory();
    QVERIFY(!failed);
    QVERIFY(executeSql(path, QStringLiteral(
        "ALTER TABLE native_english_teachers_hold RENAME TO native_english_teachers")));

    const auto retried = gateway.readNativeEnglishTeacherDirectory();
    QVERIFY(retried);
    QCOMPARE(retried.value().size(), std::size_t(1));
    QCOMPARE(retried.value()[0].id, Domain::NativeEnglishTeacherId(1));
    QCOMPARE(retried.value()[0].name, std::u16string(u"Retry Row"));
}

void WorkspaceStoreStaffDirectoryTests::
nativeValidationAndTransactionRetryDoNotTouchGsRows()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("native.tps"));
    Platform::WorkspaceStoreGateway gateway;
    QVERIFY(gateway.createWorkspace({.location = location(path)}));
    QVERIFY(executeSql(path, QStringLiteral(
        "INSERT INTO native_english_teachers "
        "(id,name,position,phone_number,birthday,nationality,email) VALUES "
        "(1,'Amy','Co-ordinator','010-1111','01-02','CA','amy@example.test'),"
        "(2,'Zoe','NET','010-2222','02-03','US','zoe@example.test')")));
    QVERIFY(executeSql(path, QStringLiteral(
        "INSERT INTO gs_team (id,name,korean_name,position,phone_number,birthday) "
        "VALUES (11,'GS One','하나','M1','010-9999','09-10')")));

    Application::NativeEnglishTeacherDirectorySaveRequest duplicate;
    duplicate.rows = {
        {.id = Domain::NativeEnglishTeacherId(1), .name = u"Morgan Lee"},
        {.id = Domain::NativeEnglishTeacherId(2), .name = u"  morgan   lee  "}
    };
    auto invalid = gateway.saveNativeEnglishTeacherDirectory(duplicate);
    QVERIFY(!invalid.saved);
    QCOMPARE(invalid.validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::DuplicateName);

    Application::NativeEnglishTeacherDirectorySaveRequest request;
    request.rows = {
        {.id = Domain::NativeEnglishTeacherId(1), .name = u"Amy Revised",
         .position = u"Co-ordinator", .phoneNumber = u"010-1111",
         .email = u"amy@example.test", .birthday = u"01-02", .nationality = u"CA"},
        {.id = Domain::NativeEnglishTeacherId(2), .name = u"Zoe Revised",
         .position = u"NET", .phoneNumber = u"010-2222",
         .email = u"zoe@example.test", .birthday = u"02-03", .nationality = u"US"}
    };
    QVERIFY(executeSql(path, QStringLiteral(
        "CREATE TRIGGER reject_native_update BEFORE UPDATE ON native_english_teachers "
        "WHEN OLD.id=2 BEGIN SELECT RAISE(ABORT, 'injected directory failure'); END")));
    auto failed = gateway.saveNativeEnglishTeacherDirectory(request);
    QVERIFY(!failed.saved);
    QVERIFY(failed.error.has_value());
    auto unchanged = gateway.readNativeEnglishTeacherDirectory();
    QVERIFY(unchanged);
    QCOMPARE(unchanged.value()[0].name, std::u16string(u"Amy"));
    QCOMPARE(unchanged.value()[1].name, std::u16string(u"Zoe"));
    const auto gsBeforeRetry = gateway.readGsTeamDirectory();
    QVERIFY(gsBeforeRetry);
    QCOMPARE(gsBeforeRetry.value().size(), std::size_t(1));
    QCOMPARE(gsBeforeRetry.value()[0].id, Domain::GsTeamMemberId(11));

    QVERIFY(executeSql(path, QStringLiteral("DROP TRIGGER reject_native_update")));
    const auto retried = gateway.saveNativeEnglishTeacherDirectory(request);
    QVERIFY(retried.saved);
    const auto updated = gateway.readNativeEnglishTeacherDirectory();
    QVERIFY(updated);
    QCOMPARE(updated.value()[0].name, std::u16string(u"Amy Revised"));
    QCOMPARE(updated.value()[1].name, std::u16string(u"Zoe Revised"));
    const auto gsAfterRetry = gateway.readGsTeamDirectory();
    QVERIFY(gsAfterRetry);
    QCOMPARE(gsAfterRetry.value()[0].id, Domain::GsTeamMemberId(11));
}

void WorkspaceStoreStaffDirectoryTests::
gsValidationAndPersistenceDoNotTouchNativeRows()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("gs.tps"));
    Platform::WorkspaceStoreGateway gateway;
    QVERIFY(gateway.createWorkspace({.location = location(path)}));
    QVERIFY(executeSql(path, QStringLiteral(
        "INSERT INTO native_english_teachers "
        "(id,name,position,phone_number,birthday,nationality,email) VALUES "
        "(4,'Native One','NET','010-4444','04-05','CA','native@example.test')")));
    QVERIFY(executeSql(path, QStringLiteral(
        "INSERT INTO gs_team (id,name,korean_name,position,phone_number,birthday) "
        "VALUES (1,'Ari','아리','M1','010-1111','01-02'),"
        "(2,'Gina','지나','C1','010-2222','02-03')")));

    Application::GsTeamDirectorySaveRequest invalid;
    invalid.rows = {
        {.id = Domain::GsTeamMemberId(1), .name = u"Ari", .koreanName = u"아리"},
        {.id = Domain::GsTeamMemberId(2), .name = u"  ari  ", .koreanName = u"지나"}
    };
    const auto rejected = gateway.saveGsTeamDirectory(invalid);
    QVERIFY(!rejected.saved);
    QCOMPARE(rejected.validation.issue,
        Application::GsTeamDirectorySaveIssue::DuplicateEnglishName);

    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        {.id = Domain::GsTeamMemberId(1), .name = u"Ari Revised",
         .koreanName = u"아리", .position = u"Custom role",
         .phoneNumber = u"010-1111", .birthday = u"01-02"},
        {.id = Domain::GsTeamMemberId(2), .name = u"Gina",
         .koreanName = u"지나", .position = u"C1",
         .phoneNumber = u"010-2222", .birthday = u"02-03"}
    };
    QVERIFY(gateway.saveGsTeamDirectory(request).saved);
    const auto gs = gateway.readGsTeamDirectory();
    QVERIFY(gs);
    QCOMPARE(gs.value()[1].id, Domain::GsTeamMemberId(1));
    QCOMPARE(gs.value()[1].name, std::u16string(u"Ari Revised"));
    QCOMPARE(gs.value()[1].position, std::u16string(u"Custom role"));
    const auto native = gateway.readNativeEnglishTeacherDirectory();
    QVERIFY(native);
    QCOMPARE(native.value().size(), std::size_t(1));
    QCOMPARE(native.value()[0].id, Domain::NativeEnglishTeacherId(4));
    QCOMPARE(native.value()[0].name, std::u16string(u"Native One"));
}

void WorkspaceStoreStaffDirectoryTests::
gsReadFailureCanBeRetriedWithoutReplacingTheWorkspaceSession()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("gs-read-retry.tps"));
    Platform::WorkspaceStoreGateway gateway;
    const auto opened = gateway.createWorkspace({.location = location(path)});
    QVERIFY(opened);
    QVERIFY(executeSql(path, QStringLiteral(
        "INSERT INTO native_english_teachers "
        "(id,name,position,phone_number,birthday,nationality,email) VALUES "
        "(7,'Native One','NET','010-7777','07-08','CA','native@example.test')")));
    QVERIFY(executeSql(path, QStringLiteral(
        "INSERT INTO gs_team (id,name,korean_name,position,phone_number,birthday) "
        "VALUES (21,'GS Retry','리트라이','M1','010-2121','02-01')")));

    const auto initial = gateway.readGsTeamDirectory();
    QVERIFY(initial);
    QCOMPARE(initial.value().size(), std::size_t(1));
    QCOMPARE(initial.value()[0].id, Domain::GsTeamMemberId(21));
    QVERIFY(executeSql(path, QStringLiteral(
        "ALTER TABLE gs_team RENAME TO gs_team_hold")));
    const auto failed = gateway.readGsTeamDirectory();
    QVERIFY(!failed);
    QVERIFY(executeSql(path, QStringLiteral(
        "ALTER TABLE gs_team_hold RENAME TO gs_team")));

    const auto retried = gateway.readGsTeamDirectory();
    QVERIFY(retried);
    QCOMPARE(retried.value().size(), std::size_t(1));
    QCOMPARE(retried.value()[0].id, Domain::GsTeamMemberId(21));
    QCOMPARE(retried.value()[0].name, std::u16string(u"GS Retry"));
    QCOMPARE(retried.value()[0].koreanName, std::u16string(u"리트라이"));
    const auto native = gateway.readNativeEnglishTeacherDirectory();
    QVERIFY(native);
    QCOMPARE(native.value().size(), std::size_t(1));
    QCOMPARE(native.value()[0].id, Domain::NativeEnglishTeacherId(7));
}

void WorkspaceStoreStaffDirectoryTests::
gsTransactionalSaveFailurePreservesRowsAndCanRetry()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("gs-save-retry.tps"));
    Platform::WorkspaceStoreGateway gateway;
    QVERIFY(gateway.createWorkspace({.location = location(path)}));
    QVERIFY(executeSql(path, QStringLiteral(
        "INSERT INTO native_english_teachers "
        "(id,name,position,phone_number,birthday,nationality,email) VALUES "
        "(7,'Native One','NET','010-7777','07-08','CA','native@example.test')")));
    QVERIFY(executeSql(path, QStringLiteral(
        "INSERT INTO gs_team (id,name,korean_name,position,phone_number,birthday) "
        "VALUES (21,'GS One','하나','M1','010-2121','02-01'),"
        "(22,'GS Two','둘','C1','010-2222','02-02')")));

    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        {.id = Domain::GsTeamMemberId(21), .name = u"GS One Revised",
         .koreanName = u"하나 수정", .position = u"M2",
         .phoneNumber = u"010-2121", .birthday = u"02-01"},
        {.id = Domain::GsTeamMemberId(22), .name = u"GS Two Revised",
         .koreanName = u"둘 수정", .position = u"C2",
         .phoneNumber = u"010-2222", .birthday = u"02-02"}
    };
    QVERIFY(executeSql(path, QStringLiteral(
        "CREATE TRIGGER reject_gs_update BEFORE UPDATE ON gs_team "
        "WHEN OLD.id=22 BEGIN SELECT RAISE(ABORT, 'injected GS directory failure'); END")));
    const auto failed = gateway.saveGsTeamDirectory(request);
    QVERIFY(!failed.saved);
    QVERIFY(failed.error.has_value());
    const auto unchanged = gateway.readGsTeamDirectory();
    QVERIFY(unchanged);
    QCOMPARE(unchanged.value()[0].name, std::u16string(u"GS One"));
    QCOMPARE(unchanged.value()[1].name, std::u16string(u"GS Two"));
    QCOMPARE(unchanged.value()[0].koreanName, std::u16string(u"하나"));
    const auto nativeBeforeRetry = gateway.readNativeEnglishTeacherDirectory();
    QVERIFY(nativeBeforeRetry);
    QCOMPARE(nativeBeforeRetry.value()[0].name, std::u16string(u"Native One"));

    QVERIFY(executeSql(path, QStringLiteral("DROP TRIGGER reject_gs_update")));
    const auto retried = gateway.saveGsTeamDirectory(request);
    QVERIFY(retried.saved);
    const auto updated = gateway.readGsTeamDirectory();
    QVERIFY(updated);
    QCOMPARE(updated.value()[0].id, Domain::GsTeamMemberId(21));
    QCOMPARE(updated.value()[0].name, std::u16string(u"GS One Revised"));
    QCOMPARE(updated.value()[0].koreanName, std::u16string(u"하나 수정"));
    QCOMPARE(updated.value()[1].id, Domain::GsTeamMemberId(22));
    QCOMPARE(updated.value()[1].name, std::u16string(u"GS Two Revised"));
    QCOMPARE(updated.value()[1].position, std::u16string(u"C2"));
    const auto nativeAfterRetry = gateway.readNativeEnglishTeacherDirectory();
    QVERIFY(nativeAfterRetry);
    QCOMPARE(nativeAfterRetry.value()[0].id, Domain::NativeEnglishTeacherId(7));
    QCOMPARE(nativeAfterRetry.value()[0].name, std::u16string(u"Native One"));
}

QTEST_MAIN(WorkspaceStoreStaffDirectoryTests)
#include "workspace_store_staff_directory_tests.moc"
