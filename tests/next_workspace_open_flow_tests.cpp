#include "data/database/workspace_store.h"
#include "next/application/application_runtime.h"
#include "next/application/open_existing_workspace_flow.h"
#include "next/platform/workspace_profile_path_policy.h"
#include "next/platform/workspace_store_gateway.h"

#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Application::WorkspaceLocation location(const QString& path)
{
    return Application::WorkspaceLocation(
        QFileInfo(path).absoluteFilePath().toUtf8().toStdString()
        );
}

bool createProfile(const QString& path)
{
    Platform::WorkspaceStoreGateway gateway;
    const auto created = gateway.createWorkspace(
        Application::CreateWorkspaceRequest{location(path)}
        );
    if (!created)
    {
        return false;
    }
    return gateway.closeWorkspace(
        Application::CloseWorkspaceRequest{created.value()}
        ).hasValue();
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

bool writeUnsupportedSchema(const QString& path, const int version = 1)
{
    const QString connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool succeeded = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            succeeded = query.exec(
                QStringLiteral("PRAGMA user_version = %1").arg(version)
                );
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return succeeded;
}

class RecordingFileDialog final : public Application::FileDialogPort
{
public:
    Application::FileDialogResponse<Application::FileDialogPath> response;
    std::optional<Application::OpenFileDialogRequest> observedRequest;
    int openCalls = 0;

    Domain::Result<Application::FileDialogResponse<Application::FileDialogPath>>
    openOneFile(const Application::OpenFileDialogRequest& request) override
    {
        ++openCalls;
        observedRequest = request;
        return Domain::Result<
            Application::FileDialogResponse<Application::FileDialogPath>>::success(
                response
                );
    }

    Domain::Result<Application::FileDialogResponse<
        std::vector<Application::FileDialogPath>>>
    openManyFiles(const Application::OpenFileDialogRequest&) override
    {
        return Domain::Result<Application::FileDialogResponse<
            std::vector<Application::FileDialogPath>>>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Unexpected multi-file dialog request.",
                .recoverable = false
            });
    }

    Domain::Result<Application::FileDialogResponse<
        Application::SavedFileDialogSelection>>
    saveFile(const Application::SaveFileDialogRequest&) override
    {
        return Domain::Result<Application::FileDialogResponse<
            Application::SavedFileDialogSelection>>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Unexpected save dialog request.",
                .recoverable = false
            });
    }

    Domain::Result<Application::FileDialogResponse<Application::FileDialogPath>>
    selectDirectory(const Application::SelectDirectoryDialogRequest&) override
    {
        return Domain::Result<
            Application::FileDialogResponse<Application::FileDialogPath>>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Unexpected directory dialog request.",
                .recoverable = false
            });
    }
};

class RecordingHistory final : public Application::RecentWorkspaceHistoryPort
{
public:
    Application::RecentWorkspaceHistory history;
    std::vector<Application::RecentWorkspaceHistory> saved;
    bool failSave = false;
    int loadCalls = 0;

    Application::RecentWorkspaceHistory load() const override
    {
        ++const_cast<RecordingHistory*>(this)->loadCalls;
        return history;
    }

    Domain::Result<void> save(
        const Application::RecentWorkspaceHistory& value
        ) const override
    {
        auto* const self = const_cast<RecordingHistory*>(this);
        self->saved.push_back(value);
        if (failSave)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected history save failure",
                .recoverable = true
            });
        }
        self->history = value;
        return Domain::Result<void>::success();
    }

    Domain::Result<void> clear() const override
    {
        return Domain::Result<void>::success();
    }
};

class RecordingNotifications final : public Application::NotificationPort
{
public:
    std::vector<Application::NotificationRequest> requests;

    Domain::Result<void> show(
        const Application::NotificationRequest& request
        ) override
    {
        requests.push_back(request);
        return Domain::Result<void>::success();
    }

    Domain::Result<void> clear() override
    {
        return Domain::Result<void>::success();
    }
};

QString pathFor(const Application::WorkspaceLocation& value)
{
    return QString::fromUtf8(
        value.value().data(),
        static_cast<qsizetype>(value.value().size())
        );
}

} // namespace

