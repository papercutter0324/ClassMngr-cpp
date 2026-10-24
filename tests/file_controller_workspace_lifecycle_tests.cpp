#include "app/controllers/file_controller.h"
#include "app/controllers/file_controller_workspace_messages.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/settingsmanager.h"
#include "data/database/database_schema_manager.h"
#include "fakes/fake_file_dialog_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "next/application/initial_setup_lifecycle.h"
#include "next/application/workspace_creation_lifecycle.h"
#include "next/platform/settings_manager_last_database_directory_port.h"
#include "next/platform/settings_manager_recent_workspace_history_port.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSettings>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <utility>

struct FileControllerWorkspaceLifecycleTestAccess final
{
    static void installWorkspaceCreationLifecycle(
        FileController& controller,
        ClassMngr::Next::Application::WorkspaceCreationLifecyclePort& port
        )
    {
        controller.m_workspaceCreationLifecycle = std::make_unique<
            ClassMngr::Next::Application::WorkspaceCreationLifecycle
            >(port);
    }

    [[nodiscard]] static QString currentFile(
        const FileController& controller
        )
    {
        return controller.m_currentFile;
    }

    [[nodiscard]] static ClassMngr::Next::Application::InitialSetupBeginResult
    beginInitialSetup(
        FileController& controller,
        ClassMngr::Next::Application::InitialSetupLifecyclePort& port,
        ClassMngr::Next::Application::WorkspaceLocation profile
        )
    {
        controller.m_initialSetupLifecycle = std::make_unique<
            ClassMngr::Next::Application::InitialSetupLifecycle
            >(port);
        return controller.m_initialSetupLifecycle->begin(std::move(profile));
    }
};

namespace
{
bool writeFile(
    const QString& path,
    const QByteArray& contents
    )
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    return file.write(contents) == contents.size();
}

bool createSchemaProfile(const QString& path)
{
    ApplicationServices services;
    if (!services.openDatabase(path))
    {
        return false;
    }
    services.closeDatabase();
    return QFileInfo::exists(path);
}

bool writeProfileMarker(
    const QString& path,
    const QString& value
    )
{
    const QString connectionName = QStringLiteral("recovery-marker-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            query.prepare(
                QStringLiteral(
                    "INSERT OR REPLACE INTO app_settings (key, value) "
                    "VALUES ('recovery-test-marker', :value)"
                    )
                );
            query.bindValue(QStringLiteral(":value"), value);
            success = query.exec();
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

QString normalCreateRecoveryPath(
    const QString& directory,
    const QString& targetName
    )
{
    return QDir(directory).filePath(
        QStringLiteral(".%1.workspace-create-%2.recovery")
            .arg(
                targetName,
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
        );
}

QString initialSetupRecoveryPath(
    const QString& directory,
    const QString& targetName
    )
{
    return QDir(directory).filePath(
        QStringLiteral(".%1.initial-setup-%2.backup")
            .arg(
                targetName,
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
        );
}

ClassMngr::Next::Application::WorkspaceLocation workspaceLocation(
    const QString& path
    )
{
    const QByteArray encoded = path.toUtf8();
    return ClassMngr::Next::Application::WorkspaceLocation(
        std::string(
            encoded.constData(),
            static_cast<std::size_t>(encoded.size())
            )
        );
}

QString qtPath(
    const ClassMngr::Next::Application::WorkspaceLocation& location
    )
{
    return QString::fromUtf8(
        location.value().data(),
        static_cast<qsizetype>(location.value().size())
        );
}

class ScriptedInitialSetupLifecyclePort final
    : public ClassMngr::Next::Application::InitialSetupLifecyclePort
{
public:
    ScriptedInitialSetupLifecyclePort(
        QString originalBackup,
        QString incompleteRecovery
        )
        : originalBackupPath(std::move(originalBackup)),
          incompleteRecoveryPath(std::move(incompleteRecovery))
    {
    }

    [[nodiscard]] bool hasActiveWorkspace() const override
    {
        return active;
    }

    [[nodiscard]] ClassMngr::Next::Domain::Result<void>
    closeActiveWorkspace() override
    {
        active = false;
        ++closeCalls;
        if (servicesToClose)
        {
            servicesToClose->closeDatabase();
        }
        return ClassMngr::Next::Domain::Result<void>::success();
    }

    [[nodiscard]] bool profileExists(
        const ClassMngr::Next::Application::WorkspaceLocation& location
        ) const override
    {
        return QFileInfo::exists(qtPath(location));
    }

    [[nodiscard]] ClassMngr::Next::Application::WorkspaceLocation
    recoveryLocationFor(
        const ClassMngr::Next::Application::WorkspaceLocation& profile
        ) override
    {
        Q_UNUSED(profile);
        ++recoveryPathCount;
        return workspaceLocation(
            recoveryPathCount == 1
                ? originalBackupPath
                : incompleteRecoveryPath
            );
    }

    [[nodiscard]] bool moveProfile(
        const ClassMngr::Next::Application::WorkspaceLocation& source,
        const ClassMngr::Next::Application::WorkspaceLocation& destination
        ) override
    {
        ++moveCalls;
        const QString sourcePath = qtPath(source);
        const QString destinationPath = qtPath(destination);
        if (
            failMoveCalls.contains(moveCalls)
            || QFileInfo::exists(destinationPath)
            || !QFileInfo::exists(sourcePath)
            )
        {
            return false;
        }
        return QFile::rename(sourcePath, destinationPath);
    }

    [[nodiscard]] bool removeProfile(
        const ClassMngr::Next::Application::WorkspaceLocation& location
        ) override
    {
        ++removeCalls;
        if (failRemoveCalls.contains(removeCalls))
        {
            return false;
        }
        return QFile::remove(qtPath(location));
    }

    [[nodiscard]] ClassMngr::Next::Domain::Result<
        ClassMngr::Next::Application::WorkspaceSession
        > createWorkspace(
            const ClassMngr::Next::Application::CreateWorkspaceRequest& request
            ) override
    {
        if (!writeFile(qtPath(request.location), incompleteProfileContents))
        {
            return ClassMngr::Next::Domain::Result<
                ClassMngr::Next::Application::WorkspaceSession
                >::failure(
                    ClassMngr::Next::Domain::OperationError{
                        .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                        .message = "Unable to create the scripted setup profile.",
                        .recoverable = true
                    }
                    );
        }

        active = true;
        return ClassMngr::Next::Domain::Result<
            ClassMngr::Next::Application::WorkspaceSession
            >::success(
                ClassMngr::Next::Application::WorkspaceSession(
                    *ClassMngr::Next::Domain::WorkspaceId::fromString(
                        "scripted-initial-setup"
                        ),
                    request.location
                    )
                );
    }

    QString originalBackupPath;
    QString incompleteRecoveryPath;
    QByteArray incompleteProfileContents = QByteArrayLiteral(
        "incomplete setup profile"
        );
    std::set<int> failMoveCalls{3, 4};
    std::set<int> failRemoveCalls;
    ApplicationServices* servicesToClose = nullptr;
    bool active = false;
    int moveCalls = 0;
    int removeCalls = 0;
    int closeCalls = 0;
    int recoveryPathCount = 0;
};

class FailingWorkspaceCreationPort final
    : public ClassMngr::Next::Application::WorkspaceCreationLifecyclePort
{
public:
    explicit FailingWorkspaceCreationPort(QString selectedPath)
        : targetPath(std::move(selectedPath))
    {
    }

    [[nodiscard]] bool profileExists(
        const ClassMngr::Next::Application::WorkspaceLocation& location
        ) const override
    {
        return QFileInfo::exists(toQString(location));
    }

    [[nodiscard]] ClassMngr::Next::Application::WorkspaceLocation
    recoveryLocationFor(
        const ClassMngr::Next::Application::WorkspaceLocation& profile
        ) override
    {
        const QFileInfo info(toQString(profile));
        const QString recoveryPath = info.absoluteDir().filePath(
            QStringLiteral(".%1.test-recovery-%2")
                .arg(info.fileName())
                .arg(recoveryPaths.size() + 1)
            );
        recoveryPaths.append(recoveryPath);
        return fromQString(recoveryPath);
    }

    [[nodiscard]] bool moveProfile(
        const ClassMngr::Next::Application::WorkspaceLocation& source,
        const ClassMngr::Next::Application::WorkspaceLocation& destination
        ) override
    {
        const QString sourcePath = toQString(source);
        const QString destinationPath = toQString(destination);
        if (
            (createAttempted && sourcePath == targetPath)
            || QFileInfo::exists(destinationPath)
            )
        {
            return false;
        }

        return QFile::rename(sourcePath, destinationPath);
    }

    [[nodiscard]] bool removeProfile(
        const ClassMngr::Next::Application::WorkspaceLocation& location
        ) override
    {
        // Keep the failed replacement at the selected path so restoring the
        // original is rejected without overwriting either file.
        if (
            retainPartialReplacementAtTarget
            && toQString(location) == targetPath
            )
        {
            return false;
        }

        return QFile::remove(toQString(location));
    }

    [[nodiscard]] ClassMngr::Next::Domain::Result<void>
    closeActiveWorkspace() override
    {
        workspaceCloseAttempted = true;
        return ClassMngr::Next::Domain::Result<void>::success();
    }

    [[nodiscard]] ClassMngr::Next::Domain::Result<
        ClassMngr::Next::Application::WorkspaceSession
        > createWorkspace(
            const ClassMngr::Next::Application::CreateWorkspaceRequest& request
            ) override
    {
        createAttempted = true;
        const QString path = toQString(request.location);
        if (!writeFile(path, partialReplacementContents))
        {
            return ClassMngr::Next::Domain::Result<
                ClassMngr::Next::Application::WorkspaceSession
                >::failure(
                    ClassMngr::Next::Domain::OperationError{
                        .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                        .message = "Injected workspace creation failure.",
                        .recoverable = true
                    }
                    );
        }

        return ClassMngr::Next::Domain::Result<
            ClassMngr::Next::Application::WorkspaceSession
            >::failure(
                ClassMngr::Next::Domain::OperationError{
                    .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                    .message = "Injected workspace creation failure.",
                    .recoverable = true
                }
                );
    }

    QString targetPath;
    QStringList recoveryPaths;
    QByteArray partialReplacementContents = QByteArrayLiteral(
        "partial replacement must remain untouched"
        );
    bool retainPartialReplacementAtTarget = true;
    bool workspaceCloseAttempted = false;
    bool createAttempted = false;

private:
    [[nodiscard]] static QString toQString(
        const ClassMngr::Next::Application::WorkspaceLocation& location
        )
    {
        return QString::fromUtf8(
            location.value().data(),
            static_cast<qsizetype>(location.value().size())
            );
    }

    [[nodiscard]] static ClassMngr::Next::Application::WorkspaceLocation
    fromQString(const QString& path)
    {
        const QByteArray encoded = path.toUtf8();
        return ClassMngr::Next::Application::WorkspaceLocation(
            std::string(
                encoded.constData(),
                static_cast<std::size_t>(encoded.size())
                )
            );
    }
};

QSettings::Status syncAndReturnAccessError(QSettings& settings)
{
    settings.sync();
    return QSettings::AccessError;
}

QByteArray readFile(
    const QString& path
    )
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    return file.readAll();
}

bool materializeSqlFixture(
    const QString& fixturePath,
    const QString& databasePath,
    QString* errorMessage
    )
{
    QFile fixture(fixturePath);
    if (!fixture.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        *errorMessage = QStringLiteral("Opening SQL fixture failed: %1")
            .arg(fixture.errorString());
        return false;
    }

    const QString connectionName =
        QStringLiteral("file-controller-legacy-fixture-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(databasePath);
        if (!database.open())
        {
            *errorMessage = database.lastError().text();
        }
        else
        {
            success = true;
            QSqlQuery query(database);
            const QStringList statements = QString::fromUtf8(fixture.readAll())
                .split(QChar(';'), Qt::SkipEmptyParts);
            for (const QString& rawStatement : statements)
            {
                const QString statement = rawStatement.trimmed();
                if (!statement.isEmpty() && !query.exec(statement))
                {
                    *errorMessage = QStringLiteral(
                        "Executing SQL fixture statement failed: %1 (%2)"
                        ).arg(
                            statement.simplified().left(160),
                            query.lastError().text()
                            );
                    success = false;
                    break;
                }
            }
        }

        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

struct LegacyDatabaseInspection
{
    int schemaVersion = -1;
    int unassignedTeacherIsNull = -1;
};

bool inspectLegacyDatabase(
    const QString& databasePath,
    LegacyDatabaseInspection* inspection,
    QString* errorMessage
    )
{
    const QString connectionName =
        QStringLiteral("file-controller-legacy-inspection-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(databasePath);
        if (!database.open())
        {
            *errorMessage = database.lastError().text();
        }
        else
        {
            const RepositoryResult<int> schemaVersion =
                DatabaseSchemaManager::schemaVersion(database);
            if (!schemaVersion)
            {
                *errorMessage = schemaVersion.error().diagnostic;
            }
            else
            {
                inspection->schemaVersion = *schemaVersion;
                QSqlQuery query(database);
                if (!query.exec(QStringLiteral(
                    "SELECT teacher_id IS NULL FROM class_info WHERE class_id=1"
                    )))
                {
                    *errorMessage = query.lastError().text();
                }
                else if (!query.next())
                {
                    *errorMessage = QStringLiteral(
                        "The legacy class information row was not readable."
                        );
                }
                else
                {
                    inspection->unassignedTeacherIsNull = query.value(0).toInt();
                    success = true;
                }
            }
        }

        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

QStringList initialSetupBackups(
    const QTemporaryDir& workspaceRoot,
    const QString& fileName
    )
{
    return QDir(workspaceRoot.path()).entryList(
        QStringList{
            QStringLiteral(".%1.initial-setup-*.backup")
                .arg(fileName)
        },
        QDir::Files | QDir::Hidden,
        QDir::Name
        );
}

QString fileControllerSource()
{
    QFile source(
        QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
            QStringLiteral("src/app/controllers/file_controller.cpp")
            )
        );
    if (!source.open(QIODevice::ReadOnly))
    {
        return {};
    }

    QString contents = QString::fromUtf8(source.readAll());
    contents.remove(QRegularExpression(QStringLiteral("\\s+")));
    return contents;
}

void connectFileActions(
    FileController& controller,
    ActionRegistry& actions
    )
{
    actions.createActions();
    controller.connectActions(actions);
}
}

class FileControllerWorkspaceLifecycleTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void nullServicesAreSafe();
    void storedDirectoryFallbackIsUsedByFileDialogs();
    void startupPersistsDirectoryThroughTypedPort();
    void normalCreateUsesCoordinatorAndUpdatesRecent();
    void recoverInterruptedProfileMatchesTargetNamesContainingNewlines();
    void recoverInterruptedProfileCopiesToAbsentTargetAndIsIdempotent();
    void recoverInterruptedProfileRequiresExplicitChoiceAndIgnoresLookalikes();
    void recoverInterruptedProfileCancellationAndCopyFailurePreserveFiles();
    void recoverInterruptedProfileReportsWhenNoCandidatesExist();
    void recoverInterruptedInitialSetupBackupCopiesAndCanBeRediscovered();
    void recoverInterruptedInitialSetupBackupPreservesPartialTarget();
    void recoverInterruptedInitialSetupCandidatesRequireExplicitChoiceAndIgnoreLookalikes();
    void normalCreateUsesUnicodeSessionLocationForCurrentFile();
    void normalOpenUsesUnicodeSessionLocationAndPreservesHistory();
    void recentFilesDeduplicateRawAndNormalizedPaths();
    void recentActionReopensActiveWorkspaceAndMovesItToMruFront();
    void recentFilesKeepNewestFirstAndCapAtTen();
    void pruningRemovesMissingPathAndClearsLastFile();
    void clearRecentFilesClearsListAndLastFile();
    void successfulOpenSurvivesRecentHistoryPersistenceFailure();
    void failedRecentHistoryClearWarnsAndPreservesMenu();
    void failedRecentHistoryPruneWarnsAndPreservesHistory();
    void recentFilesPreserveUnicodePaths();
    void startupUsesListFirstAndLastFileFallback();
    void normalCreateReplacesExistingTarget();
    void normalCreateFailureRestoresExistingTarget();
    void normalCreateFailureOnDifferentTargetRestoresActiveProfileAndHistory();
    void normalCreateFailureOnSameTargetRestoresActiveProfileAndHistory();
    void normalCreateFailureWithoutSafeRestoreLeavesNoActiveProfile();
    void workspaceCreationRollbackFailureRetainsOriginalRecoveryCopy();
    void createDoesNotPrepareAfterCloseFailure();
    void initialSetupBackupIsRemovedOnFinish();
    void initialSetupBackupIsRestoredOnCancel();
    void initialSetupRestoreWithRecoveryCleanupFailureWarnsWithPath();
    void initialSetupRestoreWithSuccessfulCleanupDoesNotWarn();
    void initialSetupWithoutBackupUpdatesRecentOnlyOnFinish();
    void initialSetupCreateFailureWarnsAfterRestoringOriginal();
    void initialSetupRestoreFailureWarnsWithOriginalBackupPath();
    void initialSetupRestoreAndMoveBackFailureWarnsWithBothRecoveryPaths();
    void createErrorUsesStructuredUtf8Message();
    void successfulStartupLoadPersistsNormalizedPath();
    void failedReplacementOpenPreservesActiveWorkspaceAndUiState();
    void missingStartupPathUsesCapturedWarning();
    void invalidStartupDatabaseUsesLegacyErrorText();
    void startupLoadUsesCoordinatorAndLegacyFallback();
    void startupRejectsOlderSchemasAndPreservesWorkspaceState_data();
    void startupRejectsOlderSchemasAndPreservesWorkspaceState();
    void autosaveUsesCoordinatorForOpenWorkspace();
    void autosaveWarnsAndPreservesStaleCoordinatorState();
    void autosaveUsesLegacyFallbackForCompatibilityWorkspace();
    void saveAsUsesCoordinatorForOpenWorkspace();
    void saveAsWarnsAndPreservesStaleCoordinatorState();
    void saveAsUsesLegacyFallbackForCompatibilityWorkspace();
    void exportUsesCoordinatorForOpenWorkspaceAndPreservesState();
    void preferenceWriteFailureDoesNotFailCreateAndCanRetry();
    void exportWarnsAndPreservesStaleCoordinatorState();
    void exportUsesLegacyFallbackForCompatibilityWorkspace();
    void exportPreservesDialogPolicyAndNoOpenGuard();
    void saveAsMigratesV2AndExportUsesCoordinator();

private:
    QTemporaryDir m_settingsRoot;
};

void FileControllerWorkspaceLifecycleTests::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsRoot.path().toUtf8()
        );
    SettingsManager::instance().clear();
}

void FileControllerWorkspaceLifecycleTests::cleanup()
{
    DialogServices::setFileDialogServiceForTesting(nullptr);
    DialogServices::setUserPromptServiceForTesting(nullptr);
    SettingsManager::instance().clear();
}

void FileControllerWorkspaceLifecycleTests::nullServicesAreSafe()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString missingPath = workspaceRoot.filePath(
        QStringLiteral("missing.tps")
        );

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    FileController controller(nullptr, nullptr);
    controller.loadDatabaseOnStartup(missingPath);

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Missing File")
        );
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("File not found:\n%1")
            .arg(QFileInfo(missingPath).absoluteFilePath())
        );
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::
storedDirectoryFallbackIsUsedByFileDialogs()
{
    QTemporaryDir storedDirectory;
    QVERIFY(storedDirectory.isValid());

    ClassMngr::Next::Platform::
        SettingsManagerLastDatabaseDirectoryPort directoryPort;
    const auto stored = directoryPort.write(
        ClassMngr::Next::Application::DirectoryPreferencePath(
            storedDirectory.path().toUtf8().toStdString()
            )
        );
    QVERIFY(stored);

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(std::nullopt);
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FileController controller(nullptr, nullptr);
    QVERIFY(!controller.createNewDatabaseInteractive());

    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().initialDirectory,
        QFileInfo(storedDirectory.path()).absoluteFilePath()
        );
}

