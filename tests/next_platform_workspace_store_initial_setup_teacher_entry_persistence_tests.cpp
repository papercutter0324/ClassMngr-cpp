#include "data/database/workspace_store.h"
#include "next/application/application_runtime.h"
#include "next/application/initial_setup_lifecycle.h"
#include "next/application/initial_setup_teacher_entry_skip_policy.h"
#include "next/platform/workspace_store_gateway.h"
#include "next/platform/workspace_store_initial_setup_lifecycle_adapter.h"
#include "next/platform/workspace_store_personal_details_port.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

[[nodiscard]] QString workspaceFixturePath(const QString& fixtureName)
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("tests/fixtures/workspaces/") + fixtureName
        );
}

[[nodiscard]] bool executeWorkspaceSql(
    const QString& path,
    const QString& statement
    )
{
    const QString connectionName = QStringLiteral("teacher-entry-sql-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        );
    bool executed = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
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

[[nodiscard]] bool executeWorkspaceFixture(
    const QString& profilePath,
    const QString& fixtureName,
    QString* errorMessage
    )
{
    QFile fixture(workspaceFixturePath(fixtureName));
    if (!fixture.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = fixture.errorString();
        }
        return false;
    }

    const QStringList statements = QString::fromUtf8(fixture.readAll())
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    const QString connectionName = QStringLiteral("teacher-entry-fixture-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        );
    bool executed = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(profilePath);
        if (!database.open())
        {
            if (errorMessage)
            {
                *errorMessage = database.lastError().text();
            }
        }
        else
        {
            executed = true;
            QSqlQuery query(database);
            for (const QString& rawStatement : statements)
            {
                const QString statement = rawStatement.trimmed();
                if (statement.isEmpty())
                {
                    continue;
                }
                if (!query.exec(statement))
                {
                    if (errorMessage)
                    {
                        *errorMessage = QStringLiteral("%1: %2")
                            .arg(statement.simplified().left(160),
                                 query.lastError().text());
                    }
                    executed = false;
                    break;
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return executed;
}

[[nodiscard]] bool createWorkspaceWithFixture(
    const QString& profilePath,
    const QString& fixtureName,
    QString* errorMessage
    )
{
    {
        WorkspaceStore store;
        const RepositoryStatus opened = store.open(profilePath);
        if (!opened)
        {
            if (errorMessage)
            {
                *errorMessage = opened.error().diagnostic;
            }
            return false;
        }
        store.close();
    }
    return executeWorkspaceFixture(profilePath, fixtureName, errorMessage);
}

[[nodiscard]] std::optional<QByteArray> fileSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    return QCryptographicHash::hash(
        file.readAll(),
        QCryptographicHash::Sha256
        );
}

[[nodiscard]] std::optional<QByteArray> readFileBytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    return file.readAll();
}

struct TeacherSqlRecord final
{
    QString teacherEn;
    QString teacherKr;
    QString preferredRomanization;
    QString preferredName;
    QString roomNumber;
    QString wifiPassword;
    QString zoomId;
};

[[nodiscard]] std::optional<TeacherSqlRecord> readTeacherSqlRecord(
    const QString& profilePath
    )
{
    const QString connectionName = QStringLiteral("teacher-entry-read-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        );
    std::optional<TeacherSqlRecord> record;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(profilePath);
        if (database.open())
        {
            QSqlQuery query(database);
            if (query.exec(QStringLiteral(
                    "SELECT teacher_en, teacher_kr, preferred_romanization, "
                    "preferred_name, room_number, wifi_password, zoom_id "
                    "FROM teachers WHERE id = 1"
                    ))
                && query.next())
            {
                record = TeacherSqlRecord{
                    .teacherEn = query.value(0).toString(),
                    .teacherKr = query.value(1).toString(),
                    .preferredRomanization = query.value(2).toString(),
                    .preferredName = query.value(3).toString(),
                    .roomNumber = query.value(4).toString(),
                    .wifiPassword = query.value(5).toString(),
                    .zoomId = query.value(6).toString()
                };
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return record;
}

} // namespace

class WorkspaceStoreInitialSetupTeacherEntryPersistenceTests final : public QObject
{
    Q_OBJECT

private slots:
    void failedCreateRetryResumeAndReopenPreserveTeacherIdentity();
};

void WorkspaceStoreInitialSetupTeacherEntryPersistenceTests::
failedCreateRetryResumeAndReopenPreserveTeacherIdentity()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("teacher-entry.tps")
        );
    const QString recoveryJournalPath = temporaryDirectory.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    QString fixtureError;
    QVERIFY2(
        createWorkspaceWithFixture(
            profilePath,
            QStringLiteral("representative_startup.sql"),
            &fixtureError
            ),
        qPrintable(fixtureError)
        );
    const auto originalProfileHash = fileSha256(profilePath);
    QVERIFY(originalProfileHash.has_value());

    const Application::WorkspaceLocation profileLocation(
        profilePath.toUtf8().toStdString()
        );
    QString backupPath;
    QByteArray pendingJournal;
    {
        Platform::WorkspaceStoreGateway gateway;
        Application::ApplicationRuntime runtime(gateway);
        Platform::WorkspaceStoreInitialSetupLifecycleAdapter lifecyclePort(
            runtime.workspaceCoordinator(),
            runtime.workspaceState(),
            recoveryJournalPath
            );
        Application::InitialSetupLifecycle lifecycle(lifecyclePort);

        const auto opened = runtime.workspaceCoordinator().openWorkspace(
            Application::OpenWorkspaceRequest{profileLocation}
            );
        QVERIFY(opened);
        const auto started = lifecycle.begin(profileLocation);
        QVERIFY(started.started());
        QVERIFY(lifecycle.hasPendingSetup());
        QVERIFY(!started.originalBackup.empty());
        backupPath = QString::fromUtf8(
            started.originalBackup.value().data(),
            static_cast<qsizetype>(started.originalBackup.value().size())
            );
        QVERIFY(QFileInfo::exists(backupPath));
        const auto originalBackupHash = fileSha256(backupPath);
        QVERIFY(originalBackupHash.has_value());
        QCOMPARE(*originalBackupHash, *originalProfileHash);
        const auto journalBefore = readFileBytes(recoveryJournalPath);
        QVERIFY(journalBefore.has_value());
        pendingJournal = *journalBefore;

        QVERIFY2(
            executeWorkspaceFixture(
                profilePath,
                QStringLiteral("initial_setup_personal_details.sql"),
                &fixtureError
                ),
            qPrintable(fixtureError)
            );
        const auto personalDetails = gateway.readPersonalDetails();
        QVERIFY(personalDetails);
        QCOMPARE(personalDetails.value().name, std::string("Fixture Teacher"));
        QCOMPARE(
            personalDetails.value().zoomLoginId,
            std::string("fixture.zoom@example.test")
            );

        const Application::TeacherCreateRequest request{
            .fields = Domain::TeacherProfileFields{
                .teacherKr = u"김선생님",
                .teacherEn = u"Alex Kim",
                .preferredRomanization = u"Kim Seonsaeng",
                .preferredName = u"Alex Kim",
                .roomNumber = u"Room 12",
                .wifiPassword = u"fixture wifi secret",
                .zoomId = u"alex.zoom"
            }
        };
        QVERIFY(executeWorkspaceSql(
            profilePath,
            QStringLiteral(R"(
                CREATE TRIGGER fail_initial_setup_teacher_create
                BEFORE INSERT ON teachers
                BEGIN
                    SELECT RAISE(ABORT, 'forced teacher create failure');
                END
            )")
            ));
        const auto failedCreate = gateway.createInitialSetupTeacher(request);
        QVERIFY(!failedCreate);
        QVERIFY(QString::fromUtf8(
            failedCreate.error().message.data(),
            static_cast<qsizetype>(failedCreate.error().message.size())
            ).contains(QStringLiteral("forced teacher create failure")));
        QVERIFY(lifecycle.hasPendingSetup());
        const auto choicesAfterFailure = gateway.loadInitialSetupTeacherChoices();
        QVERIFY(choicesAfterFailure);
        QVERIFY(choicesAfterFailure.value().teachers.empty());
        const auto journalAfterFailure = readFileBytes(recoveryJournalPath);
        QVERIFY(journalAfterFailure.has_value());
        QCOMPARE(*journalAfterFailure, pendingJournal);
        const auto backupHashAfterFailure = fileSha256(backupPath);
        QVERIFY(backupHashAfterFailure.has_value());
        QCOMPARE(*backupHashAfterFailure, *originalProfileHash);

        QVERIFY(executeWorkspaceSql(
            profilePath,
            QStringLiteral("DROP TRIGGER fail_initial_setup_teacher_create")
            ));
        const auto created = gateway.createInitialSetupTeacher(request);
        QVERIFY(created);
        QCOMPARE(created.value().value(), std::string("1"));
        QVERIFY(lifecycle.hasPendingSetup());
        const auto journalAfterCreate = readFileBytes(recoveryJournalPath);
        QVERIFY(journalAfterCreate.has_value());
        QCOMPARE(*journalAfterCreate, pendingJournal);
        const auto backupHashAfterCreate = fileSha256(backupPath);
        QVERIFY(backupHashAfterCreate.has_value());
        QCOMPARE(*backupHashAfterCreate, *originalProfileHash);
        QVERIFY(lifecyclePort.closeActiveWorkspace());
        QVERIFY(lifecycle.hasPendingSetup());
        QVERIFY(QFileInfo::exists(recoveryJournalPath));
        QVERIFY(QFileInfo::exists(backupPath));

        Application::InitialSetupLifecycle resumedLifecycle(lifecyclePort);
        const auto recovery = lifecyclePort.pendingRecovery();
        QVERIFY(recovery);
        QVERIFY(recovery.value().has_value());
        const auto resumed = resumedLifecycle.resume(*recovery.value());
        QVERIFY(resumed.started());
        QVERIFY(resumedLifecycle.hasPendingSetup());
        QCOMPARE(
            resumedLifecycle.pendingProfile().value(),
            profileLocation.value()
            );
        QCOMPARE(
            resumedLifecycle.pendingOriginalBackup().value(),
            backupPath.toUtf8().toStdString()
            );
        const auto choices = gateway.loadInitialSetupTeacherChoices();
        QVERIFY(choices);
        QCOMPARE(choices.value().teachers.size(), std::size_t{1});
        const auto& teacher = choices.value().teachers.front();
        QCOMPARE(teacher.teacherId.value(), std::string("1"));
        QCOMPARE(teacher.teacherEn, std::u16string(u"Alex Kim"));
        QCOMPARE(teacher.teacherKr, std::u16string(u"김선생님"));
        QCOMPARE(
            teacher.preferredRomanization,
            std::u16string(u"Kim Seonsaeng")
            );
        QVERIFY(Application::initialSetupTeacherEntryShouldBeSkipped(
            true,
            !choices.value().teachers.empty()
            ));

        const auto persistedTeacher = readTeacherSqlRecord(profilePath);
        QVERIFY(persistedTeacher.has_value());
        QCOMPARE(persistedTeacher->teacherEn, QStringLiteral("Alex Kim"));
        QCOMPARE(persistedTeacher->teacherKr, QString::fromUtf8("김선생님"));
        QCOMPARE(
            persistedTeacher->preferredRomanization,
            QStringLiteral("Kim Seonsaeng")
            );
        QCOMPARE(persistedTeacher->preferredName, QStringLiteral("Alex Kim"));
        QCOMPARE(persistedTeacher->roomNumber, QStringLiteral("Room 12"));
        QCOMPARE(
            persistedTeacher->wifiPassword,
            QStringLiteral("fixture wifi secret")
            );
        QCOMPARE(persistedTeacher->zoomId, QStringLiteral("alex.zoom"));
        const auto resumedPersonalDetails = gateway.readPersonalDetails();
        QVERIFY(resumedPersonalDetails);
        QCOMPARE(
            resumedPersonalDetails.value().name,
            std::string("Fixture Teacher")
            );

        const auto journalAfterResume = readFileBytes(recoveryJournalPath);
        QVERIFY(journalAfterResume.has_value());
        QCOMPARE(*journalAfterResume, pendingJournal);
        const auto backupHashAfterResume = fileSha256(backupPath);
        QVERIFY(backupHashAfterResume.has_value());
        QCOMPARE(*backupHashAfterResume, *originalProfileHash);
        QVERIFY(lifecyclePort.closeActiveWorkspace());
    }
    QVERIFY(QFileInfo::exists(recoveryJournalPath));
    QVERIFY(QFileInfo::exists(backupPath));
}

QTEST_GUILESS_MAIN(WorkspaceStoreInitialSetupTeacherEntryPersistenceTests)

#include "next_platform_workspace_store_initial_setup_teacher_entry_persistence_tests.moc"