class NextWorkspaceOpenFlowTests final : public QObject
{
    Q_OBJECT

private slots:
    void opensTpsUpdatesHistoryAfterSuccessAndRefreshesStore();
    void opensSchemaV6DbInPlaceAndSaveUsesActiveDbPath();
    void opensSelectedRecentPathWithoutUsingTheFileDialog();
    void cancelAndPendingSetupLeaveDialogHistoryAndWorkspaceUntouched();
    void corruptAndMissingProfilesPreserveActiveSessionAndSelection();
    void dirtyWorkspaceCannotBeReplaced();
    void workspaceStoreRollbackDropsPendingTransaction();
    void historyFailureDoesNotRollBackOpenedProfile();
    void openPathPolicyAcceptsTpsAndDbAndRejectsUnknownSuffix();
};

void NextWorkspaceOpenFlowTests::opensTpsUpdatesHistoryAfterSuccessAndRefreshesStore()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString currentPath = directory.filePath(QStringLiteral("current.tps"));
    const QString targetPath = directory.filePath(QStringLiteral("teacher.TPS"));
    QVERIFY(createProfile(currentPath));
    QVERIFY(createProfile(targetPath));

    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    const auto current = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(currentPath)}
        );
    QVERIFY(current);
    const auto selectedClass = Domain::ClassId::fromString("prior-selection");
    QVERIFY(selectedClass.has_value());
    runtime.selectionState().setSelection(*selectedClass);

    RecordingFileDialog fileDialog;
    fileDialog.response.selection = Application::FileDialogPath(
        targetPath.toUtf8().toStdString()
        );
    RecordingHistory history;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::OpenExistingWorkspaceFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        history,
        notifications,
        [] { return false; }
        );

    const auto opened = flow.run();

    QCOMPARE(opened.status, Application::OpenExistingWorkspaceStatus::Opened);
    QVERIFY(opened.session.has_value());
    QCOMPARE(pathFor(opened.session->location()), QFileInfo(targetPath).canonicalFilePath());
    QCOMPARE(runtime.workspaceState().snapshot().session(), opened.session);
    QVERIFY(!runtime.selectionState().snapshot().hasSelection());
    QCOMPARE(fileDialog.openCalls, 1);
    QVERIFY(fileDialog.observedRequest.has_value());
    QCOMPARE(QString::fromStdString(fileDialog.observedRequest->title),
             QStringLiteral("Open Teacher Profile"));
    QCOMPARE(fileDialog.observedRequest->nameFilters,
             std::vector<std::string>{"ClassMngr Teacher Profile (*.tps *.db)"});
    QCOMPARE(history.loadCalls, 1);
    QCOMPARE(history.saved.size(), std::size_t(1));
    QCOMPARE(history.saved.front().paths.front(), opened.session->location());
    QCOMPARE(history.saved.front().lastPath,
             std::optional<Application::RecentWorkspacePath>(opened.session->location()));
    QCOMPARE(notifications.requests.back().severity,
             Application::NotificationSeverity::Information);
}