void FileControllerWorkspaceLifecycleTests::
startupPersistsDirectoryThroughTypedPort()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString requestedPath = workspaceRoot.filePath(
        QStringLiteral("persisted-directory-workspace.tps")
        );
    const QString normalizedPath =
        QFileInfo(requestedPath).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(normalizedPath));
    seedServices.closeDatabase();

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(requestedPath);

    ClassMngr::Next::Platform::
        SettingsManagerLastDatabaseDirectoryPort directoryPort;
    const std::string storedDirectory = directoryPort.read().value();
    QCOMPARE(
        QString::fromUtf8(
            storedDirectory.data(),
            static_cast<qsizetype>(storedDirectory.size())
            ),
        QFileInfo(normalizedPath).absoluteDir().absolutePath()
        );
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedPath}
        );
}

void FileControllerWorkspaceLifecycleTests::normalCreateUsesCoordinatorAndUpdatesRecent()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(
            workspaceRoot.filePath(QStringLiteral("new-profile"))
            )
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(controller.createNewDatabaseInteractive());

    const QString expectedPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("new-profile.tps"))
        ).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), expectedPath);
    QVERIFY(QFileInfo::exists(expectedPath));
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{expectedPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), expectedPath);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().title,
        QStringLiteral("New Teacher Profile")
        );
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().defaultSuffix,
        QStringLiteral("tps")
        );
}

void FileControllerWorkspaceLifecycleTests::
recoverInterruptedProfileMatchesTargetNamesContainingNewlines()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetName = QStringLiteral("line\nbreak.tps");
    const QString targetPath = QFileInfo(
        QDir(workspaceRoot.path()).filePath(targetName)
        ).absoluteFilePath();
    const QString recoveryPath = normalCreateRecoveryPath(
        workspaceRoot.path(), targetName);
    const QByteArray recoveryBytes = QByteArrayLiteral("recovery data");
    QVERIFY(writeFile(recoveryPath, recoveryBytes));

    ClassMngr::Next::Platform::SettingsManagerLastDatabaseDirectoryPort
        directoryPort;
    QVERIFY(directoryPort.write(
        ClassMngr::Next::Application::DirectoryPreferencePath(
            workspaceRoot.path().toUtf8().toStdString()
            )
        ));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(std::nullopt);
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);
    FakeUserPromptService prompts;
    prompts.scriptedActionIds.enqueue(QStringLiteral("candidate-0"));
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(
        &services,
        nullptr,
        nullptr,
        &directoryPort
        );
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.recoverInterruptedProfile->trigger();

    QCOMPARE(prompts.actionPrompts.size(), 1);
    QCOMPARE(prompts.actionPrompts.constFirst().actions.size(), 2);
    QVERIFY(
        prompts.actionPrompts.constFirst().informativeText
            .contains(targetPath)
        );
    QVERIFY(
        prompts.actionPrompts.constFirst().informativeText
            .contains(recoveryPath)
        );
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QVERIFY(!QFileInfo::exists(targetPath));
    QCOMPARE(readFile(recoveryPath), recoveryBytes);
}

void FileControllerWorkspaceLifecycleTests::
recoverInterruptedProfileCopiesToAbsentTargetAndIsIdempotent()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString seedDirectory = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("seed")
        );
    QVERIFY(QDir().mkpath(seedDirectory));
    const QString seedPath = QDir(seedDirectory).filePath(
        QStringLiteral("seed.tps")
        );
    QVERIFY(createSchemaProfile(seedPath));
    QVERIFY(writeProfileMarker(seedPath, QStringLiteral("original")));

    const QString targetPath = QFileInfo(
        QDir(workspaceRoot.path()).filePath(QStringLiteral("restored.tps"))
        ).absoluteFilePath();
    const QString recoveryPath = normalCreateRecoveryPath(
        workspaceRoot.path(),
        QFileInfo(targetPath).fileName()
        );
    QVERIFY(QFile::copy(seedPath, recoveryPath));
    const QByteArray recoveryBytes = readFile(recoveryPath);
    QVERIFY(!QFileInfo::exists(targetPath));

    ClassMngr::Next::Platform::SettingsManagerLastDatabaseDirectoryPort
        directoryPort;
    QVERIFY(directoryPort.write(
        ClassMngr::Next::Application::DirectoryPreferencePath(
            workspaceRoot.path().toUtf8().toStdString()
            )
        ));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    fileDialogs.scriptedSaveFiles.enqueue(std::nullopt);
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    prompts.scriptedActionIds.enqueue(QStringLiteral("candidate-0"));
    prompts.scriptedActionIds.enqueue(QStringLiteral("candidate-0"));
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(
        &services,
        nullptr,
        nullptr,
        &directoryPort
        );
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.recoverInterruptedProfile->trigger();

    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), targetPath);
    QCOMPARE(readFile(targetPath), recoveryBytes);
    QCOMPARE(readFile(recoveryPath), recoveryBytes);
    QCOMPARE(prompts.actionPrompts.size(), 1);
    QCOMPARE(prompts.actionPrompts.constFirst().actions.size(), 2);
    QVERIFY(
        prompts.actionPrompts.constFirst().informativeText
            .contains(targetPath)
        );
    QVERIFY(
        prompts.actionPrompts.constFirst().informativeText
            .contains(recoveryPath)
        );
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().title,
        QStringLiteral("Save Recovered Teacher Profile")
        );

    // Discovering and retrying the same recovery remains safe: the existing
    // recovered target is refused, and canceling the retry changes nothing.
    actions.recoverInterruptedProfile->trigger();
    QCOMPARE(prompts.actionPrompts.size(), 2);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 3);
    QCOMPARE(readFile(targetPath), recoveryBytes);
    QCOMPARE(readFile(recoveryPath), recoveryBytes);
    QCOMPARE(prompts.messages.size(), 1);
    QVERIFY(
        prompts.messages.constFirst().message
            .contains(QStringLiteral("destination already exists"),
                      Qt::CaseInsensitive)
        );
    QCOMPARE(services.currentDatabasePath(), targetPath);
}

