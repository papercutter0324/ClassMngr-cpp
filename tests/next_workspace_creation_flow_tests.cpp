#include "data/database/database_schema_manager.h"
#include "data/database/workspace_store.h"
#include "next/application/application_runtime.h"
#include "next/application/normal_workspace_creation_flow.h"
#include "next/platform/workspace_profile_path_policy.h"
#include "next/platform/workspace_store_creation_lifecycle_adapter.h"
#include "next/platform/workspace_store_gateway.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Domain::OperationError testError(const char* message)
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = message,
        .recoverable = true
    };
}

Application::WorkspaceLocation location(const QString& path)
{
    return Application::WorkspaceLocation(path.toUtf8().toStdString());
}

QString pathFor(const Application::WorkspaceLocation& value)
{
    return QString::fromUtf8(
        value.value().data(),
        static_cast<qsizetype>(value.value().size())
        );
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

QByteArray readBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

class RecordingFileDialog final : public Application::FileDialogPort
{
public:
    explicit RecordingFileDialog(QString selectedPath = {})
        : selectedPath(std::move(selectedPath))
    {
    }

    Domain::Result<Application::FileDialogResponse<Application::FileDialogPath>>
    openOneFile(const Application::OpenFileDialogRequest&) override
    {
        return Domain::Result<
            Application::FileDialogResponse<Application::FileDialogPath>>::success({});
    }

    Domain::Result<Application::FileDialogResponse<
        std::vector<Application::FileDialogPath>>>
    openManyFiles(const Application::OpenFileDialogRequest&) override
    {
        return Domain::Result<Application::FileDialogResponse<
            std::vector<Application::FileDialogPath>>>::success({});
    }

    Domain::Result<Application::FileDialogResponse<
        Application::SavedFileDialogSelection>>
    saveFile(const Application::SaveFileDialogRequest& request) override
    {
        ++saveCalls;
        observedRequest = request;
        if (events)
        {
            events->push_back("dialog");
        }
        Application::FileDialogResponse<
            Application::SavedFileDialogSelection> response;
        if (!selectedPath.isNull())
        {
            response.selection = Application::SavedFileDialogSelection{
                .path = Application::FileDialogPath(
                    selectedPath.toUtf8().toStdString()
                    )
            };
        }
        return Domain::Result<decltype(response)>::success(std::move(response));
    }

    Domain::Result<
        Application::FileDialogResponse<Application::FileDialogPath>>
    selectDirectory(const Application::SelectDirectoryDialogRequest&) override
    {
        return Domain::Result<
            Application::FileDialogResponse<Application::FileDialogPath>>::success({});
    }

    QString selectedPath;
    int saveCalls = 0;
    Application::SaveFileDialogRequest observedRequest;
    std::vector<std::string>* events = nullptr;
};

class RecordingUserPrompt final : public Application::UserPromptPort
{
public:
    Domain::Result<void> showMessage(
        const Application::MessagePromptRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<void>::success();
    }

    Domain::Result<Application::ConfirmationResponse> confirm(
        const Application::ConfirmationPromptRequest& request
        ) override
    {
        ++confirmCalls;
        observedConfirmation = request;
        if (events)
        {
            events->push_back("confirm");
        }
        return Domain::Result<Application::ConfirmationResponse>::success(
            confirmation
            );
    }

    Domain::Result<Application::UnsavedChangesAction> confirmUnsavedChanges(
        const Application::UnsavedChangesPromptRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<Application::UnsavedChangesAction>::success(
            Application::UnsavedChangesAction::Cancel
            );
    }

    Domain::Result<std::optional<Application::PromptActionId>> chooseAction(
        const Application::NamedActionPromptRequest& request
        ) override
    {
        static_cast<void>(request);
        return Domain::Result<std::optional<Application::PromptActionId>>::success(
            std::nullopt
            );
    }

    Application::ConfirmationResponse confirmation =
        Application::ConfirmationResponse::Accepted;
    Application::ConfirmationPromptRequest observedConfirmation;
    int confirmCalls = 0;
    std::vector<std::string>* events = nullptr;
};

class RecordingRecentHistory final
    : public Application::RecentWorkspaceHistoryPort
{
public:
    Application::RecentWorkspaceHistory load() const override
    {
        ++loadCalls;
        return history;
    }

    Domain::Result<void> save(
        const Application::RecentWorkspaceHistory& value
        ) const override
    {
        ++saveCalls;
        lastSaved = value;
        if (failSave)
        {
            return Domain::Result<void>::failure(testError("history write failed"));
        }
        history = value;
        return Domain::Result<void>::success();
    }

    Domain::Result<void> clear() const override
    {
        history = {};
        return Domain::Result<void>::success();
    }

    mutable Application::RecentWorkspaceHistory history;
    mutable Application::RecentWorkspaceHistory lastSaved;
    mutable int loadCalls = 0;
    mutable int saveCalls = 0;
    bool failSave = false;
};

class RecordingNotifications final : public Application::NotificationPort
{
public:
    Domain::Result<void> show(
        const Application::NotificationRequest& request
        ) override
    {
        notifications.push_back(request);
        return Domain::Result<void>::success();
    }

    Domain::Result<void> clear() override
    {
        notifications.clear();
        return Domain::Result<void>::success();
    }

    std::vector<Application::NotificationRequest> notifications;
};

class FailingRemoveWorkspaceFileOperations final
    : public Platform::WorkspaceCreationFileOperationsPort
{
public:
    bool profileExists(
        const Application::WorkspaceLocation& location
        ) const override
    {
        return delegate.profileExists(location);
    }

    Application::WorkspaceLocation recoveryLocationFor(
        const Application::WorkspaceLocation& profile
        ) override
    {
        Application::WorkspaceLocation recovery =
            delegate.recoveryLocationFor(profile);
        recoveryPaths.push_back(recovery);
        return recovery;
    }

    bool moveProfile(
        const Application::WorkspaceLocation& source,
        const Application::WorkspaceLocation& destination
        ) override
    {
        return delegate.moveProfile(source, destination);
    }

    bool removeProfile(
        const Application::WorkspaceLocation& location
        ) override
    {
        ++removeCalls;
        if (failNextRemove)
        {
            failNextRemove = false;
            failedRemovalPath = location;
            return false;
        }
        return delegate.removeProfile(location);
    }

    Platform::QtWorkspaceCreationFileOperationsPort delegate;
    bool failNextRemove = true;
    int removeCalls = 0;
    Application::WorkspaceLocation failedRemovalPath;
    std::vector<Application::WorkspaceLocation> recoveryPaths;
};

class FailingPartialWorkspaceCloseLifecyclePort final
    : public Application::WorkspaceCreationLifecyclePort
{
public:
    explicit FailingPartialWorkspaceCloseLifecyclePort(
        Platform::WorkspaceStoreCreationLifecycleAdapter& delegate
        ) noexcept
        : delegate(delegate)
    {
    }

    bool profileExists(
        const Application::WorkspaceLocation& location
        ) const override
    {
        return delegate.profileExists(location);
    }

    Application::WorkspaceLocation recoveryLocationFor(
        const Application::WorkspaceLocation& profile
        ) override
    {
        return delegate.recoveryLocationFor(profile);
    }

    bool moveProfile(
        const Application::WorkspaceLocation& source,
        const Application::WorkspaceLocation& destination
        ) override
    {
        return delegate.moveProfile(source, destination);
    }

    bool removeProfile(
        const Application::WorkspaceLocation& location
        ) override
    {
        return delegate.removeProfile(location);
    }

    Domain::Result<void> closeActiveWorkspace() override
    {
        ++closeCalls;
        if (closeCalls == 2)
        {
            return Domain::Result<void>::failure(
                testError("injected failed replacement workspace close")
                );
        }
        return delegate.closeActiveWorkspace();
    }

    Domain::Result<Application::WorkspaceSession> createWorkspace(
        const Application::CreateWorkspaceRequest& request
        ) override
    {
        return delegate.createWorkspace(request);
    }

    Platform::WorkspaceStoreCreationLifecycleAdapter& delegate;
    int closeCalls = 0;
};

class InjectingWorkspaceGateway final : public Application::WorkspaceGateway
{
public:
    Domain::Result<Application::WorkspaceSession> createWorkspace(
        const Application::CreateWorkspaceRequest& request
        ) override
    {
        ++createCalls;
        if (failNextCreate)
        {
            failNextCreate = false;
            if (writePartialOnCreateFailure)
            {
                if (!writeBytes(pathFor(request.location), "partial profile"))
                {
                    return Domain::Result<Application::WorkspaceSession>::failure(
                        testError("test could not write the partial profile")
                        );
                }
            }
            return Domain::Result<Application::WorkspaceSession>::failure(
                testError("injected profile creation failure")
                );
        }
        return delegate.createWorkspace(request);
    }

    Domain::Result<Application::WorkspaceSession> openWorkspace(
        const Application::OpenWorkspaceRequest& request
        ) override
    {
        return delegate.openWorkspace(request);
    }

    Domain::Result<void> closeWorkspace(
        const Application::CloseWorkspaceRequest& request
        ) override
    {
        ++closeCalls;
        if (failNextClose)
        {
            failNextClose = false;
            return Domain::Result<void>::failure(testError("injected close failure"));
        }
        return delegate.closeWorkspace(request);
    }

    Domain::Result<void> saveWorkspace(
        const Application::SaveWorkspaceRequest& request
        ) override
    {
        return delegate.saveWorkspace(request);
    }

    Domain::Result<Application::WorkspaceLocation> saveWorkspaceAs(
        const Application::SaveWorkspaceAsRequest& request
        ) override
    {
        return delegate.saveWorkspaceAs(request);
    }

    Domain::Result<Application::WorkspaceLocation> exportWorkspace(
        const Application::ExportWorkspaceRequest& request
        ) override
    {
        return delegate.exportWorkspace(request);
    }

    ClassMngr::Next::Platform::WorkspaceStoreGateway delegate;
    bool failNextCreate = false;
    bool writePartialOnCreateFailure = false;
    bool failNextClose = false;
    int createCalls = 0;
    int closeCalls = 0;
};

class FlowFixture final
{
public:
    FlowFixture(
        QString selectedPath,
        bool setupIsPending = false,
        Platform::WorkspaceCreationFileOperationsPort* customFileOperations = nullptr
        )
        : fileDialog(std::move(selectedPath)),
          runtime(gateway),
          defaultFileOperations(),
          lifecyclePort(
              runtime.workspaceCoordinator(),
              runtime.workspaceState(),
              customFileOperations != nullptr
                  ? *customFileOperations
                  : defaultFileOperations
              ),
          lifecycle(lifecyclePort),
          flow(
              fileDialog,
              prompt,
              pathPolicy,
              lifecyclePort,
              lifecycle,
              runtime.workspaceCoordinator(),
              runtime.workspaceState(),
              recentHistory,
              notifications,
              [setupIsPending] { return setupIsPending; }
              )
    {
    }

    Domain::Result<Application::WorkspaceSession> openActive(
        const QString& path
        )
    {
        return runtime.workspaceCoordinator().createWorkspace(
            Application::CreateWorkspaceRequest{location(path)}
            );
    }

    RecordingFileDialog fileDialog;
    RecordingUserPrompt prompt;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    RecordingRecentHistory recentHistory;
    RecordingNotifications notifications;
    InjectingWorkspaceGateway gateway;
    Application::ApplicationRuntime runtime;
    Platform::QtWorkspaceCreationFileOperationsPort defaultFileOperations;
    Platform::WorkspaceStoreCreationLifecycleAdapter lifecyclePort;
    Application::WorkspaceCreationLifecycle lifecycle;
    Application::NormalWorkspaceCreationFlow flow;
};

std::optional<int> inspectSchemaAndTeacherCount(const QString& path)
{
    WorkspaceStore store;
    if (!store.openExisting(path))
    {
        return std::nullopt;
    }
    QSqlDatabase database = store.database();
    const auto version = DatabaseSchemaManager::schemaVersion(database);
    if (!version)
    {
        return std::nullopt;
    }
    int count = -1;
    {
        QSqlQuery query(database);
        if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM teachers"))
            || !query.next())
        {
            return std::nullopt;
        }
        count = query.value(0).toInt();
    }
    database = QSqlDatabase();
    store.close();
    return *version * 1000 + count;
}

