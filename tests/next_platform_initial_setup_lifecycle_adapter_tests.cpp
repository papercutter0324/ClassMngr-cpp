#include "core/application_services.h"
#include "next/application/initial_setup_lifecycle.h"
#include "next/application/workspace_coordinator.h"
#include "next/application/workspace_state.h"
#include "next/platform/application_services_workspace_port.h"
#include "next/platform/initial_setup_lifecycle_adapter.h"
#include "next/platform/legacy_workspace_gateway.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClassMngr::Next::Application;

namespace
{

bool writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }
    return file.write(contents) == contents.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return file.readAll();
}

QString fromLocation(const WorkspaceLocation& location)
{
    return QString::fromUtf8(
        location.value().data(),
        static_cast<qsizetype>(location.value().size())
        );
}

class LifecycleFixture final
{
public:
    LifecycleFixture()
        : workspacePort(services),
          workspaceGateway(workspacePort),
          workspaceUseCase(workspaceGateway),
          workspaceCoordinator(
              workspaceUseCase,
              workspaceState,
              selectionState
              ),
          lifecyclePort(services, workspaceCoordinator, workspaceState),
          lifecycle(lifecyclePort)
    {
    }

    ApplicationServices services;
    ClassMngr::Next::Platform::ApplicationServicesWorkspacePort workspacePort;
    ClassMngr::Next::Platform::LegacyWorkspaceGateway workspaceGateway;
    WorkspaceUseCase workspaceUseCase;
    WorkspaceState workspaceState;
    SelectionState selectionState;
    WorkspaceCoordinator workspaceCoordinator;
    ClassMngr::Next::Platform::InitialSetupLifecycleAdapter lifecyclePort;
    InitialSetupLifecycle lifecycle;
};

} // namespace

class NextPlatformInitialSetupLifecycleAdapterTests final : public QObject
{
    Q_OBJECT

private slots:
    void actualProfileReplacementCancelRestoresOriginal();
    void failedWorkspaceCreationWithoutOriginalLeavesNoTarget();
    void renameFailureNeverOverwritesEitherProfile();
    void removalFailureLeavesRecoveryDirectoryIntact();
    void recoveryLocationsAreUniqueSiblingPaths();
};

void NextPlatformInitialSetupLifecycleAdapterTests::
actualProfileReplacementCancelRestoresOriginal()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString target = root.filePath(QStringLiteral("teacher.tps"));
    const QByteArray original = QByteArrayLiteral("original profile contents");
    QVERIFY(writeFile(target, original));

    LifecycleFixture fixture;
    const auto started = fixture.lifecycle.begin(WorkspaceLocation(
        target.toUtf8().toStdString()
        ));

    QVERIFY(started.started());
    QVERIFY(started.workspaceClosed);
    QVERIFY(fixture.services.hasOpenDatabase());
    QVERIFY(QFileInfo::exists(target));
    QVERIFY(QFileInfo::exists(fromLocation(started.originalBackup)));
    QCOMPARE(readFile(fromLocation(started.originalBackup)), original);

    const auto canceled = fixture.lifecycle.cancel();

    QCOMPARE(canceled.status, InitialSetupCancelStatus::Canceled);
    QVERIFY(!fixture.services.hasOpenDatabase());
    QCOMPARE(readFile(target), original);
    QVERIFY(!QFileInfo::exists(fromLocation(started.originalBackup)));
    QVERIFY(QDir(root.path()).entryList(
        QStringList{QStringLiteral("*.recovery-*")},
        QDir::Files | QDir::Hidden
        ).isEmpty());
}

void NextPlatformInitialSetupLifecycleAdapterTests::
failedWorkspaceCreationWithoutOriginalLeavesNoTarget()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString target = root.filePath(QStringLiteral("invalid-profile"))
        + QChar::Null
        + QStringLiteral(".tps");

    LifecycleFixture fixture;
    const auto started = fixture.lifecycle.begin(WorkspaceLocation(
        target.toUtf8().toStdString()
        ));

    QCOMPARE(started.failure, InitialSetupBeginFailure::CreateWorkspace);
    QVERIFY(!started.error.message.empty());
    QCOMPARE(started.rollback.status, InitialSetupCancelStatus::Canceled);
    QVERIFY(!fixture.lifecycle.hasPendingSetup());
    QVERIFY(!fixture.services.hasOpenDatabase());
    QVERIFY(!QFileInfo::exists(target));
    QVERIFY(started.originalBackup.empty());
}

void NextPlatformInitialSetupLifecycleAdapterTests::
renameFailureNeverOverwritesEitherProfile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString originalPath = root.filePath(QStringLiteral("original.tps"));
    const QString setupPath = root.filePath(QStringLiteral("setup.tps"));
    QVERIFY(writeFile(originalPath, QByteArrayLiteral("original")));
    QVERIFY(writeFile(setupPath, QByteArrayLiteral("incomplete setup")));

    LifecycleFixture fixture;
    const bool moved = fixture.lifecyclePort.moveProfile(
        WorkspaceLocation(originalPath.toUtf8().toStdString()),
        WorkspaceLocation(setupPath.toUtf8().toStdString())
        );

    QVERIFY(!moved);
    QCOMPARE(readFile(originalPath), QByteArrayLiteral("original"));
    QCOMPARE(readFile(setupPath), QByteArrayLiteral("incomplete setup"));
}

void NextPlatformInitialSetupLifecycleAdapterTests::
removalFailureLeavesRecoveryDirectoryIntact()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString recoveryDirectory = root.filePath(QStringLiteral("recovery"));
    QVERIFY(QDir().mkpath(recoveryDirectory));
    const QString originalPath = QDir(recoveryDirectory).filePath(
        QStringLiteral("original.tps")
        );
    QVERIFY(writeFile(originalPath, QByteArrayLiteral("recoverable original")));

    LifecycleFixture fixture;
    const bool removed = fixture.lifecyclePort.removeProfile(
        WorkspaceLocation(recoveryDirectory.toUtf8().toStdString())
        );

    QVERIFY(!removed);
    QVERIFY(QFileInfo::exists(recoveryDirectory));
    QCOMPARE(readFile(originalPath), QByteArrayLiteral("recoverable original"));
}

void NextPlatformInitialSetupLifecycleAdapterTests::
recoveryLocationsAreUniqueSiblingPaths()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString target = root.filePath(QStringLiteral("teacher.tps"));
    LifecycleFixture fixture;

    const auto first = fixture.lifecyclePort.recoveryLocationFor(
        WorkspaceLocation(target.toUtf8().toStdString())
        );
    const auto second = fixture.lifecyclePort.recoveryLocationFor(
        WorkspaceLocation(target.toUtf8().toStdString())
        );

    QVERIFY(!first.empty());
    QVERIFY(!second.empty());
    QVERIFY(first != second);
    QCOMPARE(QFileInfo(fromLocation(first)).absolutePath(), root.path());
    QVERIFY(
        QFileInfo(fromLocation(first))
            .fileName()
            .startsWith(QStringLiteral(".teacher.tps.initial-setup-"))
        );
    QVERIFY(fromLocation(first).endsWith(QStringLiteral(".backup")));
}

QTEST_GUILESS_MAIN(NextPlatformInitialSetupLifecycleAdapterTests)

#include "next_platform_initial_setup_lifecycle_adapter_tests.moc"