void FileControllerWorkspaceLifecycleTests::
recoverInterruptedProfileRequiresExplicitChoiceAndIgnoresLookalikes()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString seedDirectory = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("seed")
        );
    QVERIFY(QDir().mkpath(seedDirectory));
    const QString seedA = QDir(seedDirectory).filePath(
        QStringLiteral("seed-a.tps")
        );
    const QString seedB = QDir(seedDirectory).filePath(
        QStringLiteral("seed-b.tps")
        );
    QVERIFY(createSchemaProfile(seedA));
    QVERIFY(createSchemaProfile(seedB));
    QVERIFY(writeProfileMarker(seedA, QStringLiteral("candidate-a")));
    QVERIFY(writeProfileMarker(seedB, QStringLiteral("candidate-b")));

    const QString targetA = QFileInfo(
        QDir(workspaceRoot.path()).filePath(QStringLiteral("a-profile.tps"))
        ).absoluteFilePath();
    const QString targetB = QFileInfo(
        QDir(workspaceRoot.path()).filePath(QStringLiteral("b-profile.tps"))
        ).absoluteFilePath();
    const QString recoveryA = normalCreateRecoveryPath(
        workspaceRoot.path(), QFileInfo(targetA).fileName());
    const QString recoveryB = normalCreateRecoveryPath(
        workspaceRoot.path(), QFileInfo(targetB).fileName());
    QVERIFY(QFile::copy(seedA, recoveryA));
    QVERIFY(QFile::copy(seedB, recoveryB));

    const QByteArray existingTarget = QByteArrayLiteral(
        "existing target must never be replaced"
        );
    QVERIFY(writeFile(targetB, existingTarget));

    const QString invalidUuidLookalike = QDir(workspaceRoot.path()).filePath(
        QStringLiteral(".ignored.tps.workspace-create-not-a-uuid.recovery")
        );
    const QString invalidInitialSetupUuidLookalike =
        QDir(workspaceRoot.path()).filePath(
            QStringLiteral(
                ".ignored.tps.initial-setup-not-a-uuid.backup"
                )
        );
    const QString ordinaryRecoveryLookalike = QDir(workspaceRoot.path()).filePath(
        QStringLiteral(".notes.recovery")
        );
    const QString nonHiddenLookalike = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("nonhidden.tps.workspace-create-%1.recovery")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
        );
    QVERIFY(writeFile(invalidUuidLookalike, QByteArrayLiteral("ignore")));
    QVERIFY(writeFile(
        invalidInitialSetupUuidLookalike,
        QByteArrayLiteral("ignore")
        ));
    QVERIFY(writeFile(ordinaryRecoveryLookalike, QByteArrayLiteral("ignore")));
    QVERIFY(writeFile(nonHiddenLookalike, QByteArrayLiteral("ignore")));

    const QString alternatePath = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("recovered-b.tps")
        );
    ClassMngr::Next::Platform::SettingsManagerLastDatabaseDirectoryPort
        directoryPort;
    QVERIFY(directoryPort.write(
        ClassMngr::Next::Application::DirectoryPreferencePath(
            workspaceRoot.path().toUtf8().toStdString()
            )
        ));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetB)
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(alternatePath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    prompts.scriptedActionIds.enqueue(QStringLiteral("candidate-1"));
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(
        &services,
        nullptr,
        nullptr,
        &directoryPort
        );
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.recoverInterruptedProfile->trigger();

    QCOMPARE(prompts.actionPrompts.size(), 1);
    const ActionPromptRequest& request = prompts.actionPrompts.constFirst();
    QCOMPARE(request.actions.size(), 3);
    QCOMPARE(request.actions.at(0).id, QStringLiteral("candidate-0"));
    QCOMPARE(request.actions.at(1).id, QStringLiteral("candidate-1"));
    QVERIFY(request.informativeText.contains(targetA));
    QVERIFY(request.informativeText.contains(targetB));
    QVERIFY(request.informativeText.contains(recoveryA));
    QVERIFY(request.informativeText.contains(recoveryB));
    QVERIFY(!request.informativeText.contains(QStringLiteral("not-a-uuid")));
    QVERIFY(!request.informativeText.contains(QStringLiteral("initial-setup")));
    QVERIFY(!request.informativeText.contains(QStringLiteral(".notes.recovery")));
    QVERIFY(!request.informativeText.contains(QStringLiteral("nonhidden")));

    QCOMPARE(fileDialogs.saveFileRequests.size(), 2);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().suggestedFileName,
        QStringLiteral("Recovered b-profile.tps")
        );
    QVERIFY(!fileDialogs.saveFileRequests.constFirst().confirmOverwrite);
    QCOMPARE(services.currentDatabasePath(), QFileInfo(alternatePath).absoluteFilePath());
    QCOMPARE(readFile(targetB), existingTarget);
    QCOMPARE(readFile(alternatePath), readFile(recoveryB));
    QVERIFY(QFileInfo::exists(recoveryA));
    QVERIFY(QFileInfo::exists(recoveryB));
    QVERIFY(!QFileInfo::exists(targetA));
    QVERIFY(prompts.messages.size() >= 1);
    QVERIFY(
        prompts.messages.constFirst().message
            .contains(QStringLiteral("destination already exists"),
                      Qt::CaseInsensitive)
        );
}

void FileControllerWorkspaceLifecycleTests::
recoverInterruptedProfileCancellationAndCopyFailurePreserveFiles()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString seedDirectory = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("seed")
        );
    QVERIFY(QDir().mkpath(seedDirectory));
    const QString seedPath = QDir(seedDirectory).filePath(
        QStringLiteral("seed.tps")
        );
    QVERIFY(createSchemaProfile(seedPath));
    QVERIFY(writeProfileMarker(seedPath, QStringLiteral("source")));

    const QString targetPath = QFileInfo(
        QDir(workspaceRoot.path()).filePath(QStringLiteral("target.tps"))
        ).absoluteFilePath();
    const QByteArray targetBytes = QByteArrayLiteral("target must survive");
    QVERIFY(writeFile(targetPath, targetBytes));
    const QString recoveryPath = normalCreateRecoveryPath(
        workspaceRoot.path(), QFileInfo(targetPath).fileName());
    QVERIFY(QFile::copy(seedPath, recoveryPath));
    const QByteArray recoveryBytes = readFile(recoveryPath);

    const QString blockingFile = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("not-a-directory")
        );
    QVERIFY(writeFile(blockingFile, QByteArrayLiteral("blocker")));
    const QString failedDestination = QDir(blockingFile).filePath(
        QStringLiteral("recovered.tps")
        );
    const QString invalidProfileDestination = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("copied-but-unopenable.tps")
        );

    ClassMngr::Next::Platform::SettingsManagerLastDatabaseDirectoryPort
        directoryPort;
    QVERIFY(directoryPort.write(
        ClassMngr::Next::Application::DirectoryPreferencePath(
            workspaceRoot.path().toUtf8().toStdString()
            )
        ));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(std::nullopt);
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(failedDestination)
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(invalidProfileDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    prompts.scriptedActionIds.enqueue(QStringLiteral("cancel"));
    prompts.scriptedActionIds.enqueue(QStringLiteral("candidate-0"));
    prompts.scriptedActionIds.enqueue(QStringLiteral("candidate-0"));
    prompts.scriptedActionIds.enqueue(QStringLiteral("candidate-0"));
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(
        &services,
        nullptr,
        nullptr,
        &directoryPort
        );
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.recoverInterruptedProfile->trigger();
    QVERIFY(fileDialogs.saveFileRequests.isEmpty());
    QCOMPARE(readFile(targetPath), targetBytes);
    QCOMPARE(readFile(recoveryPath), recoveryBytes);

    actions.recoverInterruptedProfile->trigger();
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(readFile(targetPath), targetBytes);
    QCOMPARE(readFile(recoveryPath), recoveryBytes);
    QVERIFY(!QFileInfo::exists(failedDestination));

    actions.recoverInterruptedProfile->trigger();
    QCOMPARE(fileDialogs.saveFileRequests.size(), 2);
    QVERIFY(!QFileInfo::exists(failedDestination));
    QCOMPARE(readFile(targetPath), targetBytes);
    QCOMPARE(readFile(recoveryPath), recoveryBytes);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Error);
    QVERIFY(prompts.messages.constFirst().message.contains(recoveryPath));
    QVERIFY(prompts.messages.constFirst().message.contains(failedDestination));
    QVERIFY(!services.hasOpenDatabase());

    // A copied file that cannot be opened stays beside the original recovery
    // source for manual inspection; it is never removed by the failed open.
    const QByteArray invalidRecoveryBytes = QByteArrayLiteral(
        "not a Teacher Profile"
        );
    QVERIFY(writeFile(recoveryPath, invalidRecoveryBytes));
    actions.recoverInterruptedProfile->trigger();

    QCOMPARE(fileDialogs.saveFileRequests.size(), 3);
    QVERIFY(QFileInfo::exists(invalidProfileDestination));
    QCOMPARE(readFile(invalidProfileDestination), invalidRecoveryBytes);
    QCOMPARE(readFile(recoveryPath), invalidRecoveryBytes);
    QCOMPARE(readFile(targetPath), targetBytes);
    QCOMPARE(prompts.messages.size(), 2);
    QVERIFY(
        prompts.messages.constLast().message
            .contains(QStringLiteral("could not be opened"), Qt::CaseInsensitive)
        );
    QVERIFY(
        prompts.messages.constLast().message.contains(
            invalidProfileDestination
            )
        );
    QVERIFY(prompts.messages.constLast().message.contains(recoveryPath));
    QVERIFY(!services.hasOpenDatabase());
}

void FileControllerWorkspaceLifecycleTests::
recoverInterruptedProfileReportsWhenNoCandidatesExist()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    ClassMngr::Next::Platform::SettingsManagerLastDatabaseDirectoryPort
        directoryPort;
    QVERIFY(directoryPort.write(
        ClassMngr::Next::Application::DirectoryPreferencePath(
            workspaceRoot.path().toUtf8().toStdString()
            )
        ));

    FakeFileDialogService fileDialogs;
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(
        &services,
        nullptr,
        nullptr,
        &directoryPort
        );
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.recoverInterruptedProfile->trigger();

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Information);
    QVERIFY(
        prompts.messages.constFirst().message
            .contains(QStringLiteral("No interrupted profile recovery copies"))
        );
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(fileDialogs.saveFileRequests.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::
recoverInterruptedInitialSetupBackupCopiesAndCanBeRediscovered()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString seedDirectory = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("seed")
        );
    QVERIFY(QDir().mkpath(seedDirectory));
    const QString seedPath = QDir(seedDirectory).filePath(
        QStringLiteral("seed.tps")
        );
    QVERIFY(createSchemaProfile(seedPath));
    QVERIFY(writeProfileMarker(seedPath, QStringLiteral("initial setup")));

    const QString targetName = QStringLiteral("restart\nprofile.tps");
    const QString targetPath = QFileInfo(
        QDir(workspaceRoot.path()).filePath(targetName)
        ).absoluteFilePath();
    const QString recoveryPath = initialSetupRecoveryPath(
        workspaceRoot.path(),
        QFileInfo(targetPath).fileName()
        );
    QVERIFY(QFile::copy(seedPath, recoveryPath));
    const QByteArray recoveryBytes = readFile(recoveryPath);
    QVERIFY(!QFileInfo::exists(targetPath));

    ClassMngr::Next::Platform::SettingsManagerLastDatabaseDirectoryPort
        directoryPort;
    QVERIFY(directoryPort.write(
        ClassMngr::Next::Application::DirectoryPreferencePath(
            workspaceRoot.path().toUtf8().toStdString()
            )
        ));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(std::nullopt);
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    prompts.scriptedActionIds.enqueue(QStringLiteral("candidate-0"));
    prompts.scriptedActionIds.enqueue(QStringLiteral("candidate-0"));
    prompts.scriptedActionIds.enqueue(QStringLiteral("cancel"));
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(
        &services,
        nullptr,
        nullptr,
        &directoryPort
        );
    ActionRegistry actions;
    connectFileActions(controller, actions);

    // Canceling the destination picker leaves the discovered Initial Setup
    // backup and its inferred target untouched.
    actions.recoverInterruptedProfile->trigger();
    QCOMPARE(prompts.actionPrompts.size(), 1);
    QCOMPARE(prompts.actionPrompts.constFirst().actions.size(), 2);
    QVERIFY(
        prompts.actionPrompts.constFirst().informativeText
            .contains(targetPath)
        );
    QVERIFY(
        prompts.actionPrompts.constFirst().informativeText
            .contains(recoveryPath)
        );
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QVERIFY(!QFileInfo::exists(targetPath));
    QCOMPARE(readFile(recoveryPath), recoveryBytes);
    QVERIFY(!services.hasOpenDatabase());

    // A later explicit choice copies the backup to the inferred .tps target.
    actions.recoverInterruptedProfile->trigger();
    QCOMPARE(prompts.actionPrompts.size(), 2);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), targetPath);
    QCOMPARE(readFile(targetPath), recoveryBytes);
    QCOMPARE(readFile(recoveryPath), recoveryBytes);

    // The source remains available for another deliberate recovery attempt.
    actions.recoverInterruptedProfile->trigger();
    QCOMPARE(prompts.actionPrompts.size(), 3);
    QCOMPARE(prompts.actionPrompts.constLast().actions.size(), 2);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 2);
    QCOMPARE(readFile(targetPath), recoveryBytes);
    QCOMPARE(readFile(recoveryPath), recoveryBytes);
    QCOMPARE(services.currentDatabasePath(), targetPath);
}

void FileControllerWorkspaceLifecycleTests::
recoverInterruptedInitialSetupBackupPreservesPartialTarget()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString seedDirectory = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("seed")
        );
    QVERIFY(QDir().mkpath(seedDirectory));
    const QString seedPath = QDir(seedDirectory).filePath(
        QStringLiteral("seed.tps")
        );
    QVERIFY(createSchemaProfile(seedPath));
    QVERIFY(writeProfileMarker(seedPath, QStringLiteral("preserved backup")));

    const QString targetPath = QFileInfo(
        QDir(workspaceRoot.path()).filePath(QStringLiteral("partial.tps"))
        ).absoluteFilePath();
    const QByteArray partialTargetBytes = QByteArrayLiteral(
        "partial Initial Setup target"
        );
    QVERIFY(writeFile(targetPath, partialTargetBytes));
    const QString recoveryPath = initialSetupRecoveryPath(
        workspaceRoot.path(),
        QFileInfo(targetPath).fileName()
        );
    QVERIFY(QFile::copy(seedPath, recoveryPath));
    const QByteArray recoveryBytes = readFile(recoveryPath);
    const QString alternatePath = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("recovered-partial.tps")
        );

    ClassMngr::Next::Platform::SettingsManagerLastDatabaseDirectoryPort
        directoryPort;
    QVERIFY(directoryPort.write(
        ClassMngr::Next::Application::DirectoryPreferencePath(
            workspaceRoot.path().toUtf8().toStdString()
            )
        ));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(alternatePath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    prompts.scriptedActionIds.enqueue(QStringLiteral("candidate-0"));
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(
        &services,
        nullptr,
        nullptr,
        &directoryPort
        );
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.recoverInterruptedProfile->trigger();

    QCOMPARE(prompts.actionPrompts.size(), 1);
    QVERIFY(prompts.actionPrompts.constFirst().informativeText.contains(targetPath));
    QCOMPARE(fileDialogs.saveFileRequests.size(), 2);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().suggestedFileName,
        QStringLiteral("Recovered partial.tps")
        );
    QVERIFY(!fileDialogs.saveFileRequests.constFirst().confirmOverwrite);
    QVERIFY(
        QFileInfo(alternatePath).isAbsolute()
        && services.currentDatabasePath()
               == QFileInfo(alternatePath).absoluteFilePath()
        );
    QCOMPARE(readFile(targetPath), partialTargetBytes);
    QCOMPARE(readFile(alternatePath), recoveryBytes);
    QCOMPARE(readFile(recoveryPath), recoveryBytes);
    QVERIFY(prompts.messages.size() >= 1);
    QVERIFY(
        prompts.messages.constFirst().message.contains(
            QStringLiteral("destination already exists"),
            Qt::CaseInsensitive
            )
        );
}

