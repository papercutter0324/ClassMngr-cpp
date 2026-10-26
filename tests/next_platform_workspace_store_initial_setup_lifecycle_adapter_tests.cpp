#include "data/database/database_schema_manager.h"
#include "data/database/workspace_store.h"
#include "next/application/initial_setup_lifecycle.h"
#include "next/application/workspace_coordinator.h"
#include "next/platform/workspace_store_gateway.h"
#include "next/platform/workspace_store_initial_setup_lifecycle_adapter.h"

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

using namespace ClassMngr::Next::Application;

namespace
{

QString representativeFixtureSqlPath()
{
    return QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/workspaces/representative_startup.sql");
}

bool executeFixtureSql(
    QSqlDatabase& database,
    const QString& sqlPath,
    QString* errorMessage
    )
{
    QFile sqlFile(sqlPath);
    if (!sqlFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        *errorMessage = sqlFile.errorString();
        return false;
    }

    QSqlQuery query(database);
    const QStringList statements = QString::fromUtf8(sqlFile.readAll())
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& rawStatement : statements)
    {
        const QString statement = rawStatement.trimmed();
        if (!statement.isEmpty() && !query.exec(statement))
        {
            *errorMessage = QStringLiteral("%1: %2")
                .arg(statement.simplified().left(160), query.lastError().text());
            return false;
        }
    }
    return true;
}

bool materializeRepresentativeProfile(
    const QString& profilePath,
    QString* errorMessage
    )
{
    const QString connectionName = QStringLiteral("setup-seed-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(profilePath);
        if (!database.open())
        {
            *errorMessage = database.lastError().text();
        }
        else
        {
            const auto schema = DatabaseSchemaManager::createSchema(database);
            if (!schema)
            {
                *errorMessage = schema.error().diagnostic;
            }
            else
            {
                success = executeFixtureSql(
                    database,
                    representativeFixtureSqlPath(),
                    errorMessage
                    );
            }
        }
        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

struct ProfileCounts final
{
    int teachers = -1;
    int classes = -1;
};

std::optional<ProfileCounts> readProfileCounts(
    const QString& profilePath,
    QString* errorMessage
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(profilePath);
    if (!opened)
    {
        *errorMessage = opened.error().diagnostic;
        return std::nullopt;
    }

    ProfileCounts counts;
    {
        QSqlQuery query(store.database());
        if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM teachers"))
            || !query.next())
        {
            *errorMessage = query.lastError().text();
            return std::nullopt;
        }
        counts.teachers = query.value(0).toInt();
        if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM classes"))
            || !query.next())
        {
            *errorMessage = query.lastError().text();
            return std::nullopt;
        }
        counts.classes = query.value(0).toInt();
    }
    store.close();
    return counts;
}

WorkspaceLocation workspaceLocation(const QString& path)
{
    return WorkspaceLocation(path.toUtf8().toStdString());
}

class StoreLifecycleFixture final
{
public:
    explicit StoreLifecycleFixture(const QString& journalPath)
        : workspaceUseCase(workspaceGateway),
          workspaceCoordinator(
              workspaceUseCase,
              workspaceState,
              selectionState
              ),
          lifecyclePort(workspaceCoordinator, workspaceState, journalPath),
          lifecycle(lifecyclePort)
    {
    }

    ClassMngr::Next::Platform::WorkspaceStoreGateway workspaceGateway;
    WorkspaceUseCase workspaceUseCase;
    WorkspaceState workspaceState;
    SelectionState selectionState;
    WorkspaceCoordinator workspaceCoordinator;
    ClassMngr::Next::Platform::WorkspaceStoreInitialSetupLifecycleAdapter
        lifecyclePort;
    InitialSetupLifecycle lifecycle;
};

QString journalPath(const QTemporaryDir& directory)
{
    return directory.filePath(QStringLiteral("state/setup-recovery.json"));
}

} // namespace

class NextPlatformWorkspaceStoreInitialSetupLifecycleAdapterTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void replacementCancelReopensRepresentativeProfile();
    void interruptedSetupCanResumeThenCancel();
    void interruptedSetupCanRestoreWithoutOpeningIncompleteProfile();
    void freshTpsProfileReopensAndExtensionRulesAreCaseInsensitive();
    void failedCreationDoesNotDamageAnOpenWorkspace();
};