bool hasNormalCreateRecoveryFile(const QString& directory)
{
    return !QDir(directory).entryList(
        {QStringLiteral(".*.workspace-create-*.recovery")},
        QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot
        ).isEmpty();
}

} // namespace

class NextWorkspaceCreationFlowTests final : public QObject
{
    Q_OBJECT

private slots:
    void newProfileDialogContractAndEmptyWorkspaceCreation();
    void existingInactiveDestinationIsReplaced();
    void samePathReplacementReplacesActiveWorkspaceAndRecordsHistory();
    void recoveryCleanupFailureKeepsSuccessfulCreationAndWarns();
    void failedPartialWorkspaceCloseRetainsOriginalRecoveryAndCanReopenPrior();
    void failedReplacementRestoresTargetAndReopensPreviousProfile();
    void samePathReplacementReopensCorrectProfileAfterFailure();
    void activeWorkspaceCloseFailureLeavesFilesUntouched();
    void recentHistoryFailureDoesNotUndoCreation();
    void promptAndDialogCancellationLeaveWorkspaceUntouched();
    void pendingSetupBlocksNormalCreation();
    void profilePathSuffixRulesAreCaseInsensitive();
};

void NextWorkspaceCreationFlowTests::newProfileDialogContractAndEmptyWorkspaceCreation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = directory.filePath(QStringLiteral("Current.tps"));
    const QString selection = directory.filePath(
        QStringLiteral("nested/New Profile.txt")
        );
    FlowFixture fixture(selection);
    QVERIFY(fixture.openActive(source));

    std::vector<std::string> order;
    fixture.prompt.events = &order;
    fixture.fileDialog.events = &order;
    const auto outcome = fixture.flow.run();

    QCOMPARE(outcome.status, Application::NormalWorkspaceCreationStatus::Created);
    QCOMPARE(order, (std::vector<std::string>{"confirm", "dialog"}));
    QCOMPARE(fixture.prompt.confirmCalls, 1);
    QCOMPARE(fixture.fileDialog.saveCalls, 1);
    QCOMPARE(fixture.prompt.observedConfirmation.title,
             std::string("New Teacher Profile"));
    QVERIFY(fixture.prompt.observedConfirmation.message.find("Leave the current page")
            != std::string::npos);
    QCOMPARE(fixture.fileDialog.observedRequest.purpose,
             Application::FileDialogPurpose::TeacherProfile);
    QCOMPARE(fixture.fileDialog.observedRequest.title,
             std::string("New Teacher Profile"));
    QCOMPARE(fixture.fileDialog.observedRequest.nameFilters,
             (std::vector<std::string>{"ClassMngr Teacher Profile (*.tps)"}));
    QCOMPARE(fixture.fileDialog.observedRequest.defaultSuffix, std::string("tps"));
    QVERIFY(fixture.fileDialog.observedRequest.confirmOverwrite);

    const QString expected = directory.filePath(
        QStringLiteral("nested/New Profile.txt.tps")
        );
    QCOMPARE(pathFor(outcome.profile), QFileInfo(expected).absoluteFilePath());
    QCOMPARE(fixture.runtime.workspaceState().snapshot().session()->location(),
             outcome.profile);
    QVERIFY(QFileInfo(pathFor(outcome.profile)).isFile());
    const auto versionAndTeacherCount = inspectSchemaAndTeacherCount(
        pathFor(outcome.profile)
        );
    QVERIFY(versionAndTeacherCount.has_value());
    QCOMPARE(*versionAndTeacherCount, DatabaseSchemaManager::LatestSchemaVersion * 1000);
    QCOMPARE(fixture.recentHistory.saveCalls, 1);
    QCOMPARE(fixture.recentHistory.history.paths.size(), std::size_t(1));
    QCOMPARE(fixture.recentHistory.history.paths.front(), outcome.profile);
    QCOMPARE(fixture.runtime.workspaceState().snapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Open);
    QVERIFY(!hasNormalCreateRecoveryFile(directory.path()));
}