void FileControllerWorkspaceLifecycleTests::
recoverInterruptedInitialSetupCandidatesRequireExplicitChoiceAndIgnoreLookalikes()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString seedDirectory = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("seed")
        );
    QVERIFY(QDir().mkpath(seedDirectory));
    const QString seedA = QDir(seedDirectory).filePath(
        QStringLiteral("seed-a.tps")
        );
    const QString seedB = QDir(seedDirectory).filePath(
        QStringLiteral("seed-b.tps")
        );
    QVERIFY(createSchemaProfile(seedA));
    QVERIFY(createSchemaProfile(seedB));
    QVERIFY(writeProfileMarker(seedA, QStringLiteral("candidate-a")));
    QVERIFY(writeProfileMarker(seedB, QStringLiteral("candidate-b")));

    const QString targetA = QFileInfo(
        QDir(workspaceRoot.path()).filePath(QStringLiteral("a-profile.tps"))
        ).absoluteFilePath();
    const QString targetB = QFileInfo(
        QDir(workspaceRoot.path()).filePath(QStringLiteral("b-profile.tps"))
        ).absoluteFilePath();
    const QString recoveryA = initialSetupRecoveryPath(
        workspaceRoot.path(),
        QFileInfo(targetA).fileName()
        );
    const QString recoveryB = initialSetupRecoveryPath(
        workspaceRoot.path(),
        QFileInfo(targetB).fileName()
        );
    QVERIFY(QFile::copy(seedA, recoveryA));
    QVERIFY(QFile::copy(seedB, recoveryB));
    const QByteArray recoveryBytesA = readFile(recoveryA);
    const QByteArray recoveryBytesB = readFile(recoveryB);

    const QString malformedUuid = QDir(workspaceRoot.path()).filePath(
        QStringLiteral(".bad.tps.initial-setup-not-a-uuid.backup")
        );
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString extraSuffix = QDir(workspaceRoot.path()).filePath(
        QStringLiteral(".extra.tps.initial-setup-%1.backup.extra")
            .arg(uuid)
        );
    const QString visibleLookalike = QDir(workspaceRoot.path()).filePath(
        QStringLiteral("visible.tps.initial-setup-%1.backup").arg(uuid)
        );
    QVERIFY(writeFile(malformedUuid, QByteArrayLiteral("ignore")));
    QVERIFY(writeFile(extraSuffix, QByteArrayLiteral("ignore")));
    QVERIFY(writeFile(visibleLookalike, QByteArrayLiteral("ignore")));

    ClassMngr::Next::Platform::SettingsManagerLastDatabaseDirectoryPort
        directoryPort;
    QVERIFY(directoryPort.write(
        ClassMngr::Next::Application::DirectoryPreferencePath(
            workspaceRoot.path().toUtf8().toStdString()
            )
        ));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetB)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    prompts.scriptedActionIds.enqueue(QStringLiteral("candidate-1"));
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(
        &services,
        nullptr,
        nullptr,
        &directoryPort
        );
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.recoverInterruptedProfile->trigger();

    QCOMPARE(prompts.actionPrompts.size(), 1);
    const ActionPromptRequest& request = prompts.actionPrompts.constFirst();
    QCOMPARE(request.actions.size(), 3);
    QCOMPARE(request.actions.at(0).id, QStringLiteral("candidate-0"));
    QCOMPARE(request.actions.at(1).id, QStringLiteral("candidate-1"));
    QVERIFY(request.informativeText.contains(targetA));
    QVERIFY(request.informativeText.contains(targetB));
    QVERIFY(request.informativeText.contains(recoveryA));
    QVERIFY(request.informativeText.contains(recoveryB));
    QVERIFY(!request.informativeText.contains(QStringLiteral("not-a-uuid")));
    QVERIFY(!request.informativeText.contains(QStringLiteral("backup.extra")));
    QVERIFY(!request.informativeText.contains(QStringLiteral("visible.tps")));

    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), targetB);
    QCOMPARE(readFile(targetB), recoveryBytesB);
    QCOMPARE(readFile(recoveryA), recoveryBytesA);
    QCOMPARE(readFile(recoveryB), recoveryBytesB);
    QVERIFY(!QFileInfo::exists(targetA));
}

void FileControllerWorkspaceLifecycleTests::
normalCreateUsesUnicodeSessionLocationForCurrentFile()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString unicodeDirectory = QDir(workspaceRoot.path()).filePath(
        QString::fromUtf8("\xED\x95\x99\xEA\xB5\x90")
        );
    const QString rawPath = QDir(unicodeDirectory).filePath(
        QString::fromUtf8("\xEA\xB5\x90\xEC\x82\xAC")
        );
    const QString normalizedPath = QFileInfo(
        rawPath + QStringLiteral(".tps")
        ).absoluteFilePath();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(rawPath)
        );
    fileDialogs.scriptedSaveFiles.enqueue(std::nullopt);
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    QVERIFY(controller.createNewDatabaseInteractive());
    QCOMPARE(services.currentDatabasePath(), normalizedPath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedPath}
        );
    QVERIFY(actions.saveFile->isEnabled());
    actions.saveFile->trigger();
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);

    SettingsManager::instance().setLastDatabaseDirectory(
        workspaceRoot.path()
        );
    QVERIFY(!controller.createNewDatabaseInteractive());

    QCOMPARE(fileDialogs.saveFileRequests.size(), 2);
    QCOMPARE(
        fileDialogs.saveFileRequests.constLast().initialDirectory,
        QFileInfo(normalizedPath).absolutePath()
        );
}

void FileControllerWorkspaceLifecycleTests::
normalOpenUsesUnicodeSessionLocationAndPreservesHistory()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString unicodeDirectory = QDir(workspaceRoot.path()).filePath(
        QString::fromUtf8("\xED\x95\x99\xEA\xB5\x90")
        );
    QVERIFY(QDir().mkpath(unicodeDirectory));
    const QString rawPath = QDir(unicodeDirectory).filePath(
        QString::fromUtf8("\xEA\xB5\x90\xEC\x82\xAC")
        );
    const QString normalizedPath = QFileInfo(
        rawPath + QStringLiteral(".tps")
        ).absoluteFilePath();
    const QString otherPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("other-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(normalizedPath));
    seedServices.closeDatabase();

    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({rawPath, normalizedPath, otherPath});
    settings.setLastFile(rawPath);

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(std::nullopt);
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);
    controller.loadDatabaseOnStartup(rawPath);

    QCOMPARE(services.currentDatabasePath(), normalizedPath);
    QCOMPARE(
        settings.getRecentFiles(),
        (QStringList{normalizedPath, otherPath})
        );
    QCOMPARE(settings.getLastFile(), normalizedPath);
    QVERIFY(actions.saveFile->isEnabled());
    actions.saveFile->trigger();
    QVERIFY(fileDialogs.saveFileRequests.isEmpty());

    settings.setLastDatabaseDirectory(workspaceRoot.path());
    QVERIFY(!controller.createNewDatabaseInteractive());

    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().initialDirectory,
        QFileInfo(normalizedPath).absolutePath()
        );
}

void FileControllerWorkspaceLifecycleTests::
recentFilesDeduplicateRawAndNormalizedPaths()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString rawPath = workspaceRoot.filePath(
        QStringLiteral("deduplicated-workspace")
        );
    const QString normalizedPath = QFileInfo(
        rawPath + QStringLiteral(".tps")
        ).absoluteFilePath();
    const QString otherPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("other-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(normalizedPath));
    seedServices.closeDatabase();

    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({rawPath, normalizedPath, otherPath});
    settings.setLastFile(rawPath);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(rawPath);

    QCOMPARE(
        settings.getRecentFiles(),
        (QStringList{normalizedPath, otherPath})
        );
    QCOMPARE(settings.getLastFile(), normalizedPath);
}

void FileControllerWorkspaceLifecycleTests::
recentActionReopensActiveWorkspaceAndMovesItToMruFront()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString activePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("active-workspace.tps"))
        ).absoluteFilePath();
    const QString otherPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("other-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(otherPath));
    seedServices.closeDatabase();
    QVERIFY(seedServices.openDatabase(activePath));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(activePath);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), activePath);

    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({otherPath, activePath});
    settings.setLastFile(otherPath);

    ActionRegistry actions;
    QMenu recentFilesMenu;
    actions.recentFilesMenu = &recentFilesMenu;
    connectFileActions(controller, actions);
    controller.populateRecentMenu();

    QVERIFY(recentFilesMenu.actions().size() >= 2);
    QCOMPARE(
        recentFilesMenu.actions().at(0)->data().toString(),
        otherPath
        );
    QCOMPARE(
        recentFilesMenu.actions().at(1)->data().toString(),
        activePath
        );

    QAction* const activeRecentAction = recentFilesMenu.actions().at(1);
    QVERIFY(activeRecentAction);
    activeRecentAction->trigger();

    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), activePath);
    QCOMPARE(
        settings.getRecentFiles(),
        (QStringList{activePath, otherPath})
        );
    QCOMPARE(settings.getLastFile(), activePath);
    QVERIFY(recentFilesMenu.actions().size() >= 2);
    QCOMPARE(
        recentFilesMenu.actions().at(0)->data().toString(),
        activePath
        );
    QCOMPARE(
        recentFilesMenu.actions().at(1)->data().toString(),
        otherPath
        );
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.messages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::
recentFilesKeepNewestFirstAndCapAtTen()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    ApplicationServices seedServices;
    ApplicationServices services;
    FileController controller(&services, nullptr);
    QStringList expected;
    for (int index = 0; index < 12; ++index)
    {
        const QString rawPath = workspaceRoot.filePath(
            QStringLiteral("workspace-%1").arg(index)
            );
        const QString normalizedPath = QFileInfo(
            rawPath + QStringLiteral(".tps")
            ).absoluteFilePath();
        QVERIFY(seedServices.openDatabase(normalizedPath));
        seedServices.closeDatabase();
        controller.loadDatabaseOnStartup(rawPath);

        expected.prepend(normalizedPath);
        while (expected.size() > 10)
        {
            expected.removeLast();
        }
    }

    QCOMPARE(SettingsManager::instance().getRecentFiles(), expected);
    QCOMPARE(
        SettingsManager::instance().getLastFile(),
        expected.constFirst()
        );
}

void FileControllerWorkspaceLifecycleTests::
pruningRemovesMissingPathAndClearsLastFile()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString rawPath = workspaceRoot.filePath(
        QStringLiteral("missing-workspace")
        );
    const QString normalizedPath = QFileInfo(
        rawPath + QStringLiteral(".tps")
        ).absoluteFilePath();
    const QString otherPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("retained-workspace.tps"))
        ).absoluteFilePath();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({rawPath, normalizedPath, otherPath});
    settings.setLastFile(rawPath);

    FileController controller(nullptr, nullptr);
    controller.loadDatabaseOnStartup(rawPath);

    QCOMPARE(settings.getRecentFiles(), QStringList{otherPath});
    QVERIFY(settings.getLastFile().isEmpty());
}

void FileControllerWorkspaceLifecycleTests::
clearRecentFilesClearsListAndLastFile()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString directory = QStringLiteral("/keep-this-directory");
    settings.setLastDatabaseDirectory(directory);
    settings.setRecentFiles({QStringLiteral("recent.tps")});
    settings.setLastFile(QStringLiteral("recent.tps"));

    FileController controller(nullptr, nullptr);
    ActionRegistry actions;
    QMenu recentFilesMenu;
    actions.recentFilesMenu = &recentFilesMenu;
    connectFileActions(controller, actions);
    controller.populateRecentMenu();
    QVERIFY(!actions.recentFilesMenu->actions().isEmpty());
    actions.recentFilesMenu->actions().constLast()->trigger();

    QVERIFY(settings.getRecentFiles().isEmpty());
    QVERIFY(settings.getLastFile().isEmpty());
    QCOMPARE(settings.getLastDatabaseDirectory(), directory);
}