void NextPlatformWorkspaceStoreInitialSetupLifecycleAdapterTests::
replacementCancelReopensRepresentativeProfile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString profilePath = directory.filePath(QStringLiteral("Teacher.TPS"));
    QString errorMessage;
    QVERIFY2(
        materializeRepresentativeProfile(profilePath, &errorMessage),
        qPrintable(errorMessage)
        );

    const auto seededCounts = readProfileCounts(profilePath, &errorMessage);
    QVERIFY2(seededCounts.has_value(), qPrintable(errorMessage));
    QCOMPARE(seededCounts->teachers, 4);
    QCOMPARE(seededCounts->classes, 8);

    StoreLifecycleFixture fixture(journalPath(directory));
    const auto started = fixture.lifecycle.begin(workspaceLocation(profilePath));
    QVERIFY2(started.started(), qPrintable(QString::fromStdString(started.error.message)));
    QVERIFY(started.workspaceClosed);
    QVERIFY(fixture.lifecycle.hasPendingSetup());
    QVERIFY(fixture.lifecyclePort.profileExists(workspaceLocation(profilePath)));
    QVERIFY(fixture.lifecyclePort.profileExists(started.originalBackup));
    QVERIFY(QFileInfo::exists(journalPath(directory)));

    const auto canceled = fixture.lifecycle.cancel();
    QCOMPARE(canceled.status, InitialSetupCancelStatus::Canceled);
    QVERIFY(!fixture.lifecycle.hasPendingSetup());
    QVERIFY(!QFileInfo::exists(journalPath(directory)));
    QVERIFY(!fixture.lifecyclePort.profileExists(started.originalBackup));

    const auto reopenedCounts = readProfileCounts(profilePath, &errorMessage);
    QVERIFY2(reopenedCounts.has_value(), qPrintable(errorMessage));
    QCOMPARE(reopenedCounts->teachers, 4);
    QCOMPARE(reopenedCounts->classes, 8);
}

void NextPlatformWorkspaceStoreInitialSetupLifecycleAdapterTests::
interruptedSetupCanResumeThenCancel()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString profilePath = directory.filePath(QStringLiteral("Resume.TPS"));
    const QString journal = journalPath(directory);
    QString errorMessage;
    QVERIFY2(
        materializeRepresentativeProfile(profilePath, &errorMessage),
        qPrintable(errorMessage)
        );

    InitialSetupRecoveryRecord recovery;
    {
        StoreLifecycleFixture fixture(journal);
        const auto started = fixture.lifecycle.begin(workspaceLocation(profilePath));
        QVERIFY2(started.started(), qPrintable(QString::fromStdString(started.error.message)));
        recovery = InitialSetupRecoveryRecord{
            workspaceLocation(profilePath),
            started.originalBackup
        };
        QVERIFY(fixture.workspaceCoordinator.closeWorkspace());
    }

    StoreLifecycleFixture restarted(journal);
    const auto pending = restarted.lifecyclePort.pendingRecovery();
    QVERIFY(pending);
    QVERIFY(pending.value().has_value());
    QCOMPARE(*pending.value(), recovery);

    const auto resumed = restarted.lifecycle.resume(recovery);
    QVERIFY2(resumed.started(), qPrintable(QString::fromStdString(resumed.error.message)));
    QVERIFY(restarted.lifecycle.hasPendingSetup());
    QVERIFY(restarted.lifecyclePort.hasActiveWorkspace());

    const auto canceled = restarted.lifecycle.cancel();
    QCOMPARE(canceled.status, InitialSetupCancelStatus::Canceled);
    const auto reopenedCounts = readProfileCounts(profilePath, &errorMessage);
    QVERIFY2(reopenedCounts.has_value(), qPrintable(errorMessage));
    QCOMPARE(reopenedCounts->teachers, 4);
    QCOMPARE(reopenedCounts->classes, 8);
    QVERIFY(!QFileInfo::exists(journal));
}

void NextPlatformWorkspaceStoreInitialSetupLifecycleAdapterTests::
interruptedSetupCanRestoreWithoutOpeningIncompleteProfile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString profilePath = directory.filePath(QStringLiteral("Restore.TPS"));
    const QString journal = journalPath(directory);
    QString errorMessage;
    QVERIFY2(
        materializeRepresentativeProfile(profilePath, &errorMessage),
        qPrintable(errorMessage)
        );

    InitialSetupRecoveryRecord recovery;
    {
        StoreLifecycleFixture fixture(journal);
        const auto started = fixture.lifecycle.begin(workspaceLocation(profilePath));
        QVERIFY2(started.started(), qPrintable(QString::fromStdString(started.error.message)));
        recovery = InitialSetupRecoveryRecord{
            workspaceLocation(profilePath),
            started.originalBackup
        };
        QVERIFY(fixture.workspaceCoordinator.closeWorkspace());
    }

    StoreLifecycleFixture restarted(journal);
    const auto restored = restarted.lifecycle.restore(recovery);
    QCOMPARE(restored.status, InitialSetupCancelStatus::Canceled);
    QVERIFY(!restarted.lifecyclePort.hasActiveWorkspace());
    QVERIFY(!QFileInfo::exists(journal));

    const auto reopenedCounts = readProfileCounts(profilePath, &errorMessage);
    QVERIFY2(reopenedCounts.has_value(), qPrintable(errorMessage));
    QCOMPARE(reopenedCounts->teachers, 4);
    QCOMPARE(reopenedCounts->classes, 8);
}