void NextWorkspaceCreationFlowTests::existingInactiveDestinationIsReplaced()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString current = directory.filePath(QStringLiteral("Active.tps"));
    const QString target = directory.filePath(QStringLiteral("Existing.TPS"));
    QVERIFY(writeBytes(target, "old inactive profile bytes"));

    FlowFixture fixture(target);
    QVERIFY(fixture.openActive(current));
    const auto created = fixture.flow.run();

    QCOMPARE(created.status, Application::NormalWorkspaceCreationStatus::Created);
    QVERIFY(fixture.pathPolicy.sameLocation(
        fixture.runtime.workspaceState().snapshot().session()->location(),
        location(target)
        ));
    QVERIFY(QFileInfo::exists(current));
    const auto newProfile = inspectSchemaAndTeacherCount(target);
    QVERIFY(newProfile.has_value());
    QCOMPARE(*newProfile, DatabaseSchemaManager::LatestSchemaVersion * 1000);
    QVERIFY(!hasNormalCreateRecoveryFile(directory.path()));
    QCOMPARE(fixture.recentHistory.history.paths.size(), std::size_t(1));
}

void NextWorkspaceCreationFlowTests::
samePathReplacementReplacesActiveWorkspaceAndRecordsHistory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString activePath = directory.filePath(QStringLiteral("Active.tps"));

    WorkspaceStore seed;
    QVERIFY(seed.open(activePath));
    {
        QSqlQuery query(seed.database());
        QVERIFY(query.prepare(
            QStringLiteral("INSERT INTO teachers (teacher_en) VALUES (?)")
            ));
        query.addBindValue(QStringLiteral("Profile that must be replaced"));
        QVERIFY(query.exec());
    }
    seed.close();
    const auto oldProfile = inspectSchemaAndTeacherCount(activePath);
    QVERIFY(oldProfile.has_value());
    QCOMPARE(*oldProfile, DatabaseSchemaManager::LatestSchemaVersion * 1000 + 1);

    FlowFixture fixture(activePath);
    QVERIFY(fixture.openActive(activePath));
    const auto created = fixture.flow.run();

    QCOMPARE(created.status, Application::NormalWorkspaceCreationStatus::Created);
    QVERIFY(fixture.pathPolicy.sameLocation(
        fixture.runtime.workspaceState().snapshot().session()->location(),
        location(activePath)
        ));
    QCOMPARE(fixture.runtime.workspaceState().snapshot().lifecycle(),
             Application::WorkspaceLifecycleState::Open);
    const auto replacedProfile = inspectSchemaAndTeacherCount(activePath);
    QVERIFY(replacedProfile.has_value());
    QCOMPARE(*replacedProfile, DatabaseSchemaManager::LatestSchemaVersion * 1000);
    QVERIFY(!hasNormalCreateRecoveryFile(directory.path()));
    QCOMPARE(fixture.recentHistory.loadCalls, 1);
    QCOMPARE(fixture.recentHistory.saveCalls, 1);
    QCOMPARE(fixture.recentHistory.history.paths.size(), std::size_t(1));
    QCOMPARE(fixture.recentHistory.history.paths.front(), created.profile);
}