void FileControllerWorkspaceLifecycleTests::
successfulOpenSurvivesRecentHistoryPersistenceFailure()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString requestedPath = workspaceRoot.filePath(
        QStringLiteral("opened-after-history-failure.tps")
        );
    const QString normalizedPath = QFileInfo(requestedPath).absoluteFilePath();
    const QString previousPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("previous-workspace.tps"))
        ).absoluteFilePath();
    const QString olderPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("older-workspace.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(normalizedPath));
    seedServices.closeDatabase();

    SettingsManager& settings = SettingsManager::instance();
    const QStringList previousHistory{previousPath, olderPath};
    settings.setRecentFiles(previousHistory);
    settings.setLastFile(previousPath);
    settings.sync();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    int synchronizationAttempts = 0;
    ClassMngr::Next::Platform::SettingsManagerRecentWorkspaceHistoryPort
        historyPort(
            [&synchronizationAttempts](QSettings& target)
            {
                ++synchronizationAttempts;
                return syncAndReturnAccessError(target);
            }
            );
    ApplicationServices services;
    FileController controller(
        &services,
        nullptr,
        nullptr,
        nullptr,
        &historyPort
        );
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(requestedPath);

    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedPath);
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());
    QCOMPARE(settings.getRecentFiles(), previousHistory);
    QCOMPARE(settings.getLastFile(), previousPath);
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Recent Files"));
    QCOMPARE(
        static_cast<int>(warning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(warning.message.contains(QStringLiteral("history")));
    QVERIFY(warning.details.contains(QStringLiteral("AccessError")));
}

void FileControllerWorkspaceLifecycleTests::
failedRecentHistoryClearWarnsAndPreservesMenu()
{
    const QString firstPath = QStringLiteral("first-recent.tps");
    const QString secondPath = QStringLiteral("second-recent.tps");
    SettingsManager& settings = SettingsManager::instance();
    const QStringList priorHistory{firstPath, secondPath};
    settings.setRecentFiles(priorHistory);
    settings.setLastFile(firstPath);
    settings.sync();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    int synchronizationAttempts = 0;
    ClassMngr::Next::Platform::SettingsManagerRecentWorkspaceHistoryPort
        historyPort(
            [&synchronizationAttempts](QSettings& target)
            {
                ++synchronizationAttempts;
                return syncAndReturnAccessError(target);
            }
            );
    FileController controller(
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &historyPort
        );
    ActionRegistry actions;
    QMenu recentFilesMenu;
    actions.recentFilesMenu = &recentFilesMenu;
    connectFileActions(controller, actions);
    controller.populateRecentMenu();

    QCOMPARE(recentFilesMenu.actions().size(), 4);
    recentFilesMenu.actions().constLast()->trigger();

    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.getRecentFiles(), priorHistory);
    QCOMPARE(settings.getLastFile(), firstPath);
    QCOMPARE(recentFilesMenu.actions().size(), 4);
    QCOMPARE(recentFilesMenu.actions().at(0)->data().toString(), firstPath);
    QCOMPARE(recentFilesMenu.actions().at(1)->data().toString(), secondPath);
    QVERIFY(recentFilesMenu.actions().at(2)->isSeparator());
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Recent Files"));
    QCOMPARE(
        static_cast<int>(warning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(warning.message.contains(QStringLiteral("cleared")));
    QVERIFY(warning.details.contains(QStringLiteral("AccessError")));
}

void FileControllerWorkspaceLifecycleTests::
failedRecentHistoryPruneWarnsAndPreservesHistory()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString rawMissingPath = workspaceRoot.filePath(
        QStringLiteral("missing-workspace")
        );
    const QString normalizedMissingPath = QFileInfo(
        rawMissingPath + QStringLiteral(".tps")
        ).absoluteFilePath();
    const QString retainedPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("retained-workspace.tps"))
        ).absoluteFilePath();
    QVERIFY(!QFileInfo::exists(normalizedMissingPath));

    SettingsManager& settings = SettingsManager::instance();
    const QStringList priorHistory{
        rawMissingPath,
        normalizedMissingPath,
        retainedPath
    };
    settings.setRecentFiles(priorHistory);
    settings.setLastFile(rawMissingPath);
    settings.sync();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    int synchronizationAttempts = 0;
    ClassMngr::Next::Platform::SettingsManagerRecentWorkspaceHistoryPort
        historyPort(
            [&synchronizationAttempts](QSettings& target)
            {
                ++synchronizationAttempts;
                return syncAndReturnAccessError(target);
            }
            );
    FileController controller(
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &historyPort
        );
    ActionRegistry actions;
    QMenu recentFilesMenu;
    actions.recentFilesMenu = &recentFilesMenu;
    connectFileActions(controller, actions);

    controller.loadMostRecentDatabase();

    QVERIFY(!QFileInfo::exists(normalizedMissingPath));
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.getRecentFiles(), priorHistory);
    QCOMPARE(settings.getLastFile(), rawMissingPath);
    QCOMPARE(recentFilesMenu.actions().size(), 5);
    QCOMPARE(
        recentFilesMenu.actions().at(0)->data().toString(),
        rawMissingPath
        );
    QCOMPARE(
        recentFilesMenu.actions().at(1)->data().toString(),
        normalizedMissingPath
        );
    QCOMPARE(
        recentFilesMenu.actions().at(2)->data().toString(),
        retainedPath
        );
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Recent Files"));
    QCOMPARE(
        static_cast<int>(warning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(warning.message.contains(QStringLiteral("updated")));
    QVERIFY(warning.details.contains(QStringLiteral("AccessError")));
}

void FileControllerWorkspaceLifecycleTests::recentFilesPreserveUnicodePaths()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString rawPath = workspaceRoot.filePath(
        QString::fromUtf8(
            "\xED\x95\x99\xEA\xB5\x90-\xF0\x9F\x93\x9A"
            )
        );
    const QString normalizedPath = QFileInfo(
        rawPath + QStringLiteral(".tps")
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(normalizedPath));
    seedServices.closeDatabase();

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(rawPath);

    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedPath);
}

void FileControllerWorkspaceLifecycleTests::
startupUsesListFirstAndLastFileFallback()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString firstPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("list-first.tps"))
        ).absoluteFilePath();
    const QString fallbackPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("last-file-fallback.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(firstPath));
    seedServices.closeDatabase();
    QVERIFY(seedServices.openDatabase(fallbackPath));
    seedServices.closeDatabase();

    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({firstPath});
    settings.setLastFile(fallbackPath);

    {
        ApplicationServices services;
        FileController controller(&services, nullptr);
        controller.loadMostRecentDatabase();
        QVERIFY(services.hasOpenDatabase());
        QCOMPARE(services.currentDatabasePath(), firstPath);
    }

    settings.clearRecentFiles();
    settings.setLastFile(fallbackPath);

    ApplicationServices fallbackServices;
    FileController fallbackController(&fallbackServices, nullptr);
    fallbackController.loadMostRecentDatabase();
    QVERIFY(fallbackServices.hasOpenDatabase());
    QCOMPARE(fallbackServices.currentDatabasePath(), fallbackPath);
}

void FileControllerWorkspaceLifecycleTests::normalCreateReplacesExistingTarget()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("existing-target.tps")
        );
    QVERIFY(writeFile(targetPath, QByteArrayLiteral("obsolete profile")));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(controller.createNewDatabaseInteractive());

    const QString expectedPath = QFileInfo(targetPath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), expectedPath);
    QVERIFY2(
        readFile(targetPath) != QByteArrayLiteral("obsolete profile"),
        "successful normal creation must replace the selected profile"
        );
    QVERIFY(prompts.messages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::
normalCreateFailureRestoresExistingTarget()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("failed-create-target.tps")
        );
    const QByteArray originalContents = QByteArrayLiteral(
        "original profile bytes that must survive a failed create"
        );
    QVERIFY(writeFile(targetPath, originalContents));

    // SQLite cannot initialize the replacement while its rollback-journal
    // path is occupied by a directory.
    const QString journalPath = targetPath + QStringLiteral("-journal");
    QVERIFY(QDir().mkdir(journalPath));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(!controller.createNewDatabaseInteractive());

    QCOMPARE(readFile(targetPath), originalContents);
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.currentDatabasePath().isEmpty());
    QVERIFY(SettingsManager::instance().getRecentFiles().isEmpty());
    QVERIFY(SettingsManager::instance().getLastFile().isEmpty());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("New Teacher Profile")
        );
    const QString failure = prompts.messages.constFirst().message;
    QVERIFY(failure.contains(QStringLiteral("Unable to initialize Teacher Profile")));
    QVERIFY(failure.contains(QFileInfo(targetPath).absoluteFilePath()));
    QVERIFY(failure.contains(QStringLiteral("unable to open database file")));

    const QStringList recoveryCopies = QDir(workspaceRoot.path()).entryList(
        QStringList{
            QStringLiteral(".failed-create-target.tps.workspace-create-*.recovery")
        },
        QDir::Files | QDir::Hidden,
        QDir::Name
        );
    QVERIFY(recoveryCopies.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::
normalCreateFailureOnDifferentTargetRestoresActiveProfileAndHistory()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString activePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("active-profile.tps"))
        ).absoluteFilePath();
    const QString targetPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("failed-new-profile.tps"))
        ).absoluteFilePath();
    const QString otherRecentPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("other-recent.tps"))
        ).absoluteFilePath();
    const QByteArray originalTargetContents = QByteArrayLiteral(
        "existing different-target profile must survive"
        );

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(activePath));
    QVERIFY(seedServices.settingsService()->save(
        QStringLiteral("file-controller/sentinel"),
        QStringLiteral("active profile survives")
        ));
    seedServices.closeDatabase();
    QVERIFY(writeFile(targetPath, originalTargetContents));

    // SQLite cannot initialize the selected replacement while its journal
    // path is occupied by a directory.
    QVERIFY(QDir().mkdir(targetPath + QStringLiteral("-journal")));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    SettingsService* const retainedSettings = services.settingsService();
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(activePath);
    const QString normalizedActivePath = QFileInfo(activePath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedActivePath);
    QCOMPARE(FileControllerWorkspaceLifecycleTestAccess::currentFile(controller),
        normalizedActivePath);

    SettingsManager& settings = SettingsManager::instance();
    const QStringList priorHistory{normalizedActivePath, otherRecentPath};
    settings.setRecentFiles(priorHistory);
    settings.setLastFile(normalizedActivePath);

    QVERIFY(!controller.createNewDatabaseInteractive());

    QCOMPARE(readFile(targetPath), originalTargetContents);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedActivePath);
    QCOMPARE(FileControllerWorkspaceLifecycleTestAccess::currentFile(controller),
        normalizedActivePath);
    QCOMPARE(settings.getRecentFiles(), priorHistory);
    QCOMPARE(settings.getLastFile(), normalizedActivePath);
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());
    QVERIFY(retainedSettings->load(
        QStringLiteral("file-controller/sentinel")
        ).has_value());
    QCOMPARE(
        retainedSettings->load(
            QStringLiteral("file-controller/sentinel")
            )->toString(),
        QStringLiteral("active profile survives")
        );

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("New Teacher Profile"));
    QCOMPARE(
        static_cast<int>(warning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(warning.message.contains(
        QStringLiteral("Unable to initialize Teacher Profile")
        ));
    QVERIFY(warning.message.contains(targetPath));
    QVERIFY(!warning.message.contains(
        QStringLiteral("previous Teacher Profile could not be reopened")
        ));
}

void FileControllerWorkspaceLifecycleTests::
normalCreateFailureOnSameTargetRestoresActiveProfileAndHistory()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString activePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("active-profile.tps"))
        ).absoluteFilePath();
    const QString otherRecentPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("other-recent.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(activePath));
    QVERIFY(seedServices.settingsService()->save(
        QStringLiteral("file-controller/sentinel"),
        QStringLiteral("same-target profile survives")
        ));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(activePath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    SettingsService* const retainedSettings = services.settingsService();
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(activePath);
    const QString normalizedActivePath = QFileInfo(activePath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedActivePath);
    QCOMPARE(FileControllerWorkspaceLifecycleTestAccess::currentFile(controller),
        normalizedActivePath);

    SettingsManager& settings = SettingsManager::instance();
    const QStringList priorHistory{normalizedActivePath, otherRecentPath};
    settings.setRecentFiles(priorHistory);
    settings.setLastFile(normalizedActivePath);

    // The scripted create fails after the lifecycle backs up the original.
    // Allowing removal of the partial candidate exercises successful rollback
    // and the controller's same-path reopen path without obstructing reload.
    FailingWorkspaceCreationPort port(activePath);
    port.retainPartialReplacementAtTarget = false;
    FileControllerWorkspaceLifecycleTestAccess::
        installWorkspaceCreationLifecycle(controller, port);

    QVERIFY(!controller.createNewDatabaseInteractive());

    QVERIFY(port.createAttempted);
    QVERIFY(port.workspaceCloseAttempted);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedActivePath);
    QCOMPARE(FileControllerWorkspaceLifecycleTestAccess::currentFile(controller),
        normalizedActivePath);
    QCOMPARE(settings.getRecentFiles(), priorHistory);
    QCOMPARE(settings.getLastFile(), normalizedActivePath);
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());
    QVERIFY(retainedSettings->load(
        QStringLiteral("file-controller/sentinel")
        ).has_value());
    QCOMPARE(
        retainedSettings->load(
            QStringLiteral("file-controller/sentinel")
            )->toString(),
        QStringLiteral("same-target profile survives")
        );

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("New Teacher Profile"));
    QCOMPARE(
        static_cast<int>(warning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(warning.message.contains(
        QStringLiteral("Injected workspace creation failure")
        ));
    QVERIFY(!warning.message.contains(
        QStringLiteral("previous Teacher Profile could not be reopened")
        ));
}

void FileControllerWorkspaceLifecycleTests::
normalCreateFailureWithoutSafeRestoreLeavesNoActiveProfile()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString activePath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("active-profile.tps"))
        ).absoluteFilePath();
    const QString unrelatedRecentPath = QFileInfo(
        workspaceRoot.filePath(QStringLiteral("unrelated-recent.tps"))
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(activePath));
    QVERIFY(seedServices.settingsService()->save(
        QStringLiteral("file-controller/sentinel"),
        QStringLiteral("original active profile")
        ));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(activePath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);
    controller.loadDatabaseOnStartup(activePath);
    QVERIFY(services.hasOpenDatabase());

    SettingsManager& settings = SettingsManager::instance();
    const QStringList priorHistory{unrelatedRecentPath};
    settings.setRecentFiles(priorHistory);
    settings.setLastFile(unrelatedRecentPath);

    FailingWorkspaceCreationPort port(activePath);
    FileControllerWorkspaceLifecycleTestAccess::
        installWorkspaceCreationLifecycle(controller, port);

    QVERIFY(!controller.createNewDatabaseInteractive());

    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.currentDatabasePath().isEmpty());
    QVERIFY(FileControllerWorkspaceLifecycleTestAccess::currentFile(controller)
        .isEmpty());
    QVERIFY(!actions.saveFile->isEnabled());
    QVERIFY(!actions.closeFile->isEnabled());
    QCOMPARE(settings.getRecentFiles(), priorHistory);
    QCOMPARE(settings.getLastFile(), unrelatedRecentPath);
    QVERIFY(!settings.getRecentFiles().contains(activePath));
    QVERIFY(port.createAttempted);
    QCOMPARE(port.recoveryPaths.size(), 2);
    QVERIFY(QFileInfo::exists(port.recoveryPaths.constFirst()));
    QCOMPARE(readFile(activePath), port.partialReplacementContents);

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("New Teacher Profile"));
    QCOMPARE(
        static_cast<int>(warning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(warning.message.contains(
        QStringLiteral("original Teacher Profile could not be restored")
        ));
    QVERIFY(warning.message.contains(port.recoveryPaths.constFirst()));
    QVERIFY(warning.message.contains(activePath));
}

void FileControllerWorkspaceLifecycleTests::
workspaceCreationRollbackFailureRetainsOriginalRecoveryCopy()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("rollback-failure-target.tps")
        );
    const QByteArray originalContents = QByteArrayLiteral(
        "original profile bytes must remain recoverable"
        );
    QVERIFY(writeFile(targetPath, originalContents));

    FailingWorkspaceCreationPort port(targetPath);
    ClassMngr::Next::Application::WorkspaceCreationLifecycle lifecycle(port);
    const QByteArray encodedTargetPath = targetPath.toUtf8();
    const auto result = lifecycle.createReplacingProfile(
        ClassMngr::Next::Application::WorkspaceLocation(
            std::string(
                encodedTargetPath.constData(),
                static_cast<std::size_t>(encodedTargetPath.size())
                )
            )
        );

    QVERIFY(!result.created);
    QVERIFY(port.createAttempted);
    QVERIFY(port.workspaceCloseAttempted);
    QVERIFY(result.workspaceClosed);
    QVERIFY(!result.originalProfileRestored);
    QVERIFY(result.partialReplacementRemainsAtTarget);
    QCOMPARE(port.recoveryPaths.size(), 2);

    const QString originalRecoveryPath =
        port.recoveryPaths.constFirst();
    QCOMPARE(
        QString::fromUtf8(
            result.originalRecoveryPath.value().data(),
            static_cast<qsizetype>(
                result.originalRecoveryPath.value().size()
                )
            ),
        originalRecoveryPath
        );
    QCOMPARE(readFile(originalRecoveryPath), originalContents);
    QCOMPARE(readFile(targetPath), port.partialReplacementContents);

    const QString warning =
        ClassMngr::App::Controllers::
            newTeacherProfileCreationFailureMessage(result, targetPath);
    QCOMPARE(
        warning,
        QStringLiteral(
            "Injected workspace creation failure.\n\n"
            "The original Teacher Profile could not be restored to the "
            "selected path. Its recovery copy is at:\n%1\n\n"
            "The incomplete replacement remains at:\n%2"
            ).arg(originalRecoveryPath, targetPath)
        );
}

