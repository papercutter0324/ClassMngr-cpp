#include "data/database/workspace_store.h"
#include "next/application/application_runtime.h"
#include "next/application/workspace_export_flow.h"
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
                "CREATE TABLE IF NOT EXISTS export_probe (value TEXT NOT NULL)"
                ));
            if (succeeded)
            {
                query.prepare(QStringLiteral(
                    "INSERT INTO export_probe (value) VALUES (?)"
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
                    "SELECT value FROM export_probe ORDER BY rowid DESC LIMIT 1"
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

class NextWorkspaceExportFlowTests final : public QObject
{
    Q_OBJECT

private slots:
    void exportsCommittedSnapshotWithoutChangingActiveSessionOrSelection();
    void cancelAndPendingSetupLeaveWorkspaceUntouched();
    void sameFileAliasIsSafeAndPreservesTheSource();
    void copyFailurePreservesActiveSessionAndExistingDestinationBytes();
    void directoryPreferenceWarningDoesNotFailExport();
};

void NextWorkspaceExportFlowTests::
exportsCommittedSnapshotWithoutChangingActiveSessionOrSelection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.tps"));
    const QString selectedPath = directory.filePath(QStringLiteral("snapshot.db"));
    const QString expectedPath = directory.filePath(QStringLiteral("snapshot.tps"));
    QVERIFY(createProfile(sourcePath));
    QVERIFY(writeMarker(sourcePath, QStringLiteral("committed before export")));

    Platform::WorkspaceStoreGateway gateway;
    Application::ApplicationRuntime runtime(gateway);
    const auto opened = runtime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(sourcePath)}
        );
    QVERIFY(opened);
    const auto selectedClass = Domain::ClassId::fromString("selected-before-export");
    QVERIFY(selectedClass.has_value());
    runtime.selectionState().setSelection(*selectedClass);
    QVERIFY(runtime.workspaceState().markDirty());
    const Application::WorkspaceStateSnapshot before =
        runtime.workspaceState().snapshot();
    const Application::SelectionStateSnapshot selectionBefore =
        runtime.selectionState().snapshot();

    RecordingFileDialog fileDialog;
    fileDialog.saveResponse.selection = Application::SavedFileDialogSelection{
        .path = Application::FileDialogPath(selectedPath.toUtf8().toStdString())
    };
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::WorkspaceExportFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        notifications,
        Application::FileDialogPath(
            QFileInfo(sourcePath).absolutePath().toUtf8().toStdString()
            ),
        [] { return false; }
        );

    const auto exported = flow.run();

    QCOMPARE(exported.status, Application::WorkspaceExportStatus::Exported);
    QVERIFY(exported.location.has_value());
    QCOMPARE(pathFor(*exported.location), QFileInfo(expectedPath).canonicalFilePath());
    QVERIFY(QFileInfo::exists(expectedPath));
    QVERIFY(!QFileInfo::exists(selectedPath));
    QCOMPARE(runtime.workspaceState().snapshot(), before);
    QCOMPARE(runtime.workspaceState().snapshot().session(),
             std::optional<Application::WorkspaceSession>(opened.value()));
    QCOMPARE(runtime.workspaceState().snapshot().unsavedState(),
             Application::WorkspaceUnsavedState::Dirty);
    QCOMPARE(runtime.selectionState().snapshot(), selectionBefore);
    QCOMPARE(runtime.workspaceState().snapshot().session()->location(),
             opened.value().location());
    QCOMPARE(fileDialog.saveCalls, 1);
    QVERIFY(fileDialog.observedSaveRequest.has_value());
    QCOMPARE(fileDialog.observedSaveRequest->purpose,
             Application::FileDialogPurpose::ExportTeacherProfile);
    QCOMPARE(fileDialog.observedSaveRequest->title,
             std::string("Export Teacher Profile"));
    QCOMPARE(fileDialog.observedSaveRequest->nameFilters,
             std::vector<std::string>{"ClassMngr Teacher Profile (*.tps)"});
    QCOMPARE(fileDialog.observedSaveRequest->defaultSuffix, std::string("tps"));
    QVERIFY(fileDialog.observedSaveRequest->confirmOverwrite);
    QCOMPARE(fileDialog.observedSaveRequest->suggestedFileName, std::string{});
    QCOMPARE(QString::fromUtf8(
                 fileDialog.observedSaveRequest->initialDirectory.value().data(),
                 static_cast<qsizetype>(
                     fileDialog.observedSaveRequest->initialDirectory.value().size()
                     )
                 ),
             QFileInfo(sourcePath).absolutePath());
    QCOMPARE(readMarker(expectedPath),
             std::optional<QString>(QStringLiteral("committed before export")));
    QVERIFY(!notifications.requests.empty());
    QCOMPARE(notifications.requests.back().severity,
             Application::NotificationSeverity::Information);

    Platform::WorkspaceStoreGateway exportedGateway;
    Application::ApplicationRuntime exportedRuntime(exportedGateway);
    const auto reopened = exportedRuntime.workspaceCoordinator().openWorkspace(
        Application::OpenWorkspaceRequest{location(expectedPath)}
        );
    QVERIFY(reopened);
    QCOMPARE(readMarker(expectedPath),
             std::optional<QString>(QStringLiteral("committed before export")));
    QVERIFY(exportedRuntime.workspaceCoordinator().closeWorkspace());
    QVERIFY(runtime.workspaceState().markDiscarded());
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
}