void NextWorkspaceOpenFlowTests::opensSchemaV6DbInPlaceAndSaveUsesActiveDbPath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString currentPath = directory.filePath(QStringLiteral("current.tps"));
    const QString selectedPath = directory.filePath(QStringLiteral("legacy.DB"));
    const QString siblingTpsPath = directory.filePath(QStringLiteral("legacy.tps"));
    QVERIFY(createProfile(currentPath));
    QVERIFY(createProfile(selectedPath));

    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    const auto current = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(currentPath)}
        );
    QVERIFY(current);
    const QStringList connectionNamesBeforeDbOpen = QSqlDatabase::connectionNames();
    const auto selectedClass = Domain::ClassId::fromString("prior-selection");
    QVERIFY(selectedClass.has_value());
    runtime.selectionState().setSelection(*selectedClass);

    RecordingFileDialog fileDialog;
    fileDialog.response.selection = Application::FileDialogPath(
        selectedPath.toUtf8().toStdString()
        );
    RecordingHistory history;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::OpenExistingWorkspaceFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        history,
        notifications,
        [] { return false; }
        );

    const auto opened = flow.run();

    QCOMPARE(opened.status, Application::OpenExistingWorkspaceStatus::Opened);
    QVERIFY(opened.session.has_value());
    const QString canonicalPath = QFileInfo(selectedPath).canonicalFilePath();
    QCOMPARE(pathFor(opened.session->location()), canonicalPath);
    QCOMPARE(QFileInfo(canonicalPath).fileName(), QStringLiteral("legacy.DB"));
    QCOMPARE(runtime.workspaceState().snapshot().session(), opened.session);
    QVERIFY(!runtime.selectionState().snapshot().hasSelection());
    QCOMPARE(history.saved.size(), std::size_t(1));
    QCOMPARE(history.saved.front().paths.front(), opened.session->location());
    QCOMPARE(history.saved.front().lastPath,
             std::optional<Application::RecentWorkspacePath>(opened.session->location()));
    QVERIFY(QFileInfo::exists(selectedPath));
    QVERIFY(!QFileInfo::exists(siblingTpsPath));

    QString activeConnectionName;
    for (const QString& connectionName : QSqlDatabase::connectionNames())
    {
        if (!connectionNamesBeforeDbOpen.contains(connectionName))
        {
            activeConnectionName = connectionName;
            break;
        }
    }
    QVERIFY(!activeConnectionName.isEmpty());
    {
        QSqlDatabase activeDatabase = QSqlDatabase::database(activeConnectionName);
        QVERIFY(activeDatabase.isOpen());
        QVERIFY(activeDatabase.transaction());
        QVERIFY(runtime.workspaceState().markDirty());
        QVERIFY(runtime.workspaceCoordinator().saveWorkspace());
    }
    QCOMPARE(runtime.workspaceState().snapshot().session()->location(),
             opened.session->location());
    QCOMPARE(runtime.workspaceState().snapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Clean);
    QVERIFY(QFileInfo::exists(selectedPath));
    QVERIFY(!QFileInfo::exists(siblingTpsPath));
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
}

void NextWorkspaceOpenFlowTests::
cancelAndPendingSetupLeaveDialogHistoryAndWorkspaceUntouched()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString currentPath = directory.filePath(QStringLiteral("current.tps"));
    QVERIFY(createProfile(currentPath));
    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    const auto current = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(currentPath)}
        );
    QVERIFY(current);

    RecordingFileDialog fileDialog;
    RecordingHistory history;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    bool setupPending = false;
    Application::OpenExistingWorkspaceFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        history,
        notifications,
        [&setupPending] { return setupPending; }
        );

    const auto canceled = flow.run();
    QCOMPARE(canceled.status, Application::OpenExistingWorkspaceStatus::Canceled);
    QCOMPARE(runtime.workspaceState().snapshot().session(), current.value());
    QCOMPARE(fileDialog.openCalls, 1);
    QCOMPARE(history.loadCalls, 0);
    QVERIFY(history.saved.empty());

    setupPending = true;
    const auto blocked = flow.run();
    QCOMPARE(blocked.status, Application::OpenExistingWorkspaceStatus::Blocked);
    QCOMPARE(runtime.workspaceState().snapshot().session(), current.value());
    QCOMPARE(fileDialog.openCalls, 1);
    QCOMPARE(history.loadCalls, 0);
    QVERIFY(history.saved.empty());
    QCOMPARE(notifications.requests.back().severity,
             Application::NotificationSeverity::Warning);
}

