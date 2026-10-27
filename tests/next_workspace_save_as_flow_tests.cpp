#include "data/database/workspace_store.h"
#include "next/application/application_runtime.h"
#include "next/application/workspace_save_as_flow.h"
#include "next/platform/workspace_profile_path_policy.h"
#include "next/platform/workspace_store_gateway.h"

#include <QDir>
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

bool writeMarker(const QString& path, const QString& marker)
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
            succeeded = query.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS save_as_probe ("
                "value TEXT NOT NULL)"
                ));
            if (succeeded)
            {
                query.prepare(QStringLiteral(
                    "INSERT INTO save_as_probe (value) VALUES (?)"
                    ));
                query.addBindValue(marker);
                succeeded = query.exec();
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return succeeded;
}

std::optional<QString> readMarker(const QString& path)
{
    const QString connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    std::optional<QString> result;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            if (query.exec(QStringLiteral(
                    "SELECT value FROM save_as_probe ORDER BY rowid DESC LIMIT 1"
                    ))
                && query.next())
            {
                result = query.value(0).toString();
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return result;
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
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return file.readAll();
}

class RecordingFileDialog final : public Application::FileDialogPort
{
public:
    Application::FileDialogResponse<Application::SavedFileDialogSelection>
        saveResponse;
    std::optional<Application::SaveFileDialogRequest> observedSaveRequest;
    int saveCalls = 0;

    Domain::Result<Application::FileDialogResponse<Application::FileDialogPath>>
    openOneFile(const Application::OpenFileDialogRequest&) override
    {
        return Domain::Result<
            Application::FileDialogResponse<Application::FileDialogPath>>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unexpected open-file dialog request.",
            .recoverable = false
        });
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
    saveFile(const Application::SaveFileDialogRequest& request) override
    {
        ++saveCalls;
        observedSaveRequest = request;
        return Domain::Result<Application::FileDialogResponse<
            Application::SavedFileDialogSelection>>::success(saveResponse);
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
    mutable Application::RecentWorkspaceHistory history;
    mutable int loadCalls = 0;
    mutable int saveCalls = 0;
    mutable bool failSave = false;
    mutable std::vector<Application::RecentWorkspaceHistory> saved;

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
        saved.push_back(value);
        if (failSave)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected history save failure",
                .recoverable = true
            });
        }
        history = value;
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

class NextWorkspaceSaveAsFlowTests final : public QObject
{
    Q_OBJECT

private slots:
    void savesDatabaseSnapshotSwitchesLocationAndPreservesSessionIdentity();
    void cancelAndPendingSetupLeaveWorkspaceAndHistoryUntouched();
    void sameFileAliasIsSafeAndDoesNotDeleteSource();
    void copyFailurePreservesActiveSessionAndExistingDestinationBytes();
    void historyFailureKeepsSaveAsSuccessful();
};