void NextWorkspaceExportFlowTests::
cancelAndPendingSetupLeaveWorkspaceUntouched()
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
    const auto selectedClass = Domain::ClassId::fromString("cancel-selection");
    QVERIFY(selectedClass.has_value());
    runtime.selectionState().setSelection(*selectedClass);
    QVERIFY(runtime.workspaceState().markDirty());
    const auto before = runtime.workspaceState().snapshot();
    const auto selectionBefore = runtime.selectionState().snapshot();

    RecordingFileDialog fileDialog;
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    bool setupPending = false;
    auto makeFlow = [&] {
        return Application::WorkspaceExportFlow(
            fileDialog,
            pathPolicy,
            runtime.workspaceCoordinator(),
            notifications,
            Application::FileDialogPath(
                QFileInfo(sourcePath).absolutePath().toUtf8().toStdString()
                ),
            [&setupPending] { return setupPending; }
            );
    };

    const auto canceled = makeFlow().run();
    QCOMPARE(canceled.status, Application::WorkspaceExportStatus::Canceled);
    QCOMPARE(runtime.workspaceState().snapshot(), before);
    QCOMPARE(runtime.selectionState().snapshot(), selectionBefore);
    QCOMPARE(fileDialog.saveCalls, 1);
    QVERIFY(notifications.requests.empty());

    setupPending = true;
    const auto blocked = makeFlow().run();
    QCOMPARE(blocked.status, Application::WorkspaceExportStatus::Blocked);
    QCOMPARE(runtime.workspaceState().snapshot(), before);
    QCOMPARE(runtime.selectionState().snapshot(), selectionBefore);
    QCOMPARE(fileDialog.saveCalls, 1);
    QCOMPARE(notifications.requests.size(), std::size_t(1));
    QCOMPARE(notifications.requests.back().severity,
             Application::NotificationSeverity::Warning);
    const auto closeWhileDirty = runtime.workspaceCoordinator().closeWorkspace();
    QVERIFY(!closeWhileDirty);
    QVERIFY(runtime.workspaceState().markDiscarded());
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
}

void NextWorkspaceExportFlowTests::
sameFileAliasIsSafeAndPreservesTheSource()
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
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::WorkspaceExportFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        notifications,
        Application::FileDialogPath(
            QFileInfo(sourcePath).absolutePath().toUtf8().toStdString()
            ),
        [] { return false; }
        );

    const auto exported = flow.run();

    QCOMPARE(exported.status, Application::WorkspaceExportStatus::Exported);
    QCOMPARE(runtime.workspaceState().snapshot().session(),
             std::optional<Application::WorkspaceSession>(opened.value()));
    QVERIFY(QFileInfo::exists(sourcePath));
    QCOMPARE(readMarker(sourcePath),
             std::optional<QString>(QStringLiteral("source remains")));
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
}

void NextWorkspaceExportFlowTests::
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
    const auto before = runtime.workspaceState().snapshot();

    RecordingFileDialog fileDialog;
    fileDialog.saveResponse.selection = Application::SavedFileDialogSelection{
        .path = Application::FileDialogPath(destinationPath.toUtf8().toStdString())
    };
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::WorkspaceExportFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        notifications,
        Application::FileDialogPath(
            QFileInfo(sourcePath).absolutePath().toUtf8().toStdString()
            ),
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
    QCOMPARE(failed.status, Application::WorkspaceExportStatus::Failed);
    QCOMPARE(runtime.workspaceState().snapshot(), before);
    QCOMPARE(runtime.workspaceState().snapshot().session(),
             std::optional<Application::WorkspaceSession>(opened.value()));
    QVERIFY(QFileInfo::exists(sourcePath));
    QCOMPARE(readBytes(destinationPath), originalDestinationBytes);
    QVERIFY(!notifications.requests.empty());
    QCOMPARE(notifications.requests.back().severity,
             Application::NotificationSeverity::Error);
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
}

void NextWorkspaceExportFlowTests::directoryPreferenceWarningDoesNotFailExport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("warning.tps"));
    const QString destinationPath = directory.filePath(QStringLiteral("warning-copy.tps"));
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
    fileDialog.saveResponse.preferenceWarning = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected directory preference failure",
        .recoverable = true
    };
    RecordingNotifications notifications;
    Platform::WorkspaceProfilePathPolicy pathPolicy;
    Application::WorkspaceExportFlow flow(
        fileDialog,
        pathPolicy,
        runtime.workspaceCoordinator(),
        notifications,
        Application::FileDialogPath(
            QFileInfo(sourcePath).absolutePath().toUtf8().toStdString()
            ),
        [] { return false; }
        );

    const auto exported = flow.run();

    QCOMPARE(exported.status, Application::WorkspaceExportStatus::Exported);
    QVERIFY(exported.location.has_value());
    QVERIFY(QFileInfo::exists(destinationPath));
    QVERIFY(exported.message.find("directory could not be remembered")
            != std::string::npos);
    QCOMPARE(notifications.requests.back().severity,
             Application::NotificationSeverity::Warning);
    QCOMPARE(runtime.workspaceState().snapshot().session(),
             std::optional<Application::WorkspaceSession>(opened.value()));
    QVERIFY(runtime.workspaceCoordinator().closeWorkspace());
}

QTEST_MAIN(NextWorkspaceExportFlowTests)
#include "next_workspace_export_flow_tests.moc"