void NextWorkspaceOpenFlowTests::
corruptAndMissingProfilesPreserveActiveSessionAndSelection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString currentPath = directory.filePath(QStringLiteral("current.tps"));
    const QString corruptPath = directory.filePath(QStringLiteral("corrupt.tps"));
    const QString unsupportedPath = directory.filePath(
        QStringLiteral("unsupported-schema.tps")
        );
    const QString versionFiveDbPath = directory.filePath(
        QStringLiteral("unsupported-v5.db")
        );
    const QString futureDbPath = directory.filePath(
        QStringLiteral("unsupported-v7.DB")
        );
    const QString versionFiveTpsPath = directory.filePath(
        QStringLiteral("unsupported-v5.tps")
        );
    const QString futureTpsPath = directory.filePath(
        QStringLiteral("unsupported-v7.tps")
        );
    const QString missingPath = directory.filePath(QStringLiteral("missing.tps"));
    const QString retryPath = directory.filePath(QStringLiteral("retry.tps"));
    QVERIFY(createProfile(currentPath));
    QVERIFY(writeBytes(corruptPath, QByteArrayLiteral("not a SQLite profile")));
    QVERIFY(writeUnsupportedSchema(unsupportedPath));
    QVERIFY(writeUnsupportedSchema(versionFiveDbPath, 5));
    QVERIFY(writeUnsupportedSchema(futureDbPath, 7));
    QVERIFY(createProfile(retryPath));
    const auto readBytes = [](const QString& path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    };
    const QByteArray versionFiveBytesBefore = readBytes(versionFiveDbPath);
    const QByteArray futureBytesBefore = readBytes(futureDbPath);
    QVERIFY(!versionFiveBytesBefore.isEmpty());
    QVERIFY(!futureBytesBefore.isEmpty());

    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    const auto current = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(currentPath)}
        );
    QVERIFY(current);
    const QByteArray activeProfileBytesBeforeSchemaFailures = readBytes(currentPath);
    const auto selectedClass = Domain::ClassId::fromString("preserved-selection");
    QVERIFY(selectedClass.has_value());
    runtime.selectionState().setSelection(*selectedClass);

    RecordingFileDialog fileDialog;
    RecordingHistory history;
    const Application::RecentWorkspacePath corruptHistoryPath(
        corruptPath.toUtf8().toStdString()
        );
    const Application::RecentWorkspacePath unsupportedHistoryPath(
        unsupportedPath.toUtf8().toStdString()
        );
    const Application::RecentWorkspacePath missingHistoryPath(
        missingPath.toUtf8().toStdString()
        );
    const Application::RecentWorkspacePath unrelatedHistoryPath(
        directory.filePath(QStringLiteral("unrelated.tps")).toUtf8().toStdString()
        );
    history.history.paths = {
        corruptHistoryPath,
        unsupportedHistoryPath,
        missingHistoryPath,
        unrelatedHistoryPath
    };
    history.history.lastPath = missingHistoryPath;
    const Application::RecentWorkspaceHistory historyBeforeSchemaFailures =
        history.history;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::OpenExistingWorkspaceFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        history,
        notifications,
        [] { return false; }
        );

    fileDialog.response.selection = Application::FileDialogPath(
        corruptPath.toUtf8().toStdString()
        );
    const auto corrupt = flow.run();
    QCOMPARE(corrupt.status, Application::OpenExistingWorkspaceStatus::Failed);
    QVERIFY(runtime.workspaceState().snapshot().session().has_value());
    QCOMPARE(runtime.workspaceState().snapshot().session().value(), current.value());
    QVERIFY(runtime.selectionState().snapshot().classId());
    QCOMPARE(runtime.selectionState().snapshot().classId()->value(),
             selectedClass->value());
    QCOMPARE(history.loadCalls, 0);
    QVERIFY(history.saved.empty());

    fileDialog.response.selection = Application::FileDialogPath(
        unsupportedPath.toUtf8().toStdString()
        );
    const auto unsupported = flow.run();
    QCOMPARE(unsupported.status, Application::OpenExistingWorkspaceStatus::Failed);
    QCOMPARE(runtime.workspaceState().snapshot().session().value(), current.value());
    QVERIFY(runtime.selectionState().snapshot().classId());
    QCOMPARE(runtime.selectionState().snapshot().classId()->value(),
             selectedClass->value());
    QCOMPARE(history.loadCalls, 0);
    QVERIFY(history.saved.empty());

    for (const QString& unsupportedDbPath : {versionFiveDbPath, futureDbPath})
    {
        fileDialog.response.selection = Application::FileDialogPath(
            unsupportedDbPath.toUtf8().toStdString()
            );
        const auto schemaRejected = flow.run();
        QCOMPARE(schemaRejected.status, Application::OpenExistingWorkspaceStatus::Failed);
        QVERIFY(schemaRejected.message.find("schema version") != std::string::npos);
        QCOMPARE(runtime.workspaceState().snapshot().session().value(), current.value());
        QVERIFY(runtime.selectionState().snapshot().classId());
        QCOMPARE(runtime.selectionState().snapshot().classId()->value(),
                 selectedClass->value());
        QCOMPARE(history.loadCalls, 0);
        QVERIFY(history.saved.empty());
        QVERIFY(QFileInfo::exists(unsupportedDbPath));
    }
    QCOMPARE(readBytes(versionFiveDbPath), versionFiveBytesBefore);
    QCOMPARE(readBytes(futureDbPath), futureBytesBefore);
    QVERIFY(!QFileInfo::exists(versionFiveTpsPath));
    QVERIFY(!QFileInfo::exists(futureTpsPath));
    QCOMPARE(history.history, historyBeforeSchemaFailures);
    QCOMPARE(readBytes(currentPath), activeProfileBytesBeforeSchemaFailures);

    fileDialog.response.selection = Application::FileDialogPath(
        missingPath.toUtf8().toStdString()
        );
    const auto missing = flow.run();
    QCOMPARE(missing.status, Application::OpenExistingWorkspaceStatus::Failed);
    QCOMPARE(runtime.workspaceState().snapshot().session().value(), current.value());
    QVERIFY(runtime.selectionState().snapshot().classId());
    QCOMPARE(runtime.selectionState().snapshot().classId()->value(),
             selectedClass->value());
    QCOMPARE(history.loadCalls, 1);
    QCOMPARE(history.saved.size(), std::size_t(1));
    QCOMPARE(
        history.history.paths,
        Application::RecentWorkspacePaths({
            corruptHistoryPath,
            unsupportedHistoryPath,
            unrelatedHistoryPath
        })
        );
    QVERIFY(!history.history.lastPath.has_value());
    QVERIFY(!QFileInfo::exists(missingPath));
    QVERIFY(notifications.requests.back().message.find("could not be found")
            != std::string::npos);

    fileDialog.response.selection = Application::FileDialogPath(
        retryPath.toUtf8().toStdString()
        );
    const auto retried = flow.run();
    QCOMPARE(retried.status, Application::OpenExistingWorkspaceStatus::Opened);
    QCOMPARE(runtime.workspaceState().snapshot().session()->location(),
             location(QFileInfo(retryPath).canonicalFilePath()));
    QVERIFY(!runtime.selectionState().snapshot().hasSelection());
    QCOMPARE(history.loadCalls, 2);
    QCOMPARE(history.saved.size(), std::size_t(2));
}