void FileControllerWorkspaceLifecycleTests::createDoesNotPrepareAfterCloseFailure()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString existingPath = workspaceRoot.filePath(
        QStringLiteral("currently-open.tps")
        );
    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(existingPath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(existingPath);
    QVERIFY(services.hasOpenDatabase());

    const QString expectedExistingPath = QFileInfo(existingPath).absoluteFilePath();
    QCOMPARE(SettingsManager::instance().getLastFile(), expectedExistingPath);

    // Leave the coordinator session stale so its close postcondition fails.
    services.closeDatabase();

    const QString replacementPath = workspaceRoot.filePath(
        QStringLiteral("replacement.tps")
        );
    QVERIFY(writeFile(replacementPath, QByteArrayLiteral("must survive")));
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(replacementPath)
        );

    QVERIFY(!controller.createNewDatabaseInteractive());
    QCOMPARE(readFile(replacementPath), QByteArrayLiteral("must survive"));
    QVERIFY(!services.hasOpenDatabase());
    QCOMPARE(SettingsManager::instance().getLastFile(), expectedExistingPath);
    QVERIFY(prompts.messages.isEmpty());

    const QString initialSetupPath = workspaceRoot.filePath(
        QStringLiteral("initial-setup.tps")
        );
    QVERIFY(writeFile(initialSetupPath, QByteArrayLiteral("must also survive")));
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(initialSetupPath)
        );

    QVERIFY(!controller.createInitialSetupDatabaseInteractive());
    QCOMPARE(readFile(initialSetupPath), QByteArrayLiteral("must also survive"));
    QVERIFY(initialSetupBackups(workspaceRoot, QStringLiteral("initial-setup.tps")).isEmpty());
    QVERIFY(prompts.messages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::initialSetupBackupIsRemovedOnFinish()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("setup-target.tps")
        );
    const QByteArray originalContents = QByteArrayLiteral("original profile");
    QVERIFY(writeFile(targetPath, originalContents));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(controller.createInitialSetupDatabaseInteractive());
    const QString expectedPath = QFileInfo(targetPath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), expectedPath);
    QCOMPARE(SettingsManager::instance().getRecentFiles(), QStringList());

    const QStringList backups = initialSetupBackups(
        workspaceRoot,
        QStringLiteral("setup-target.tps")
        );
    QCOMPARE(backups.size(), 1);
    QCOMPARE(
        readFile(workspaceRoot.filePath(backups.constFirst())),
        originalContents
        );

    controller.finishInitialSetup();

    QVERIFY(initialSetupBackups(workspaceRoot, QStringLiteral("setup-target.tps")).isEmpty());
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{expectedPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), expectedPath);
}

void FileControllerWorkspaceLifecycleTests::initialSetupBackupIsRestoredOnCancel()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("cancel-target.tps")
        );
    const QByteArray originalContents = QByteArrayLiteral("restore this profile");
    QVERIFY(writeFile(targetPath, originalContents));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(controller.createInitialSetupDatabaseInteractive());
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        initialSetupBackups(workspaceRoot, QStringLiteral("cancel-target.tps")).size(),
        1
        );

    controller.cancelInitialSetup();

    QVERIFY(!services.hasOpenDatabase());
    QCOMPARE(readFile(targetPath), originalContents);
    QVERIFY(initialSetupBackups(workspaceRoot, QStringLiteral("cancel-target.tps")).isEmpty());
    QVERIFY(SettingsManager::instance().getRecentFiles().isEmpty());
    QVERIFY(SettingsManager::instance().getLastFile().isEmpty());
}

void FileControllerWorkspaceLifecycleTests::
initialSetupRestoreWithRecoveryCleanupFailureWarnsWithPath()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("cleanup-failure-target.tps")
        );
    const QString backupPath = workspaceRoot.filePath(
        QStringLiteral(".cleanup-failure-target.original.backup")
        );
    const QString incompleteRecoveryPath = workspaceRoot.filePath(
        QStringLiteral(".cleanup-failure-target.incomplete.recovery")
        );
    const QByteArray originalContents = QByteArrayLiteral("original profile");
    const QByteArray incompleteContents = QByteArrayLiteral(
        "incomplete setup profile"
        );
    QVERIFY(writeFile(targetPath, originalContents));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    ScriptedInitialSetupLifecyclePort port(
        backupPath,
        incompleteRecoveryPath
        );
    port.failMoveCalls.clear();
    port.failRemoveCalls.insert(1);
    port.servicesToClose = &services;
    FileController controller(&services, nullptr);
    const auto started =
        FileControllerWorkspaceLifecycleTestAccess::beginInitialSetup(
            controller,
            port,
            workspaceLocation(targetPath)
            );

    QVERIFY(started.started());
    QCOMPARE(readFile(backupPath), originalContents);
    QCOMPARE(readFile(targetPath), incompleteContents);

    const QString activeWorkspacePath = workspaceRoot.filePath(
        QStringLiteral("active-workspace.tps")
        );
    QVERIFY(services.openDatabase(activeWorkspacePath));
    QVERIFY(services.hasOpenDatabase());

    controller.cancelInitialSetup();

    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.currentDatabasePath().isEmpty());
    QVERIFY(!port.active);
    QCOMPARE(port.closeCalls, 2);
    QCOMPARE(readFile(targetPath), originalContents);
    QVERIFY(!QFileInfo::exists(backupPath));
    QCOMPARE(readFile(incompleteRecoveryPath), incompleteContents);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Initial Setup")
        );
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral(
            "The original Teacher Profile was restored, but the incomplete "
            "setup copy could not be removed. It remains at:\n%1"
            ).arg(incompleteRecoveryPath)
        );
}

void FileControllerWorkspaceLifecycleTests::
initialSetupRestoreWithSuccessfulCleanupDoesNotWarn()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("cleanup-success-target.tps")
        );
    const QString backupPath = workspaceRoot.filePath(
        QStringLiteral(".cleanup-success-target.original.backup")
        );
    const QString incompleteRecoveryPath = workspaceRoot.filePath(
        QStringLiteral(".cleanup-success-target.incomplete.recovery")
        );
    const QByteArray originalContents = QByteArrayLiteral("original profile");
    const QByteArray incompleteContents = QByteArrayLiteral(
        "incomplete setup profile"
        );
    QVERIFY(writeFile(targetPath, originalContents));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    ScriptedInitialSetupLifecyclePort port(
        backupPath,
        incompleteRecoveryPath
        );
    port.failMoveCalls.clear();
    port.servicesToClose = &services;
    FileController controller(&services, nullptr);
    const auto started =
        FileControllerWorkspaceLifecycleTestAccess::beginInitialSetup(
            controller,
            port,
            workspaceLocation(targetPath)
            );

    QVERIFY(started.started());
    QCOMPARE(readFile(backupPath), originalContents);
    QCOMPARE(readFile(targetPath), incompleteContents);

    const QString activeWorkspacePath = workspaceRoot.filePath(
        QStringLiteral("active-workspace.tps")
        );
    QVERIFY(services.openDatabase(activeWorkspacePath));
    QVERIFY(services.hasOpenDatabase());

    controller.cancelInitialSetup();

    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.currentDatabasePath().isEmpty());
    QCOMPARE(readFile(targetPath), originalContents);
    QVERIFY(!QFileInfo::exists(backupPath));
    QVERIFY(!QFileInfo::exists(incompleteRecoveryPath));
    QVERIFY(prompts.messages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::
initialSetupWithoutBackupUpdatesRecentOnlyOnFinish()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("new-setup-target.tps")
        );
    QVERIFY(!QFileInfo::exists(targetPath));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(controller.createInitialSetupDatabaseInteractive());
    const QString expectedPath = QFileInfo(targetPath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), expectedPath);
    QVERIFY(initialSetupBackups(
        workspaceRoot,
        QStringLiteral("new-setup-target.tps")
        ).isEmpty());
    QVERIFY(SettingsManager::instance().getRecentFiles().isEmpty());
    QVERIFY(SettingsManager::instance().getLastFile().isEmpty());
    QVERIFY(prompts.messages.isEmpty());

    controller.finishInitialSetup();

    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), expectedPath);
    QVERIFY(initialSetupBackups(
        workspaceRoot,
        QStringLiteral("new-setup-target.tps")
        ).isEmpty());
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{expectedPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), expectedPath);
    QVERIFY(prompts.messages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::
initialSetupCreateFailureWarnsAfterRestoringOriginal()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("failed-setup-target.tps")
        );
    const QByteArray originalContents = QByteArrayLiteral("original profile");
    QVERIFY(writeFile(targetPath, originalContents));

    // SQLite cannot open the replacement while its rollback-journal path is
    // occupied by a directory. The initial-setup lifecycle must first restore
    // the preserved target, then report the workspace creation error.
    const QString journalPath = targetPath + QStringLiteral("-journal");
    QVERIFY(QDir().mkdir(journalPath));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(!controller.createInitialSetupDatabaseInteractive());

    QCOMPARE(readFile(targetPath), originalContents);
    QVERIFY(initialSetupBackups(
        workspaceRoot,
        QStringLiteral("failed-setup-target.tps")
        ).isEmpty());
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(SettingsManager::instance().getRecentFiles().isEmpty());
    QVERIFY(SettingsManager::instance().getLastFile().isEmpty());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("New Teacher Profile")
        );
    const QString createError = prompts.messages.constFirst().message;
    QVERIFY(createError.startsWith(
        QStringLiteral("Unable to initialize Teacher Profile:\n%1\n\n")
            .arg(QFileInfo(targetPath).absoluteFilePath())
        ));
    QVERIFY(createError.contains(QStringLiteral("unable to open database file")));

    // This source contract guards the two-warning branch too: when rollback
    // cannot restore the original, its recovery warning must be emitted before
    // the create error so neither message masks the other.
    const QString source = fileControllerSource();
    QVERIFY(!source.isEmpty());
    const qsizetype rollbackWarning = source.indexOf(
        QStringLiteral("showInitialSetupCancelWarning(started.rollback);")
        );
    const qsizetype createErrorWarning = source.indexOf(
        QStringLiteral("domainErrorMessage(started.error)")
        );
    QVERIFY(rollbackWarning >= 0);
    QVERIFY(createErrorWarning > rollbackWarning);
}

void FileControllerWorkspaceLifecycleTests::
initialSetupRestoreFailureWarnsWithOriginalBackupPath()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("restore-warning-target.tps")
        );
    const QByteArray originalContents = QByteArrayLiteral("original profile");
    QVERIFY(writeFile(targetPath, originalContents));

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(targetPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(controller.createInitialSetupDatabaseInteractive());
    QVERIFY(services.hasOpenDatabase());
    const QStringList backups = initialSetupBackups(
        workspaceRoot,
        QStringLiteral("restore-warning-target.tps")
        );
    QCOMPARE(backups.size(), 1);

    // Model an unavailable backup path at cancel time. Preserve its contents
    // beside the expected recovery path so this failure case remains
    // non-destructive while exercising the UI warning mapping.
    const QString backupPath = workspaceRoot.filePath(backups.constFirst());
    const QString heldOriginalPath = backupPath + QStringLiteral(".held");
    QVERIFY(QFile::rename(backupPath, heldOriginalPath));

    controller.cancelInitialSetup();

    QVERIFY(!services.hasOpenDatabase());
    QCOMPARE(readFile(heldOriginalPath), originalContents);
    QVERIFY(QFileInfo::exists(targetPath));
    QVERIFY(SettingsManager::instance().getRecentFiles().isEmpty());
    QVERIFY(SettingsManager::instance().getLastFile().isEmpty());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Initial Setup")
        );
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral(
            "The original Teacher Profile could not be restored:\n%1"
            ).arg(backupPath)
        );
}