void NextWorkspaceCreationFlowTests::
recoveryCleanupFailureKeepsSuccessfulCreationAndWarns()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString activePath = directory.filePath(QStringLiteral("Active.tps"));
    const QString target = directory.filePath(QStringLiteral("Existing.tps"));
    const QByteArray originalBytes("old profile bytes\0with tail", 26);
    QVERIFY(writeBytes(target, originalBytes));

    FailingRemoveWorkspaceFileOperations fileOperations;
    FlowFixture fixture(target, false, &fileOperations);
    QVERIFY(fixture.openActive(activePath));
    const auto created = fixture.flow.run();

    QCOMPARE(created.status, Application::NormalWorkspaceCreationStatus::Created);
    QVERIFY(fixture.pathPolicy.sameLocation(
        fixture.runtime.workspaceState().snapshot().session()->location(),
        location(target)
        ));
    const auto newProfile = inspectSchemaAndTeacherCount(target);
    QVERIFY(newProfile.has_value());
    QCOMPARE(*newProfile, DatabaseSchemaManager::LatestSchemaVersion * 1000);

    QCOMPARE(fileOperations.removeCalls, 1);
    QCOMPARE(fileOperations.recoveryPaths.size(), std::size_t(1));
    const QString retainedRecoveryPath = pathFor(fileOperations.failedRemovalPath);
    QCOMPARE(retainedRecoveryPath, pathFor(fileOperations.recoveryPaths.front()));
    QVERIFY(QFileInfo::exists(retainedRecoveryPath));
    QCOMPARE(readBytes(retainedRecoveryPath), originalBytes);
    QVERIFY(created.message.find(retainedRecoveryPath.toStdString())
            != std::string::npos);

    QCOMPARE(fixture.recentHistory.loadCalls, 1);
    QCOMPARE(fixture.recentHistory.saveCalls, 1);
    QCOMPARE(fixture.recentHistory.history.paths.size(), std::size_t(1));
    QCOMPARE(fixture.recentHistory.history.paths.front(), created.profile);
    QVERIFY(!fixture.notifications.notifications.empty());
    QCOMPARE(fixture.notifications.notifications.back().severity,
             Application::NotificationSeverity::Warning);
    QVERIFY(fixture.notifications.notifications.back().message.find(
                retainedRecoveryPath.toStdString()
                ) != std::string::npos);
}