void NextWorkspaceOpenFlowTests::opensSelectedRecentPathWithoutUsingTheFileDialog()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString targetPath = directory.filePath(QStringLiteral("recent.DB"));
    QVERIFY(createProfile(targetPath));

    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    RecordingFileDialog fileDialog;
    RecordingHistory history;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::OpenExistingWorkspaceFlow flow(
        pathPolicy,
        runtime.workspaceCoordinator(),
        history,
        notifications,
        [] { return false; }
        );

    const Application::FileDialogPath selected(targetPath.toUtf8().toStdString());
    const auto opened = flow.openPath(selected);

    QCOMPARE(opened.status, Application::OpenExistingWorkspaceStatus::Opened);
    QVERIFY(opened.session.has_value());
    QCOMPARE(runtime.workspaceState().snapshot().session(), opened.session);
    QCOMPARE(QFileInfo(pathFor(opened.session->location())).fileName(),
             QStringLiteral("recent.DB"));
    QVERIFY(!QFileInfo::exists(directory.filePath(QStringLiteral("recent.tps"))));
    QCOMPARE(fileDialog.openCalls, 0);
    QCOMPARE(history.loadCalls, 1);
    QCOMPARE(history.saved.size(), std::size_t(1));
    QCOMPARE(history.saved.front().paths.front(), opened.session->location());
    QCOMPARE(history.saved.front().lastPath,
             std::optional<Application::RecentWorkspacePath>(opened.session->location()));
}

void NextWorkspaceOpenFlowTests::dirtyWorkspaceCannotBeReplaced()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString currentPath = directory.filePath(QStringLiteral("current.tps"));
    QVERIFY(createProfile(currentPath));
    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    const auto current = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(currentPath)}
        );
    QVERIFY(current);
    const auto selectedClass = Domain::ClassId::fromString("dirty-selection");
    QVERIFY(selectedClass.has_value());
    runtime.selectionState().setSelection(*selectedClass);
    QVERIFY(runtime.workspaceState().markDirty());

    RecordingFileDialog fileDialog;
    fileDialog.response.selection = Application::FileDialogPath(
        currentPath.toUtf8().toStdString()
        );
    RecordingHistory history;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::OpenExistingWorkspaceFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        history,
        notifications,
        [] { return false; }
        );

    const auto rejected = flow.run();

    QCOMPARE(rejected.status, Application::OpenExistingWorkspaceStatus::Failed);
    QCOMPARE(runtime.workspaceState().snapshot().session().value(), current.value());
    QCOMPARE(runtime.workspaceState().snapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Dirty);
    QVERIFY(runtime.selectionState().snapshot().classId());
    QCOMPARE(runtime.selectionState().snapshot().classId()->value(),
             selectedClass->value());
    QCOMPARE(history.loadCalls, 0);
    QVERIFY(history.saved.empty());
    QVERIFY(notifications.requests.back().message.find("unsaved changes")
            != std::string::npos);
}