void FileControllerWorkspaceLifecycleTests::
initialSetupRestoreAndMoveBackFailureWarnsWithBothRecoveryPaths()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString targetPath = workspaceRoot.filePath(
        QStringLiteral("restore-and-moveback-warning-target.tps")
        );
    const QString backupPath = workspaceRoot.filePath(
        QStringLiteral(".restore-and-moveback-warning-target.original.backup")
        );
    const QString incompleteRecoveryPath = workspaceRoot.filePath(
        QStringLiteral(".restore-and-moveback-warning-target.incomplete.recovery")
        );
    const QByteArray originalContents = QByteArrayLiteral("original profile");
    const QByteArray incompleteContents = QByteArrayLiteral(
        "incomplete setup profile"
        );
    QVERIFY(writeFile(targetPath, originalContents));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    ScriptedInitialSetupLifecyclePort port(
        backupPath,
        incompleteRecoveryPath
        );
    FileController controller(&services, nullptr);
    const auto started =
        FileControllerWorkspaceLifecycleTestAccess::beginInitialSetup(
            controller,
            port,
            workspaceLocation(targetPath)
            );

    QVERIFY(started.started());
    QCOMPARE(readFile(backupPath), originalContents);
    QCOMPARE(readFile(targetPath), incompleteContents);

    controller.cancelInitialSetup();

    QVERIFY(!QFileInfo::exists(targetPath));
    QCOMPARE(readFile(backupPath), originalContents);
    QCOMPARE(readFile(incompleteRecoveryPath), incompleteContents);
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Initial Setup"));
    QVERIFY(warning.message.contains(
        QStringLiteral("Original backup path:\n%1").arg(backupPath)
        ));
    QVERIFY(warning.message.contains(
        QStringLiteral(
            "Incomplete Teacher Profile recovery path:\n%1"
            ).arg(incompleteRecoveryPath)
        ));
}

void FileControllerWorkspaceLifecycleTests::createErrorUsesStructuredUtf8Message()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString invalidPath = workspaceRoot.filePath(
        QStringLiteral("structured-error")
        ) + QChar::Null + QStringLiteral(".tps");

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(invalidPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);

    QVERIFY(!controller.createNewDatabaseInteractive());
    QVERIFY(!services.hasOpenDatabase());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("New Teacher Profile")
        );
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Opening the workspace returned an invalid path.")
        );
    QVERIFY(SettingsManager::instance().getRecentFiles().isEmpty());
}

void FileControllerWorkspaceLifecycleTests::successfulStartupLoadPersistsNormalizedPath()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    SettingsManager::instance().clear();

    const QString requestedPath = workspaceRoot.filePath(
        QStringLiteral("normalized-startup")
        );
    const QString normalizedPath = workspaceRoot.filePath(
        QStringLiteral("normalized-startup.tps")
        );

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(normalizedPath));
    seedServices.closeDatabase();

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(requestedPath);

    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(normalizedPath).absoluteFilePath()
        );
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{QFileInfo(normalizedPath).absoluteFilePath()}
        );
    QCOMPARE(
        SettingsManager::instance().getLastFile(),
        QFileInfo(normalizedPath).absoluteFilePath()
        );
}

void FileControllerWorkspaceLifecycleTests::
failedReplacementOpenPreservesActiveWorkspaceAndUiState()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    SettingsManager::instance().clear();

    const QString activePath = workspaceRoot.filePath(
        QStringLiteral("active-profile.tps")
        );
    const QString invalidPath = workspaceRoot.filePath(
        QStringLiteral("invalid-profile.tps")
        );
    const QString replacementPath = workspaceRoot.filePath(
        QStringLiteral("replacement-profile.tps")
        );

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(activePath));
    QVERIFY(seedServices.settingsService()->save(
        QStringLiteral("file-controller/sentinel"),
        QStringLiteral("profile A")
        ));
    seedServices.closeDatabase();
    QVERIFY(seedServices.openDatabase(replacementPath));
    QVERIFY(seedServices.settingsService()->save(
        QStringLiteral("file-controller/sentinel"),
        QStringLiteral("profile B")
        ));
    seedServices.closeDatabase();

    QVERIFY(writeFile(
        invalidPath,
        QByteArrayLiteral("not a SQLite database")
        ));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    SettingsService* const retainedSettings = services.settingsService();
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(activePath);
    const QString normalizedActivePath =
        QFileInfo(activePath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedActivePath);
    QCOMPARE(SettingsManager::instance().getRecentFiles(), QStringList{
        normalizedActivePath
    });
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedActivePath);
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());
    QCOMPARE(
        retainedSettings->load(
            QStringLiteral("file-controller/sentinel")
            )->toString(),
        QStringLiteral("profile A")
        );

    controller.loadDatabaseOnStartup(invalidPath);

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Open Teacher Profile"));
    QCOMPARE(
        static_cast<int>(warning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(warning.message.contains(
        QStringLiteral("Unable to initialize Teacher Profile")
        ));
    QVERIFY(warning.message.contains(QFileInfo(invalidPath).absoluteFilePath()));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedActivePath);
    QCOMPARE(SettingsManager::instance().getRecentFiles(), QStringList{
        normalizedActivePath
    });
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedActivePath);
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());
    QCOMPARE(
        retainedSettings->load(
            QStringLiteral("file-controller/sentinel")
            )->toString(),
        QStringLiteral("profile A")
        );

    controller.autosave();
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(services.currentDatabasePath(), normalizedActivePath);

    controller.loadDatabaseOnStartup(replacementPath);
    const QString normalizedReplacementPath =
        QFileInfo(replacementPath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedReplacementPath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        (QStringList{
            normalizedReplacementPath,
            normalizedActivePath
        })
        );
    QCOMPARE(
        SettingsManager::instance().getLastFile(),
        normalizedReplacementPath
        );
    QCOMPARE(
        retainedSettings->load(
            QStringLiteral("file-controller/sentinel")
            )->toString(),
        QStringLiteral("profile B")
        );
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());
}

void FileControllerWorkspaceLifecycleTests::missingStartupPathUsesCapturedWarning()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    SettingsManager::instance().clear();

    const QString missingPath = workspaceRoot.filePath(
        QStringLiteral("missing-startup.tps")
        );

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(missingPath);

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& request = prompts.messages.constFirst();
    QCOMPARE(request.title, QStringLiteral("Missing File"));
    QCOMPARE(
        request.message,
        QStringLiteral("File not found:\n%1")
            .arg(QFileInfo(missingPath).absoluteFilePath())
        );
    QCOMPARE(
        static_cast<int>(request.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::invalidStartupDatabaseUsesLegacyErrorText()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    SettingsManager::instance().clear();

    const QString invalidPath = workspaceRoot.filePath(
        QStringLiteral("invalid-startup.tps")
        );
    QFile invalidFile(invalidPath);
    QVERIFY(invalidFile.open(QIODevice::WriteOnly));
    QVERIFY(invalidFile.write("not a SQLite database") > 0);
    invalidFile.close();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(invalidPath);

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& request = prompts.messages.constFirst();
    QCOMPARE(request.title, QStringLiteral("Open Teacher Profile"));
    QCOMPARE(
        static_cast<int>(request.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY2(
        !request.message.trimmed().isEmpty(),
        "the legacy open error text must be preserved"
        );
}

void FileControllerWorkspaceLifecycleTests::startupLoadUsesCoordinatorAndLegacyFallback()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString legacyPath = workspaceRoot.filePath(
        QStringLiteral("created-by-legacy-path.tps")
        );
    const QString firstPath = workspaceRoot.filePath(
        QStringLiteral("first-프로필.tps")
        );
    const QString secondPath = workspaceRoot.filePath(
        QStringLiteral("second-프로필.tps")
        );

    ApplicationServices seedServices;
    for (const QString& path : {legacyPath, firstPath, secondPath})
    {
        QVERIFY(seedServices.openDatabase(path));
        seedServices.closeDatabase();
    }

    ApplicationServices services;
    FileController controller(&services, nullptr);

    // Creation and initial setup still open through the legacy facade in this
    // slice. An already-open legacy service is the compatibility state that
    // the controller must close before entering the coordinator path.
    QVERIFY(services.openDatabase(legacyPath));
    controller.loadDatabaseOnStartup(firstPath);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(firstPath).absoluteFilePath()
        );

    // The next startup load replaces the coordinator-owned session after the
    // candidate workspace opens successfully.
    controller.loadDatabaseOnStartup(secondPath);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(secondPath).absoluteFilePath()
        );
}

void FileControllerWorkspaceLifecycleTests::
startupRejectsOlderSchemasAndPreservesWorkspaceState_data()
{
    QTest::addColumn<int>("version");
    QTest::addColumn<bool>("hasActiveWorkspace");
    for (int version = 0; version < 6; ++version)
    {
        const QByteArray row = QByteArray::number(version);
        QTest::newRow((row + "-closed").constData()) << version << false;
        QTest::newRow((row + "-active-v6").constData()) << version << true;
    }
}

void FileControllerWorkspaceLifecycleTests::
startupRejectsOlderSchemasAndPreservesWorkspaceState()
{
    QFETCH(int, version);
    QFETCH(bool, hasActiveWorkspace);
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    const QString olderPath = workspaceRoot.filePath(QStringLiteral("older-startup.db"));
    const QString fixturePath = QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR))
        .filePath(QStringLiteral("tests/fixtures/workspaces/legacy_startup.sql"));
    QString setupError;
    QVERIFY2(materializeSqlFixture(fixturePath, olderPath, &setupError), qPrintable(setupError));
    const QString connectionName = QStringLiteral("older-startup-version");
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(olderPath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version)));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    const QByteArray contentsBefore = readFile(olderPath);
    QVERIFY(!contentsBefore.isEmpty());

    SettingsManager::instance().clear();
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);
    const QString activePath = workspaceRoot.filePath(QStringLiteral("current-v6.tps"));
    if (hasActiveWorkspace)
    {
        QVERIFY(services.openDatabase(activePath));
        services.closeDatabase();
        controller.loadDatabaseOnStartup(activePath);
        QVERIFY(services.hasOpenDatabase());
    }
    const QString activePathBefore = services.currentDatabasePath();
    const QStringList recentBefore = SettingsManager::instance().getRecentFiles();
    const QString lastBefore = SettingsManager::instance().getLastFile();
    const QStringList filesBefore = QDir(workspaceRoot.path()).entryList(QDir::Files);

    controller.loadDatabaseOnStartup(olderPath);

    QCOMPARE(prompts.messages.size(), 1);
    const auto& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Open Teacher Profile"));
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QVERIFY(warning.message.contains(QFileInfo(olderPath).absoluteFilePath()));
    QVERIFY(warning.message.contains(QStringLiteral("unsupported schema version %1").arg(version)));
    QVERIFY(warning.message.contains(QStringLiteral("Migration from older Teacher Profiles is not available")));
    QVERIFY(prompts.confirmations.isEmpty());
    QCOMPARE(services.hasOpenDatabase(), hasActiveWorkspace);
    QCOMPARE(services.currentDatabasePath(), activePathBefore);
    QCOMPARE(SettingsManager::instance().getRecentFiles(), recentBefore);
    QCOMPARE(SettingsManager::instance().getLastFile(), lastBefore);
    QCOMPARE(actions.saveFile->isEnabled(), hasActiveWorkspace);
    QCOMPARE(actions.closeFile->isEnabled(), hasActiveWorkspace);
    QCOMPARE(readFile(olderPath), contentsBefore);
    QCOMPARE(QDir(workspaceRoot.path()).entryList(QDir::Files), filesBefore);

    LegacyDatabaseInspection inspection;
    QString inspectionError;
    QVERIFY2(inspectLegacyDatabase(olderPath, &inspection, &inspectionError), qPrintable(inspectionError));
    QCOMPARE(inspection.schemaVersion, version);
    QCOMPARE(inspection.unassignedTeacherIsNull, 0);
}

void FileControllerWorkspaceLifecycleTests::autosaveUsesCoordinatorForOpenWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save.tps")
        );

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(workspacePath);
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(prompts.messages.isEmpty());

    controller.autosave();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(workspacePath).absoluteFilePath()
        );
}

void FileControllerWorkspaceLifecycleTests::autosaveWarnsAndPreservesStaleCoordinatorState()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString firstPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-first.tps")
        );
    const QString secondPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-second.tps")
        );

    ApplicationServices seedServices;
    for (const QString& path : {firstPath, secondPath})
    {
        QVERIFY(seedServices.openDatabase(path));
        seedServices.closeDatabase();
    }

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    controller.loadDatabaseOnStartup(firstPath);
    QVERIFY(services.hasOpenDatabase());

    // Keep the coordinator's session on the first path while the
    // compatibility service is externally moved to a different workspace.
    QVERIFY(services.openDatabase(secondPath));
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(secondPath).absoluteFilePath()
        );

    controller.autosave();

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest firstWarning = prompts.messages.constFirst();
    QCOMPARE(firstWarning.title, QStringLiteral("Save Teacher Profile"));
    QCOMPARE(
        firstWarning.message,
        QStringLiteral(
            "Saving the workspace received a handle for a different open workspace."
            )
        );
    QCOMPARE(
        static_cast<int>(firstWarning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(secondPath).absoluteFilePath()
        );

    // A second failure proves the v2 session and compatibility/UI state were
    // not cleared or replaced after the first structured failure.
    controller.autosave();
    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(
        prompts.messages.constLast().message,
        firstWarning.message
        );
}

void FileControllerWorkspaceLifecycleTests::autosaveUsesLegacyFallbackForCompatibilityWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = workspaceRoot.filePath(
        QStringLiteral("legacy-save.tps")
        );

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    seedServices.closeDatabase();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    // This compatibility path is open before FileController can create a v2
    // session, so autosave must retain the historical void service call.
    QVERIFY(services.openDatabase(workspacePath));
    FileController controller(&services, nullptr);

    controller.autosave();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(workspacePath).absoluteFilePath()
        );
}