void NextWorkspaceCreationFlowTests::
failedPartialWorkspaceCloseRetainsOriginalRecoveryAndCanReopenPrior()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString previous = directory.filePath(QStringLiteral("Previous.tps"));
    const QString target = directory.filePath(QStringLiteral("Target.tps"));
    const QByteArray originalBytes("original bytes\0kept in recovery", 31);
    QVERIFY(writeBytes(target, originalBytes));

    RecordingFileDialog fileDialog(target);
    RecordingUserPrompt prompt;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    RecordingRecentHistory recentHistory;
    RecordingNotifications notifications;
    InjectingWorkspaceGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    FailingRemoveWorkspaceFileOperations fileOperations;
    fileOperations.failNextRemove = false;
    Platform::WorkspaceStoreCreationLifecycleAdapter adapter(
        runtime.workspaceCoordinator(),
        runtime.workspaceState(),
        fileOperations
        );
    FailingPartialWorkspaceCloseLifecyclePort lifecyclePort(adapter);
    Application::WorkspaceCreationLifecycle lifecycle(lifecyclePort);
    Application::NormalWorkspaceCreationFlow flow(
        fileDialog,
        prompt,
        pathPolicy,
        lifecyclePort,
        lifecycle,
        runtime.workspaceCoordinator(),
        runtime.workspaceState(),
        recentHistory,
        notifications,
        [] { return false; }
        );

    QVERIFY(runtime.workspaceCoordinator().createWorkspace(
        Application::CreateWorkspaceRequest{location(previous)}
        ));
    gateway.failNextCreate = true;
    gateway.writePartialOnCreateFailure = true;
    const auto failed = flow.run();

    QCOMPARE(failed.status, Application::NormalWorkspaceCreationStatus::Failed);
    QCOMPARE(lifecyclePort.closeCalls, 2);
    QVERIFY(!runtime.workspaceState().snapshot().session().has_value());
    QCOMPARE(readBytes(target), QByteArray("partial profile"));
    QCOMPARE(fileOperations.recoveryPaths.size(), std::size_t(1));
    const QString retainedRecoveryPath = pathFor(fileOperations.recoveryPaths.front());
    QVERIFY(QFileInfo::exists(retainedRecoveryPath));
    QCOMPARE(readBytes(retainedRecoveryPath), originalBytes);
    QCOMPARE(recentHistory.saveCalls, 0);
    QVERIFY(!notifications.notifications.empty());
    QCOMPARE(notifications.notifications.back().severity,
             Application::NotificationSeverity::Error);
    QVERIFY(notifications.notifications.back().message.find(
                "failed replacement workspace could not be closed"
                ) != std::string::npos);
    QVERIFY(notifications.notifications.back().message.find(
                retainedRecoveryPath.toStdString()
                ) != std::string::npos);

    const auto reopened = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(previous)}
        );
    QVERIFY(reopened);
    QCOMPARE(runtime.workspaceState().snapshot().session()->location(),
             location(previous));
    const auto priorProfile = inspectSchemaAndTeacherCount(previous);
    QVERIFY(priorProfile.has_value());
    QCOMPARE(*priorProfile, DatabaseSchemaManager::LatestSchemaVersion * 1000);
}