void NextWorkspaceOpenFlowTests::workspaceStoreRollbackDropsPendingTransaction()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString profilePath = directory.filePath(QStringLiteral("rollback.tps"));

    WorkspaceStore store;
    QVERIFY(store.open(profilePath));
    QSqlDatabase database = store.database();
    QVERIFY(database.isOpen());
    QVERIFY(database.transaction());
    QSqlQuery createProbeTable(database);
    QVERIFY(createProbeTable.exec(
        QStringLiteral("CREATE TABLE rollback_probe (value TEXT NOT NULL)")
        ));

    QVERIFY(store.rollback());
    QSqlQuery checkProbeTable(store.database());
    QVERIFY(checkProbeTable.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type = 'table' "
        "AND name = 'rollback_probe'"
        )));
    QVERIFY(!checkProbeTable.next());
    store.close();
}

void NextWorkspaceOpenFlowTests::historyFailureDoesNotRollBackOpenedProfile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString targetPath = directory.filePath(QStringLiteral("target.tps"));
    QVERIFY(createProfile(targetPath));
    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);

    RecordingFileDialog fileDialog;
    fileDialog.response.selection = Application::FileDialogPath(
        targetPath.toUtf8().toStdString()
        );
    RecordingHistory history;
    history.failSave = true;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::OpenExistingWorkspaceFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        history,
        notifications,
        [] { return false; }
        );

    const auto opened = flow.run();

    QCOMPARE(opened.status, Application::OpenExistingWorkspaceStatus::Opened);
    QVERIFY(runtime.workspaceState().snapshot().session().has_value());
    QCOMPARE(runtime.workspaceState().snapshot().session().value(), *opened.session);
    QCOMPARE(history.loadCalls, 1);
    QCOMPARE(history.saved.size(), std::size_t(1));
    QVERIFY(opened.message.find("history could not be saved") != std::string::npos);
    QCOMPARE(notifications.requests.back().severity,
             Application::NotificationSeverity::Warning);
}

void NextWorkspaceOpenFlowTests::
openPathPolicyAcceptsTpsAndDbAndRejectsUnknownSuffix()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    const QString targetPath = directory.filePath(QStringLiteral("Case.TPS"));
    const QString lowerDbPath = directory.filePath(QStringLiteral("legacy.db"));
    const QString upperDbPath = directory.filePath(QStringLiteral("UPPER.DB"));
    QVERIFY(writeBytes(targetPath, QByteArrayLiteral("placeholder")));
    QVERIFY(writeBytes(lowerDbPath, QByteArrayLiteral("placeholder")));
    QVERIFY(writeBytes(upperDbPath, QByteArrayLiteral("placeholder")));

    const auto normalized = pathPolicy.normalizeOpenProfilePath(
        Application::FileDialogPath(targetPath.toUtf8().toStdString())
        );
    QVERIFY(normalized);
    QCOMPARE(QFileInfo(pathFor(normalized.value())).fileName(),
             QStringLiteral("Case.TPS"));

    for (const QString& acceptedPath : {lowerDbPath, upperDbPath})
    {
        const auto accepted = pathPolicy.normalizeOpenProfilePath(
            Application::FileDialogPath(acceptedPath.toUtf8().toStdString())
            );
        QVERIFY(accepted);
        QCOMPARE(QFileInfo(pathFor(accepted.value())).fileName(),
                 QFileInfo(acceptedPath).fileName());
    }

    const auto rejected = pathPolicy.normalizeOpenProfilePath(
        Application::FileDialogPath(
            directory.filePath(QStringLiteral("profile.sqlite")).toUtf8().toStdString()
            )
        );
    QVERIFY(!rejected);
    QVERIFY(rejected.error().message.find("Only .tps or .db") != std::string::npos);
}

QTEST_MAIN(NextWorkspaceOpenFlowTests)
#include "next_workspace_open_flow_tests.moc"