void NextWorkspaceSaveAsFlowTests::
savesDatabaseSnapshotSwitchesLocationAndPreservesSessionIdentity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.tps"));
    const QString selectedPath = directory.filePath(QStringLiteral("copy.db"));
    const QString expectedPath = directory.filePath(QStringLiteral("copy.tps"));
    QVERIFY(createProfile(sourcePath));
    QVERIFY(writeMarker(sourcePath, QStringLiteral("persisted before save-as")));

    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    const auto opened = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(sourcePath)}
        );
    QVERIFY(opened);
    const Domain::WorkspaceId originalId = opened.value().workspaceId();

    RecordingFileDialog fileDialog;
    fileDialog.saveResponse.selection = Application::SavedFileDialogSelection{
        .path = Application::FileDialogPath(selectedPath.toUtf8().toStdString())
    };
    RecordingHistory history;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::WorkspaceSaveAsFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        &history,
        notifications,
        Application::FileDialogPath(
            QFileInfo(sourcePath).absolutePath().toUtf8().toStdString()
            ),
        QFileInfo(sourcePath).fileName().toUtf8().toStdString(),
        [] { return false; }
        );

    const auto saved = flow.run();

    QCOMPARE(saved.status, Application::WorkspaceSaveAsStatus::Saved);
    QVERIFY(saved.location.has_value());
    QCOMPARE(pathFor(*saved.location), QFileInfo(expectedPath).canonicalFilePath());
    QVERIFY(QFileInfo::exists(expectedPath));
    QVERIFY(!QFileInfo::exists(selectedPath));
    QCOMPARE(runtime.workspaceState().snapshot().session()->workspaceId(), originalId);
    QCOMPARE(runtime.workspaceState().snapshot().session()->location(), *saved.location);
    QCOMPARE(runtime.workspaceState().snapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Clean);
    QCOMPARE(fileDialog.saveCalls, 1);
    QVERIFY(fileDialog.observedSaveRequest.has_value());
    QCOMPARE(fileDialog.observedSaveRequest->purpose,
             Application::FileDialogPurpose::TeacherProfile);
    QCOMPARE(fileDialog.observedSaveRequest->title,
             std::string("Save Teacher Profile"));
    QCOMPARE(fileDialog.observedSaveRequest->nameFilters,
             std::vector<std::string>{"ClassMngr Teacher Profile (*.tps)"});
    QCOMPARE(fileDialog.observedSaveRequest->defaultSuffix, std::string("tps"));
    QVERIFY(fileDialog.observedSaveRequest->confirmOverwrite);
    QCOMPARE(QString::fromUtf8(
                 fileDialog.observedSaveRequest->initialDirectory.value().data(),
                 static_cast<qsizetype>(
                     fileDialog.observedSaveRequest->initialDirectory.value().size()
                     )
                 ),
             QFileInfo(sourcePath).absolutePath());
    QCOMPARE(fileDialog.observedSaveRequest->suggestedFileName,
             std::string("source.tps"));
    QCOMPARE(history.loadCalls, 1);
    QCOMPARE(history.saveCalls, 1);
    QCOMPARE(history.history.paths.size(), std::size_t(1));
    QCOMPARE(history.history.paths.front(), *saved.location);
    QCOMPARE(readMarker(expectedPath),
             std::optional<QString>(QStringLiteral("persisted before save-as")));

    QCOMPARE(runtime.workspaceState().snapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Clean);
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
    QVERIFY(!runtime.workspaceState().snapshot().session().has_value());

    const auto reopened = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(expectedPath)}
        );
    QVERIFY(reopened);
    QCOMPARE(readMarker(expectedPath),
             std::optional<QString>(QStringLiteral("persisted before save-as")));
    QVERIFY(QFileInfo::exists(sourcePath));
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
}

void NextWorkspaceSaveAsFlowTests::
cancelAndPendingSetupLeaveWorkspaceAndHistoryUntouched()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("cancel.tps"));
    QVERIFY(createProfile(sourcePath));

    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    const auto opened = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(sourcePath)}
        );
    QVERIFY(opened);

    RecordingFileDialog fileDialog;
    RecordingHistory history;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    bool setupPending = false;
    auto makeFlow = [&] {
        return Application::WorkspaceSaveAsFlow(
            fileDialog,
            pathPolicy,
            runtime.workspaceCoordinator(),
            &history,
            notifications,
            Application::FileDialogPath(
                QFileInfo(sourcePath).absolutePath().toUtf8().toStdString()
                ),
            QFileInfo(sourcePath).fileName().toUtf8().toStdString(),
            [&setupPending] { return setupPending; }
            );
    };

    const auto canceled = makeFlow().run();
    QCOMPARE(canceled.status, Application::WorkspaceSaveAsStatus::Canceled);
    QCOMPARE(runtime.workspaceState().snapshot().session(),
             std::optional<Application::WorkspaceSession>(opened.value()));
    QCOMPARE(fileDialog.saveCalls, 1);
    QCOMPARE(history.loadCalls, 0);
    QCOMPARE(history.saveCalls, 0);

    setupPending = true;
    const auto blocked = makeFlow().run();
    QCOMPARE(blocked.status, Application::WorkspaceSaveAsStatus::Blocked);
    QCOMPARE(runtime.workspaceState().snapshot().session(),
             std::optional<Application::WorkspaceSession>(opened.value()));
    QCOMPARE(fileDialog.saveCalls, 1);
    QCOMPARE(history.loadCalls, 0);
    QCOMPARE(history.saveCalls, 0);
}

void NextWorkspaceSaveAsFlowTests::
sameFileAliasIsSafeAndDoesNotDeleteSource()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("alias.tps"));
    QVERIFY(createProfile(sourcePath));
    QVERIFY(writeMarker(sourcePath, QStringLiteral("source remains")));

    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    const auto opened = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(sourcePath)}
        );
    QVERIFY(opened);

    RecordingFileDialog fileDialog;
    const QString aliasPath = directory.filePath(QStringLiteral("./alias.tps"));
    fileDialog.saveResponse.selection = Application::SavedFileDialogSelection{
        .path = Application::FileDialogPath(aliasPath.toUtf8().toStdString())
    };
    RecordingHistory history;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::WorkspaceSaveAsFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        &history,
        notifications,
        Application::FileDialogPath(
            QFileInfo(sourcePath).absolutePath().toUtf8().toStdString()
            ),
        QFileInfo(sourcePath).fileName().toUtf8().toStdString(),
        [] { return false; }
        );

    const auto saved = flow.run();

    QCOMPARE(saved.status, Application::WorkspaceSaveAsStatus::Saved);
    QCOMPARE(runtime.workspaceState().snapshot().session()->workspaceId(),
             opened.value().workspaceId());
    QVERIFY(QFileInfo::exists(sourcePath));
    QCOMPARE(readMarker(sourcePath),
             std::optional<QString>(QStringLiteral("source remains")));
    QCOMPARE(history.loadCalls, 1);
    QCOMPARE(history.saveCalls, 1);
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
    const auto reopened = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(sourcePath)}
        );
    QVERIFY(reopened);
    QCOMPARE(readMarker(sourcePath),
             std::optional<QString>(QStringLiteral("source remains")));
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
}