void NextWorkspaceCreationFlowTests::failedReplacementRestoresTargetAndReopensPreviousProfile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString previous = directory.filePath(QStringLiteral("Previous.tps"));
    const QString target = directory.filePath(QStringLiteral("Target.tps"));
    const QByteArray originalBytes("original target bytes\0with tail", 31);
    QVERIFY(writeBytes(target, originalBytes));

    FlowFixture fixture(target);
    QVERIFY(fixture.openActive(previous));
    fixture.gateway.failNextCreate = true;
    fixture.gateway.writePartialOnCreateFailure = true;
    const auto failed = fixture.flow.run();

    QCOMPARE(failed.status, Application::NormalWorkspaceCreationStatus::Failed);
    QCOMPARE(readBytes(target), originalBytes);
    QCOMPARE(fixture.runtime.workspaceState().snapshot().session()->location(),
             location(previous));
    QVERIFY(!hasNormalCreateRecoveryFile(directory.path()));
    QCOMPARE(fixture.recentHistory.saveCalls, 0);
    QVERIFY(!fixture.notifications.notifications.empty());
    QCOMPARE(fixture.notifications.notifications.back().severity,
             Application::NotificationSeverity::Error);
}

void NextWorkspaceCreationFlowTests::samePathReplacementReopensCorrectProfileAfterFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString activePath = directory.filePath(QStringLiteral("Same.TPS"));
    FlowFixture fixture(activePath);
    QVERIFY(fixture.openActive(activePath));
    fixture.gateway.failNextCreate = true;
    fixture.gateway.writePartialOnCreateFailure = true;
    const auto failed = fixture.flow.run();

    QCOMPARE(failed.status, Application::NormalWorkspaceCreationStatus::Failed);
    QCOMPARE(fixture.runtime.workspaceState().snapshot().session()->location(),
             location(activePath));
    QCOMPARE(*inspectSchemaAndTeacherCount(activePath),
             DatabaseSchemaManager::LatestSchemaVersion * 1000);
    QCOMPARE(fixture.recentHistory.saveCalls, 0);
}