void NextPlatformWorkspaceStoreInitialSetupLifecycleAdapterTests::
freshTpsProfileReopensAndExtensionRulesAreCaseInsensitive()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString journal = journalPath(directory);
    QString errorMessage;
    const QString extensionlessPath = directory.filePath(QStringLiteral("Fresh"));
    const auto normalized = ClassMngr::Next::Platform::
        WorkspaceStoreInitialSetupLifecycleAdapter::profileLocationForSetupPath(
            ClassMngr::Next::Application::FileDialogPath(
                extensionlessPath.toUtf8().toStdString()
                )
            );
    QVERIFY(normalized);
    QCOMPARE(
        QString::fromUtf8(normalized.value().value().data(),
                          static_cast<qsizetype>(normalized.value().value().size())),
        extensionlessPath + QStringLiteral(".tps")
        );

    const QString upperCasePath = directory.filePath(QStringLiteral("Fresh.TPS"));
    const auto uppercase = ClassMngr::Next::Platform::
        WorkspaceStoreInitialSetupLifecycleAdapter::profileLocationForSetupPath(
            ClassMngr::Next::Application::FileDialogPath(
                upperCasePath.toUtf8().toStdString()
                )
            );
    QVERIFY(uppercase);
    QCOMPARE(QString::fromStdString(uppercase.value().value()), upperCasePath);

    const auto unsupported = ClassMngr::Next::Platform::
        WorkspaceStoreInitialSetupLifecycleAdapter::profileLocationForSetupPath(
            ClassMngr::Next::Application::FileDialogPath(
                directory.filePath(QStringLiteral("wrong.db")).toUtf8().toStdString()
                )
            );
    QVERIFY(!unsupported);

    StoreLifecycleFixture fixture(journal);
    const auto started = fixture.lifecycle.begin(uppercase.value());
    QVERIFY2(started.started(), qPrintable(QString::fromStdString(started.error.message)));
    QVERIFY(QFileInfo::exists(upperCasePath));
    QVERIFY(fixture.workspaceCoordinator.closeWorkspace());

    const auto freshCounts = readProfileCounts(upperCasePath, &errorMessage);
    QVERIFY2(freshCounts.has_value(), qPrintable(errorMessage));
    QCOMPARE(freshCounts->teachers, 0);
    QCOMPARE(freshCounts->classes, 0);

    const auto canceled = fixture.lifecycle.cancel();
    QCOMPARE(canceled.status, InitialSetupCancelStatus::Canceled);
    QVERIFY(!QFileInfo::exists(upperCasePath));
}

void NextPlatformWorkspaceStoreInitialSetupLifecycleAdapterTests::
failedCreationDoesNotDamageAnOpenWorkspace()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString existingProfile = directory.filePath(QStringLiteral("Existing.tps"));
    QString errorMessage;
    QVERIFY2(
        materializeRepresentativeProfile(existingProfile, &errorMessage),
        qPrintable(errorMessage)
        );

    StoreLifecycleFixture fixture(journalPath(directory));
    const auto opened = fixture.workspaceCoordinator.openWorkspace(
        OpenWorkspaceRequest{workspaceLocation(existingProfile)}
        );
    QVERIFY(opened);

    const QString invalidTarget = directory.filePath(QStringLiteral("directory.tps"));
    QVERIFY(QDir().mkpath(invalidTarget));
    const auto failed = fixture.lifecycle.begin(workspaceLocation(invalidTarget));
    QCOMPARE(failed.failure, InitialSetupBeginFailure::CreateWorkspace);
    QCOMPARE(failed.rollback.status, InitialSetupCancelStatus::Canceled);
    QVERIFY(!QFileInfo::exists(journalPath(directory)));
    QVERIFY(QFileInfo::exists(existingProfile));

    const auto reopened = fixture.workspaceCoordinator.openWorkspace(
        OpenWorkspaceRequest{workspaceLocation(existingProfile)}
        );
    QVERIFY(reopened);
    QVERIFY(fixture.workspaceCoordinator.closeWorkspace());
    const auto counts = readProfileCounts(existingProfile, &errorMessage);
    QVERIFY2(counts.has_value(), qPrintable(errorMessage));
    QCOMPARE(counts->teachers, 4);
    QCOMPARE(counts->classes, 8);
}

QTEST_GUILESS_MAIN(NextPlatformWorkspaceStoreInitialSetupLifecycleAdapterTests)

#include "next_platform_workspace_store_initial_setup_lifecycle_adapter_tests.moc"