void NextWorkspaceSaveAsFlowTests::
copyFailurePreservesActiveSessionAndExistingDestinationBytes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("failure.tps"));
    const QString destinationDirectory = directory.filePath(QStringLiteral("locked"));
    const QString destinationPath = QDir(destinationDirectory).filePath(
        QStringLiteral("existing.tps")
        );
    QVERIFY(createProfile(sourcePath));
    QVERIFY(QDir().mkpath(destinationDirectory));
    const QByteArray originalDestinationBytes = QByteArrayLiteral("keep these bytes");
    QVERIFY(writeBytes(destinationPath, originalDestinationBytes));

    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    const auto opened = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(sourcePath)}
        );
    QVERIFY(opened);

    RecordingFileDialog fileDialog;
    fileDialog.saveResponse.selection = Application::SavedFileDialogSelection{
        .path = Application::FileDialogPath(destinationPath.toUtf8().toStdString())
    };
    RecordingHistory history;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::WorkspaceSaveAsFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        &history,
        notifications,
        Application::FileDialogPath(
            QFileInfo(sourcePath).absolutePath().toUtf8().toStdString()
            ),
        QFileInfo(sourcePath).fileName().toUtf8().toStdString(),
        [] { return false; }
        );

    const QFile::Permissions originalPermissions = QFileInfo(destinationDirectory)
        .permissions();
    QVERIFY(QFile::setPermissions(
        destinationDirectory,
        QFile::ReadOwner | QFile::ExeOwner
        ));
    const auto failed = flow.run();
    const bool restoredPermissions = QFile::setPermissions(
        destinationDirectory,
        originalPermissions
        );

    QVERIFY(restoredPermissions);
    QCOMPARE(failed.status, Application::WorkspaceSaveAsStatus::Failed);
    QCOMPARE(runtime.workspaceState().snapshot().session(),
             std::optional<Application::WorkspaceSession>(opened.value()));
    QVERIFY(QFileInfo::exists(sourcePath));
    QCOMPARE(readBytes(destinationPath), originalDestinationBytes);
    QCOMPARE(history.loadCalls, 0);
    QCOMPARE(history.saveCalls, 0);
    QVERIFY(!notifications.requests.empty());
    QCOMPARE(notifications.requests.back().severity,
             Application::NotificationSeverity::Error);
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
}

void NextWorkspaceSaveAsFlowTests::historyFailureKeepsSaveAsSuccessful()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("history.tps"));
    const QString destinationPath = directory.filePath(QStringLiteral("history-copy.tps"));
    QVERIFY(createProfile(sourcePath));

    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    const auto opened = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(sourcePath)}
        );
    QVERIFY(opened);

    RecordingFileDialog fileDialog;
    fileDialog.saveResponse.selection = Application::SavedFileDialogSelection{
        .path = Application::FileDialogPath(destinationPath.toUtf8().toStdString())
    };
    RecordingHistory history;
    history.failSave = true;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::WorkspaceSaveAsFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        &history,
        notifications,
        Application::FileDialogPath(
            QFileInfo(sourcePath).absolutePath().toUtf8().toStdString()
            ),
        QFileInfo(sourcePath).fileName().toUtf8().toStdString(),
        [] { return false; }
        );

    const auto saved = flow.run();

    QCOMPARE(saved.status, Application::WorkspaceSaveAsStatus::Saved);
    QCOMPARE(pathFor(runtime.workspaceState().snapshot().session()->location()),
             QFileInfo(destinationPath).canonicalFilePath());
    QVERIFY(QFileInfo::exists(destinationPath));
    QCOMPARE(history.loadCalls, 1);
    QCOMPARE(history.saveCalls, 1);
    QVERIFY(saved.message.find("recent profile history could not be saved")
            != std::string::npos);
    QCOMPARE(notifications.requests.back().severity,
             Application::NotificationSeverity::Warning);
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
}

QTEST_MAIN(NextWorkspaceSaveAsFlowTests)
#include "next_workspace_save_as_flow_tests.moc"