void NextWorkspaceCreationFlowTests::activeWorkspaceCloseFailureLeavesFilesUntouched()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString activePath = directory.filePath(QStringLiteral("Current.tps"));
    const QString target = directory.filePath(QStringLiteral("Target.tps"));
    const QByteArray targetBytes("keep these target bytes", 23);
    QVERIFY(writeBytes(target, targetBytes));

    FlowFixture fixture(target);
    QVERIFY(fixture.openActive(activePath));
    fixture.gateway.failNextClose = true;
    const auto failed = fixture.flow.run();

    QCOMPARE(failed.status, Application::NormalWorkspaceCreationStatus::Failed);
    QCOMPARE(readBytes(target), targetBytes);
    QCOMPARE(fixture.runtime.workspaceState().snapshot().session()->location(),
             location(activePath));
    QCOMPARE(fixture.gateway.createCalls, 1); // Only the initial open.
    QCOMPARE(fixture.recentHistory.saveCalls, 0);
}

void NextWorkspaceCreationFlowTests::recentHistoryFailureDoesNotUndoCreation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString target = directory.filePath(QStringLiteral("Created.tps"));
    FlowFixture fixture(target);
    fixture.recentHistory.failSave = true;
    const auto created = fixture.flow.run();

    QCOMPARE(created.status, Application::NormalWorkspaceCreationStatus::Created);
    QVERIFY(fixture.pathPolicy.sameLocation(
        fixture.runtime.workspaceState().snapshot().session()->location(),
        location(target)
        ));
    QCOMPARE(fixture.recentHistory.saveCalls, 1);
    QVERIFY(!fixture.notifications.notifications.empty());
    QCOMPARE(fixture.notifications.notifications.back().severity,
             Application::NotificationSeverity::Warning);
    QVERIFY(fixture.notifications.notifications.back().message.find(
                "recent profile history could not be saved")
            != std::string::npos);
}