void FileControllerWorkspaceLifecycleTests::saveAsUsesCoordinatorForOpenWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-source.tps")
        );
    const QString requestedDestination = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-destination")
        );
    const QString destinationPath = QFileInfo(
        requestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(requestedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(sourcePath);
    const QString normalizedSourcePath = QFileInfo(sourcePath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedSourcePath);
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());

    actions.saveAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(destinationPath));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), destinationPath);
    const QStringList expectedRecentFiles{
        destinationPath,
        normalizedSourcePath
    };
    QCOMPARE(SettingsManager::instance().getRecentFiles(), expectedRecentFiles);
    QCOMPARE(SettingsManager::instance().getLastFile(), destinationPath);
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().title,
        QStringLiteral("Save Teacher Profile")
        );
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().defaultSuffix,
        QStringLiteral("tps")
        );

    // The coordinator session must now use the returned save-as location;
    // saving again proves the controller did not need to reopen it through
    // loadDatabase.
    controller.autosave();
    QVERIFY(prompts.messages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::saveAsWarnsAndPreservesStaleCoordinatorState()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString firstPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-first.tps")
        );
    const QString secondPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-second.tps")
        );
    const QString failedDestination = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-failed.tps")
        );

    ApplicationServices seedServices;
    for (const QString& path : {firstPath, secondPath})
    {
        QVERIFY(seedServices.openDatabase(path));
        seedServices.closeDatabase();
    }

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(failedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(firstPath);
    const QString normalizedFirstPath = QFileInfo(firstPath).absoluteFilePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedFirstPath);

    // Leave the coordinator's session on the first path while the
    // compatibility service is externally moved to a different workspace.
    QVERIFY(services.openDatabase(secondPath));
    QCOMPARE(
        services.currentDatabasePath(),
        QFileInfo(secondPath).absoluteFilePath()
        );

    actions.saveAsFile->trigger();

    QVERIFY(!QFileInfo::exists(failedDestination));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Save Teacher Profile")
        );
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral(
            "Saving the workspace as received a handle for a different open workspace."
            )
        );
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedFirstPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedFirstPath);
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());

    // Re-align the compatibility service and prove the failed coordinator
    // call left its session and controller state available for a later save.
    QVERIFY(services.openDatabase(firstPath));
    const QString recoveredDestination = workspaceRoot.filePath(
        QStringLiteral("coordinator-save-as-recovered.tps")
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(recoveredDestination)
        );

    actions.saveAsFile->trigger();

    QVERIFY(QFileInfo::exists(recoveredDestination));
    QCOMPARE(prompts.messages.size(), 1);
    const QString recoveredPath = QFileInfo(recoveredDestination).absoluteFilePath();
    const QStringList expectedRecentFiles{
        recoveredPath,
        normalizedFirstPath
    };
    QCOMPARE(SettingsManager::instance().getRecentFiles(), expectedRecentFiles);
    QCOMPARE(
        SettingsManager::instance().getLastFile(),
        recoveredPath
        );
}

void FileControllerWorkspaceLifecycleTests::saveAsUsesLegacyFallbackForCompatibilityWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = workspaceRoot.filePath(
        QStringLiteral("legacy-save-as-source.tps")
        );
    const QString requestedDestination = workspaceRoot.filePath(
        QStringLiteral("legacy-save-as-destination")
        );
    const QString destinationPath = QFileInfo(
        requestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(requestedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    // This compatibility path is open before FileController can create a v2
    // session, so save-as must retain the legacy call followed by loadDatabase.
    QVERIFY(services.openDatabase(sourcePath));
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.saveAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(destinationPath));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), destinationPath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{destinationPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), destinationPath);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().title,
        QStringLiteral("Save Teacher Profile")
        );
}

void FileControllerWorkspaceLifecycleTests::exportUsesCoordinatorForOpenWorkspaceAndPreservesState()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = workspaceRoot.filePath(
        QStringLiteral("coordinator-export-source-내보내기.tps")
        );
    const QString firstRequestedDestination = workspaceRoot.filePath(
        QStringLiteral("exports/first-export-내보내기")
        );
    const QString secondRequestedDestination = workspaceRoot.filePath(
        QStringLiteral("exports/second-export-내보내기")
        );
    const QString firstDestination = QFileInfo(
        firstRequestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();
    const QString secondDestination = QFileInfo(
        secondRequestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(firstRequestedDestination)
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(secondRequestedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(sourcePath);
    const QString normalizedSourcePath = QFileInfo(sourcePath).absoluteFilePath();
    const QString sourceDirectory = QFileInfo(sourcePath).absoluteDir().absolutePath();
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedSourcePath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedSourcePath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedSourcePath);
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        sourceDirectory
        );
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());

    actions.exportAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(firstDestination));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedSourcePath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedSourcePath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedSourcePath);
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        QFileInfo(firstDestination).absoluteDir().absolutePath()
        );
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    const SaveFileRequest firstRequest = fileDialogs.saveFileRequests.constFirst();
    QCOMPARE(firstRequest.title, QStringLiteral("Export Teacher Profile As"));
    QCOMPARE(
        static_cast<int>(firstRequest.purpose),
        static_cast<int>(FileDialogPurpose::TeacherProfile)
        );
    QCOMPARE(firstRequest.initialDirectory, sourceDirectory);
    QCOMPARE(firstRequest.defaultSuffix, QStringLiteral("tps"));
    QCOMPARE(
        firstRequest.nameFilters,
        QStringList{QStringLiteral("ClassMngr Teacher Profile (*.tps)")}
        );

    // The second dialog still follows the active source workspace, proving
    // export did not replace m_currentFile with the returned destination.
    actions.exportAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(secondDestination));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedSourcePath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedSourcePath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedSourcePath);
    QCOMPARE(fileDialogs.saveFileRequests.size(), 2);
    QCOMPARE(
        fileDialogs.saveFileRequests.at(1).initialDirectory,
        sourceDirectory
        );

    controller.autosave();
    QVERIFY(prompts.messages.isEmpty());
    QCOMPARE(services.currentDatabasePath(), normalizedSourcePath);
}

void FileControllerWorkspaceLifecycleTests::
preferenceWriteFailureDoesNotFailCreateAndCanRetry()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString previousDirectory =
        QDir(workspaceRoot.path()).filePath(QStringLiteral("previous-directory"));
    QVERIFY(QDir().mkpath(previousDirectory));
    const QString firstRequestedPath =
        QDir(workspaceRoot.path()).filePath(QStringLiteral("first-create/profile"));
    const QString retryRequestedPath =
        QDir(workspaceRoot.path()).filePath(QStringLiteral("retry-create/profile"));
    const QString firstWorkspacePath = QFileInfo(
        firstRequestedPath + QStringLiteral(".tps")
        ).absoluteFilePath();
    const QString retryWorkspacePath = QFileInfo(
        retryRequestedPath + QStringLiteral(".tps")
        ).absoluteFilePath();

    SettingsManager::instance().setLastDatabaseDirectory(previousDirectory);
    SettingsManager::instance().sync();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(firstRequestedPath)
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(retryRequestedPath)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    ClassMngr::Next::Platform::SettingsManagerLastDatabaseDirectoryPort
        directoryPort(
            [&injectAccessError, &synchronizationAttempts](QSettings& target)
            {
                ++synchronizationAttempts;
                if (injectAccessError)
                {
                    return QSettings::AccessError;
                }

                target.sync();
                return target.status();
            }
            );

    ApplicationServices services;
    FileController controller(
        &services,
        nullptr,
        nullptr,
        &directoryPort
        );

    const bool createdDespitePreferenceFailure =
        controller.createNewDatabaseInteractive();

    QVERIFY(createdDespitePreferenceFailure);
    QVERIFY(QFileInfo::exists(firstWorkspacePath));
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Settings"));
    QCOMPARE(
        static_cast<int>(warning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(warning.message.contains(
        QStringLiteral("last database directory preference could not be saved")
        ));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), firstWorkspacePath);
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        previousDirectory
        );
    QCOMPARE(synchronizationAttempts, 1);

    injectAccessError = false;
    const bool retriedCreate = controller.createNewDatabaseInteractive();

    QVERIFY(retriedCreate);
    QVERIFY(QFileInfo::exists(retryWorkspacePath));
    QCOMPARE(prompts.messages.size(), 1);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), retryWorkspacePath);
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        QFileInfo(retryWorkspacePath).absoluteDir().absolutePath()
        );
    QCOMPARE(synchronizationAttempts, 2);
}

void FileControllerWorkspaceLifecycleTests::exportWarnsAndPreservesStaleCoordinatorState()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString firstPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-export-first-내보내기.tps")
        );
    const QString secondPath = workspaceRoot.filePath(
        QStringLiteral("coordinator-export-second-내보내기.tps")
        );
    const QString firstFailedDestination = workspaceRoot.filePath(
        QStringLiteral("exports/failed-export-one")
        );
    const QString secondFailedDestination = workspaceRoot.filePath(
        QStringLiteral("exports/failed-export-two")
        );

    ApplicationServices seedServices;
    for (const QString& path : {firstPath, secondPath})
    {
        QVERIFY(seedServices.openDatabase(path));
        seedServices.closeDatabase();
    }

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(firstFailedDestination)
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(secondFailedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    controller.loadDatabaseOnStartup(firstPath);
    const QString normalizedFirstPath = QFileInfo(firstPath).absoluteFilePath();
    const QString normalizedSecondPath = QFileInfo(secondPath).absoluteFilePath();
    const QString firstDirectory = QFileInfo(firstPath).absoluteDir().absolutePath();
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(services.openDatabase(secondPath));
    QCOMPARE(services.currentDatabasePath(), normalizedSecondPath);

    actions.exportAsFile->trigger();

    QVERIFY(!QFileInfo::exists(firstFailedDestination + QStringLiteral(".tps")));
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest firstWarning = prompts.messages.constFirst();
    QCOMPARE(firstWarning.title, QStringLiteral("Export Teacher Profile"));
    QCOMPARE(
        firstWarning.message,
        QStringLiteral(
            "Exporting the workspace received a handle for a different open workspace."
            )
        );
    QCOMPARE(
        static_cast<int>(firstWarning.severity),
        static_cast<int>(PromptSeverity::Warning)
        );
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedSecondPath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedFirstPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedFirstPath);
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        firstDirectory
        );
    QVERIFY(actions.saveFile->isEnabled());
    QVERIFY(actions.closeFile->isEnabled());

    // A repeated failure proves the coordinator session and controller state
    // survived the structured export error.
    actions.exportAsFile->trigger();

    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(prompts.messages.constLast().title, firstWarning.title);
    QCOMPARE(prompts.messages.constLast().message, firstWarning.message);
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), normalizedSecondPath);
    QCOMPARE(
        SettingsManager::instance().getRecentFiles(),
        QStringList{normalizedFirstPath}
        );
    QCOMPARE(SettingsManager::instance().getLastFile(), normalizedFirstPath);
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        firstDirectory
        );
}

void FileControllerWorkspaceLifecycleTests::exportUsesLegacyFallbackForCompatibilityWorkspace()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = workspaceRoot.filePath(
        QStringLiteral("legacy-export-source-내보내기.tps")
        );
    const QString requestedDestination = workspaceRoot.filePath(
        QStringLiteral("exports/legacy-export-destination-내보내기")
        );
    const QString destinationPath = QFileInfo(
        requestedDestination + QStringLiteral(".tps")
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(requestedDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    // This compatibility path is open before FileController can create a v2
    // session, so export must retain the historical service call.
    QVERIFY(services.openDatabase(sourcePath));
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.exportAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(destinationPath));
    QVERIFY(services.hasOpenDatabase());
    QCOMPARE(services.currentDatabasePath(), QFileInfo(sourcePath).absoluteFilePath());
    QVERIFY(SettingsManager::instance().getRecentFiles().isEmpty());
    QVERIFY(SettingsManager::instance().getLastFile().isEmpty());
    QCOMPARE(
        SettingsManager::instance().getLastDatabaseDirectory(),
        QFileInfo(destinationPath).absoluteDir().absolutePath()
        );
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().title,
        QStringLiteral("Export Teacher Profile As")
        );
    QCOMPARE(
        static_cast<int>(fileDialogs.saveFileRequests.constFirst().purpose),
        static_cast<int>(FileDialogPurpose::TeacherProfile)
        );
}

void FileControllerWorkspaceLifecycleTests::exportPreservesDialogPolicyAndNoOpenGuard()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString sourcePath = workspaceRoot.filePath(
        QStringLiteral("dialog-policy-source.tps")
        );
    const QString firstDestination = workspaceRoot.filePath(
        QStringLiteral("dialog-policy-export")
        );
    const QString secondDestination = workspaceRoot.filePath(
        QStringLiteral("dialog-policy-no-open")
        );

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(sourcePath));
    seedServices.closeDatabase();

    FakeFileDialogService fileDialogs;
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(firstDestination)
        );
    fileDialogs.scriptedSaveFiles.enqueue(
        std::optional<QString>(secondDestination)
        );
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ApplicationServices services;
    QVERIFY(services.openDatabase(sourcePath));
    FileController controller(&services, nullptr);
    ActionRegistry actions;
    connectFileActions(controller, actions);

    actions.exportAsFile->trigger();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QFileInfo::exists(firstDestination + QStringLiteral(".tps")));
    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().title,
        QStringLiteral("Export Teacher Profile As")
        );
    QCOMPARE(
        fileDialogs.saveFileRequests.constFirst().defaultSuffix,
        QStringLiteral("tps")
        );

    // The public action keeps the existing no-open guard and must not even
    // consume a queued selection once the compatibility service is closed.
    services.closeDatabase();
    actions.exportAsFile->trigger();

    QCOMPARE(fileDialogs.saveFileRequests.size(), 1);
    QVERIFY(!QFileInfo::exists(secondDestination + QStringLiteral(".tps")));
    QVERIFY(prompts.messages.isEmpty());
}

void FileControllerWorkspaceLifecycleTests::saveAsMigratesV2AndExportUsesCoordinator()
{
    const QString source = fileControllerSource();
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral("m_workspaceCoordinator->saveWorkspace()")));
    QVERIFY(source.contains(QStringLiteral("m_services->saveDatabase();")));
    QVERIFY(source.contains(QStringLiteral("m_workspaceCoordinator->saveWorkspaceAs(")));
    QVERIFY(source.contains(QStringLiteral("m_services->saveDatabaseAs(normalized);")));
    QVERIFY(source.contains(QStringLiteral("m_workspaceCoordinator->exportWorkspace(")));
    QVERIFY(source.contains(QStringLiteral("m_services->exportDatabaseAs(normalized);")));
    QVERIFY(source.contains(QStringLiteral("domainErrorMessage(exported.error())")));
    QVERIFY(source.contains(QStringLiteral("rememberDatabaseDirectory(returnedDestination)")));
}

QTEST_MAIN(FileControllerWorkspaceLifecycleTests)

#include "file_controller_workspace_lifecycle_tests.moc"