void NextWorkspaceCreationFlowTests::promptAndDialogCancellationLeaveWorkspaceUntouched()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString activePath = directory.filePath(QStringLiteral("Current.tps"));
    const QString target = directory.filePath(QStringLiteral("Target.tps"));
    QVERIFY(writeBytes(target, "existing target"));
    FlowFixture fixture(target);
    QVERIFY(fixture.openActive(activePath));

    fixture.prompt.confirmation = Application::ConfirmationResponse::Declined;
    const auto promptCanceled = fixture.flow.run();
    QCOMPARE(promptCanceled.status, Application::NormalWorkspaceCreationStatus::Canceled);
    QCOMPARE(fixture.fileDialog.saveCalls, 0);
    QCOMPARE(fixture.runtime.workspaceState().snapshot().session()->location(),
             location(activePath));
    QCOMPARE(readBytes(target), QByteArray("existing target"));

    fixture.prompt.confirmation = Application::ConfirmationResponse::Accepted;
    fixture.fileDialog.selectedPath = QString();
    const auto dialogCanceled = fixture.flow.run();
    QCOMPARE(dialogCanceled.status, Application::NormalWorkspaceCreationStatus::Canceled);
    QCOMPARE(fixture.runtime.workspaceState().snapshot().session()->location(),
             location(activePath));
    QCOMPARE(readBytes(target), QByteArray("existing target"));
    QCOMPARE(fixture.recentHistory.saveCalls, 0);
}

void NextWorkspaceCreationFlowTests::pendingSetupBlocksNormalCreation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString target = directory.filePath(QStringLiteral("Target.tps"));
    QVERIFY(writeBytes(target, "unchanged"));
    FlowFixture fixture(target, true);
    const auto blocked = fixture.flow.run();

    QCOMPARE(blocked.status, Application::NormalWorkspaceCreationStatus::Blocked);
    QCOMPARE(fixture.prompt.confirmCalls, 0);
    QCOMPARE(fixture.fileDialog.saveCalls, 0);
    QCOMPARE(readBytes(target), QByteArray("unchanged"));
}

void NextWorkspaceCreationFlowTests::profilePathSuffixRulesAreCaseInsensitive()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Platform::WorkspaceProfilePathPolicy policy;

    const auto tps = policy.normalizeNewProfilePath(Application::FileDialogPath(
        directory.filePath(QStringLiteral("Case.TPS")).toUtf8().toStdString()
        ));
    QVERIFY(tps);
    QVERIFY(QFileInfo(pathFor(tps.value())).isAbsolute());
    QCOMPARE(QFileInfo(pathFor(tps.value())).fileName(), QStringLiteral("Case.TPS"));

    const auto legacy = policy.normalizeNewProfilePath(Application::FileDialogPath(
        directory.filePath(QStringLiteral("Legacy.DB")).toUtf8().toStdString()
        ));
    QVERIFY(legacy);
    QVERIFY(QFileInfo(pathFor(legacy.value())).isAbsolute());
    QCOMPARE(QFileInfo(pathFor(legacy.value())).fileName(), QStringLiteral("Legacy.tps"));

    const auto otherSuffix = policy.normalizeNewProfilePath(Application::FileDialogPath(
        directory.filePath(QStringLiteral("Teacher.data")).toUtf8().toStdString()
        ));
    QVERIFY(otherSuffix);
    QVERIFY(QFileInfo(pathFor(otherSuffix.value())).isAbsolute());
    QCOMPARE(QFileInfo(pathFor(otherSuffix.value())).fileName(),
             QStringLiteral("Teacher.data.tps"));

    const auto trailingDot = policy.normalizeNewProfilePath(
        Application::FileDialogPath(
            directory.filePath(QStringLiteral("Teacher.")).toUtf8().toStdString()
            )
        );
    QVERIFY(trailingDot);
    QCOMPARE(QFileInfo(pathFor(trailingDot.value())).fileName(),
             QStringLiteral("Teacher..tps"));
}

QTEST_MAIN(NextWorkspaceCreationFlowTests)
#include "next_workspace_creation_flow_tests.moc"
