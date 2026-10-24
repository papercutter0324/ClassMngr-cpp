#include "app/controllers/file_controller.h"
#include "app/controllers/file_controller_workspace_messages.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/database_file_format.h"
#include "core/result.h"
#include "data/data_service.h"
#include "next/application/initial_setup_lifecycle.h"
#include "next/application/recent_workspace_history.h"
#include "next/application/recent_workspace_history_use_case.h"
#include "next/application/workspace_creation_lifecycle.h"
#include "next/application/workspace_coordinator.h"
#include "next/platform/initial_setup_lifecycle_adapter.h"
#include "next/platform/application_services_workspace_port.h"
#include "next/platform/legacy_workspace_gateway.h"
#include "next/platform/settings_manager_last_database_directory_port.h"
#include "next/platform/settings_manager_recent_workspace_history_port.h"
#include "next/platform/workspace_creation_lifecycle_adapter.h"
#include "ui/shared/dialogs/file_dialog_service.h"

#include <QAction>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QStandardPaths>

#include <cstddef>
#include <string>

namespace
{
QString domainErrorMessage(
    const ClassMngr::Next::Domain::OperationError& error
    )
{
    return QString::fromUtf8(
        error.message.data(),
        static_cast<qsizetype>(error.message.size())
        );
}

std::string utf8Path(
    const QString& path
    )
{
    const QByteArray encoded = path.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

QString qtPath(
    const ClassMngr::Next::Application::RecentWorkspacePath& path
    )
{
    return QString::fromUtf8(
        path.value().data(),
        static_cast<qsizetype>(path.value().size())
        );
}

QString qtWorkspacePath(
    const ClassMngr::Next::Application::WorkspaceLocation& path
    )
{
    return QString::fromUtf8(
        path.value().data(),
        static_cast<qsizetype>(path.value().size())
        );
}

bool sameProfilePath(
    const QString& leftPath,
    const QString& rightPath
    )
{
    const QFileInfo leftInfo(leftPath);
    const QFileInfo rightInfo(rightPath);
    const QString leftCanonicalPath = leftInfo.canonicalFilePath();
    const QString rightCanonicalPath = rightInfo.canonicalFilePath();
    const QString comparableLeftPath =
        QDir::cleanPath(
            leftCanonicalPath.isEmpty()
                ? leftInfo.absoluteFilePath()
                : leftCanonicalPath
            );
    const QString comparableRightPath =
        QDir::cleanPath(
            rightCanonicalPath.isEmpty()
                ? rightInfo.absoluteFilePath()
                : rightCanonicalPath
            );

    return comparableLeftPath == comparableRightPath;
}
}

QString ClassMngr::App::Controllers::
newTeacherProfileCreationFailureMessage(
    const ClassMngr::Next::Application::WorkspaceCreationLifecycleResult& result,
    const QString& selectedPath
    )
{
    QString message = QString::fromUtf8(
        result.error.message.data(),
        static_cast<qsizetype>(result.error.message.size())
        );

    if (!result.workspaceClosed)
    {
        message += QCoreApplication::translate(
            "FileController",
            "\n\nThe failed workspace could not be closed."
            );
    }

    if (!result.originalRecoveryPath.empty())
    {
        const QString recoveryPath = QString::fromUtf8(
            result.originalRecoveryPath.value().data(),
            static_cast<qsizetype>(
                result.originalRecoveryPath.value().size()
                )
            );
        message += QCoreApplication::translate(
            "FileController",
            "\n\nThe original Teacher Profile could not be restored "
            "to the selected path. Its recovery copy is at:\n%1"
            ).arg(recoveryPath);
    }

    if (!result.partialRecoveryPath.empty())
    {
        const QString recoveryPath = QString::fromUtf8(
            result.partialRecoveryPath.value().data(),
            static_cast<qsizetype>(
                result.partialRecoveryPath.value().size()
                )
            );
        message += QCoreApplication::translate(
            "FileController",
            "\n\nThe incomplete replacement was retained at:\n%1"
            ).arg(recoveryPath);
    }
    else if (result.partialReplacementRemainsAtTarget)
    {
        message += QCoreApplication::translate(
            "FileController",
            "\n\nThe incomplete replacement remains at:\n%1"
            ).arg(selectedPath);
    }

    return message;
}

FileController::FileController(
    ApplicationServices* services,
    MainWindow* window,
    QObject* parent,
    ClassMngr::Next::Application::LastDatabaseDirectoryPort*
        lastDatabaseDirectoryPort,
    ClassMngr::Next::Application::RecentWorkspaceHistoryPort*
        recentWorkspaceHistoryPort
    )
    : QObject(parent)
    , m_services(services)
    , m_window(window)
    , m_lastDatabaseDirectoryPort(lastDatabaseDirectoryPort)
    , m_recentWorkspaceHistoryPort(recentWorkspaceHistoryPort)
{
    if (!m_lastDatabaseDirectoryPort)
    {
        m_ownedLastDatabaseDirectoryPort =
            std::make_unique<
                ClassMngr::Next::Platform::
                    SettingsManagerLastDatabaseDirectoryPort
                >();
        m_lastDatabaseDirectoryPort =
            m_ownedLastDatabaseDirectoryPort.get();
    }

    if (!m_recentWorkspaceHistoryPort)
    {
        m_ownedRecentWorkspaceHistoryPort =
            std::make_unique<
                ClassMngr::Next::Platform::
                    SettingsManagerRecentWorkspaceHistoryPort
                >();
        m_recentWorkspaceHistoryPort =
            m_ownedRecentWorkspaceHistoryPort.get();
    }

    if (!m_services)
    {
        return;
    }

    m_workspacePort =
        std::make_unique<
            ClassMngr::Next::Platform::ApplicationServicesWorkspacePort
            >(*m_services);

    m_workspaceGateway =
        std::make_unique<
            ClassMngr::Next::Platform::LegacyWorkspaceGateway
            >(*m_workspacePort);

    m_workspaceUseCase =
        std::make_unique<
            ClassMngr::Next::Application::WorkspaceUseCase
            >(*m_workspaceGateway);

    m_workspaceState =
        std::make_unique<
            ClassMngr::Next::Application::WorkspaceState
            >();

    m_selectionState =
        std::make_unique<
            ClassMngr::Next::Application::SelectionState
            >();

    m_workspaceCoordinator =
        std::make_unique<
            ClassMngr::Next::Application::WorkspaceCoordinator
            >(
                *m_workspaceUseCase,
                *m_workspaceState,
                *m_selectionState
                );

    m_initialSetupLifecycleAdapter =
        std::make_unique<
            ClassMngr::Next::Platform::InitialSetupLifecycleAdapter
            >(
                *m_services,
                *m_workspaceCoordinator,
                *m_workspaceState
                );

    m_initialSetupLifecycle =
        std::make_unique<
            ClassMngr::Next::Application::InitialSetupLifecycle
            >(*m_initialSetupLifecycleAdapter);

    m_workspaceCreationLifecycleAdapter =
        std::make_unique<
            ClassMngr::Next::Platform::WorkspaceCreationLifecycleAdapter
            >(
                *m_services,
                *m_workspaceCoordinator,
                *m_workspaceState
                );

    m_workspaceCreationLifecycle =
        std::make_unique<
            ClassMngr::Next::Application::WorkspaceCreationLifecycle
            >(*m_workspaceCreationLifecycleAdapter);
}

FileController::~FileController() = default;

void FileController::connectActions(
    ActionRegistry& actions
    )
{
    m_actions = &actions;

    connect(
        actions.newFile,
        &QAction::triggered,
        this,
        &FileController::newFile
        );

    connect(
        actions.openFile,
        &QAction::triggered,
        this,
        &FileController::openFile
        );

    connect(
        actions.recoverInterruptedProfile,
        &QAction::triggered,
        this,
        &FileController::recoverInterruptedProfile
        );

    connect(
        actions.saveFile,
        &QAction::triggered,
        this,
        &FileController::saveFile
        );

    connect(
        actions.saveAsFile,
        &QAction::triggered,
        this,
        &FileController::saveAsFile
        );

    connect(
        actions.exportAsFile,
        &QAction::triggered,
        this,
        &FileController::exportAsFile
        );

    connect(
        actions.closeFile,
        &QAction::triggered,
        this,
        &FileController::closeFile
        );

    if (actions.saveModeState)
    {
        actions.saveModeState->onChanged =
            [this](SaveMode mode)
        {
            setSaveMode(mode);
        };
    }
}

void FileController::setSaveMode(
    SaveMode mode
    )
{
    switch (mode)
    {
    case SaveMode::Automatic:
        // TODO:
        // Enable autosave manager
        break;

    case SaveMode::Manual:
        // TODO:
        // Disable autosave manager
        break;
    }
}

bool FileController::confirmUnsavedChanges(
    bool exiting
    )
{
    return !m_window
        || m_window->confirmCurrentPageCanLeave(exiting);
}

void FileController::loadMostRecentDatabase()
{
    const QString recentPath =
        mostRecentDatabasePath();

    if (recentPath.isEmpty())
    {
        enterNoDatabaseState();
        return;
    }

    const QString normalizedPath =
        normalizeInputFilePath(recentPath);

    if (!QFileInfo::exists(normalizedPath))
    {
        pruneRecentFile(recentPath);
        populateRecentMenu();

        if (!m_services || !m_services->hasOpenDatabase())
        {
            enterNoDatabaseState();
        }
        return;
    }

    if (
        !loadDatabase(recentPath, false)
        && (!m_services || !m_services->hasOpenDatabase())
        )
    {
        enterNoDatabaseState();
    }
}

void FileController::loadDatabaseOnStartup(
    const QString& filePath
    )
{
    if (filePath.trimmed().isEmpty())
    {
        enterNoDatabaseState();
        return;
    }

    loadDatabase(filePath);
}

void FileController::newFile()
{
    (void) createNewDatabaseInteractive();
}

bool FileController::createNewDatabaseInteractive()
{
    return createNewDatabaseInteractive(false);
}

bool FileController::createInitialSetupDatabaseInteractive()
{
    return createNewDatabaseInteractive(true);
}

bool FileController::createNewDatabaseInteractive(
    bool forInitialSetup
    )
{
    if (!confirmUnsavedChanges())
        return false;

    const std::optional<QString> selection =
        DialogServices::fileDialogs().saveFile(
            SaveFileRequest{
                .parent = m_window,
                .title = tr("New Teacher Profile"),
                .purpose = FileDialogPurpose::TeacherProfile,
                .initialDirectory = databaseDialogDirectory(),
                .nameFilters = {
                    tr("ClassMngr Teacher Profile (*.tps)")
                },
                .defaultSuffix = QStringLiteral("tps")
            }
            );

    if (!selection)
        return false;

    const QString normalizedPath =
        normalizeNativeOutputFilePath(*selection);

    if (forInitialSetup)
    {
        return createInitialSetupDatabase(normalizedPath);
    }

    if (!m_services)
    {
        enterNoDatabaseState();
        return false;
    }

    const QString previousProfilePath =
        !m_currentFile.trimmed().isEmpty()
            ? m_currentFile
            : (
                m_services->hasOpenDatabase()
                    ? m_services->currentDatabasePath()
                    : QString()
                );
    const bool replacingPreviouslyOpenProfile =
        !previousProfilePath.trimmed().isEmpty()
        && sameProfilePath(previousProfilePath, normalizedPath);

    if (!closeActiveDatabase())
    {
        return false;
    }

    const auto created = m_workspaceCreationLifecycle->createReplacingProfile(
        ClassMngr::Next::Application::WorkspaceLocation(
            utf8Path(normalizedPath)
            )
        );

    if (!created.created)
    {
        QString failureMessage =
            ClassMngr::App::Controllers::
                newTeacherProfileCreationFailureMessage(
                    created,
                    normalizedPath
                    );

        const bool samePathProfileWasRestored =
            !replacingPreviouslyOpenProfile
            || (
                created.originalProfileRestored
                && !created.partialReplacementRemainsAtTarget
                );
        const bool mayReopenPreviousProfile =
            !previousProfilePath.trimmed().isEmpty()
            && created.workspaceClosed
            && samePathProfileWasRestored;

        if (mayReopenPreviousProfile)
        {
            if (!loadDatabase(previousProfilePath, false))
            {
                failureMessage += tr(
                    "\n\nThe previous Teacher Profile could not be reopened "
                    "automatically:\n%1\nUse Open Teacher Profile to reopen it."
                    ).arg(previousProfilePath);
                enterNoDatabaseState();
            }
        }
        else
        {
            enterNoDatabaseState();
        }

        DialogServices::showWarning(
            m_window,
            tr("New Teacher Profile"),
            failureMessage
            );

        return false;
    }

    m_currentFile = qtWorkspacePath(created.createdProfile);

    updateRecentFiles(m_currentFile);

    if (m_window)
    {
        m_window->applyDatabaseLoadedState();
    }
    else
    {
        setLoadedFileState();
    }

    if (!created.originalRecoveryPath.empty())
    {
        DialogServices::showWarning(
            m_window,
            tr("New Teacher Profile"),
            tr(
                "The new Teacher Profile was created, but the replaced "
                "original could not be removed. Its recovery copy is at:\n%1"
                ).arg(qtWorkspacePath(created.originalRecoveryPath))
            );
    }

    return true;
}

bool FileController::createInitialSetupDatabase(
    const QString& filePath
    )
{
    if (!m_services || !m_initialSetupLifecycle)
    {
        enterNoDatabaseState();
        return false;
    }

    const auto started = m_initialSetupLifecycle->begin(
        ClassMngr::Next::Application::WorkspaceLocation(
            utf8Path(filePath)
            )
        );

    if (started.workspaceClosed)
    {
        m_currentFile.clear();
        if (started.workspaceWasOpen && m_window)
        {
            m_window->clearDatabaseBackedState();
        }
    }

    if (!started.started())
    {
        using ClassMngr::Next::Application::InitialSetupBeginFailure;
        switch (started.failure)
        {
        case InitialSetupBeginFailure::CloseWorkspace:
            return false;

        case InitialSetupBeginFailure::PreserveProfile:
            DialogServices::showWarning(
                m_window,
                tr("New Teacher Profile"),
                tr("Unable to preserve the existing Teacher Profile file:\n%1")
                    .arg(filePath)
                );
            enterNoDatabaseState();
            return false;

        case InitialSetupBeginFailure::CreateWorkspace:
            showInitialSetupCancelWarning(started.rollback);
            if (started.rollback.workspaceClosed)
            {
                enterNoDatabaseState();
            }
            DialogServices::showWarning(
                m_window,
                tr("New Teacher Profile"),
                domainErrorMessage(started.error)
                );
            return false;

        case InitialSetupBeginFailure::AlreadyActive:
        case InitialSetupBeginFailure::None:
            DialogServices::showWarning(
                m_window,
                tr("New Teacher Profile"),
                domainErrorMessage(started.error)
                );
            return false;
        }
    }

    m_currentFile = QString::fromUtf8(
        started.createdProfile.value().data(),
        static_cast<qsizetype>(started.createdProfile.value().size())
        );

    if (m_window)
    {
        m_window->applyDatabaseLoadedState();
    }
    else
    {
        setLoadedFileState();
    }

    return true;
}

void FileController::finishInitialSetup()
{
    if (!m_initialSetupLifecycle)
    {
        return;
    }

    const auto finished = m_initialSetupLifecycle->finish();
    if (finished.originalBackupRemovalFailed)
    {
        const QString backupPath = QString::fromUtf8(
            finished.originalBackup.value().data(),
            static_cast<qsizetype>(finished.originalBackup.value().size())
            );
        DialogServices::showWarning(
            m_window,
            tr("Initial Setup"),
            tr("Setup is complete, but the replaced Teacher Profile could not be removed:\n%1")
                .arg(backupPath)
            );
    }

    if (finished.wasActive && !m_currentFile.isEmpty())
    {
        updateRecentFiles(m_currentFile);
    }
}

void FileController::cancelInitialSetup()
{
    if (!m_initialSetupLifecycle)
    {
        return;
    }

    const auto canceled = m_initialSetupLifecycle->cancel();
    if (!canceled.workspaceClosed)
    {
        return;
    }

    m_currentFile.clear();
    if (canceled.workspaceWasOpen && m_window)
    {
        m_window->clearDatabaseBackedState();
    }

    showInitialSetupCancelWarning(canceled);
    enterNoDatabaseState();
}

void FileController::showInitialSetupCancelWarning(
    const ClassMngr::Next::Application::InitialSetupCancelResult& result
    )
{
    using ClassMngr::Next::Application::InitialSetupCancelStatus;

    if (
        result.status
        == InitialSetupCancelStatus::IncompleteProfileRemovalFailed
        )
    {
        const QString profilePath = QString::fromUtf8(
            result.profile.value().data(),
            static_cast<qsizetype>(result.profile.value().size())
            );
        DialogServices::showWarning(
            m_window,
            tr("Initial Setup"),
            tr("The incomplete Teacher Profile could not be removed:\n%1")
                .arg(profilePath)
            );
    }
    else if (
        result.status
        == InitialSetupCancelStatus::OriginalRestoredRecoveryRemovalFailed
        )
    {
        const QString incompleteRecoveryPath = QString::fromUtf8(
            result.incompleteRecovery.value().data(),
            static_cast<qsizetype>(
                result.incompleteRecovery.value().size()
                )
            );
        DialogServices::showWarning(
            m_window,
            tr("Initial Setup"),
            tr(
                "The original Teacher Profile was restored, but the "
                "incomplete setup copy could not be removed. It remains at:\n%1"
                )
                .arg(incompleteRecoveryPath)
            );
    }
    else if (
        result.status == InitialSetupCancelStatus::OriginalProfileRestoreFailed
        )
    {
        const QString backupPath = QString::fromUtf8(
            result.originalBackup.value().data(),
            static_cast<qsizetype>(result.originalBackup.value().size())
            );
        if (!result.incompleteRecovery.empty())
        {
            const QString incompleteRecoveryPath = QString::fromUtf8(
                result.incompleteRecovery.value().data(),
                static_cast<qsizetype>(
                    result.incompleteRecovery.value().size()
                    )
                );
            DialogServices::showWarning(
                m_window,
                tr("Initial Setup"),
                tr(
                    "The original Teacher Profile could not be restored."
                    "\n\nOriginal backup path:\n%1"
                    "\n\nIncomplete Teacher Profile recovery path:\n%2"
                    )
                    .arg(backupPath, incompleteRecoveryPath)
                );
        }
        else
        {
            DialogServices::showWarning(
                m_window,
                tr("Initial Setup"),
                tr("The original Teacher Profile could not be restored:\n%1")
                    .arg(backupPath)
                );
        }
    }
}

void FileController::openFile()
{
    if (!confirmUnsavedChanges())
        return;

    const std::optional<QString> selection =
        DialogServices::fileDialogs().openFile(
            OpenFileRequest{
                .parent = m_window,
                .title = tr("Open Teacher Profile"),
                .purpose = FileDialogPurpose::TeacherProfile,
                .initialDirectory = databaseDialogDirectory(),
                .nameFilters = {
                    tr("ClassMngr Teacher Profile (*.tps)"),
                    tr("Legacy Teacher Profile (*.db)")
                }
            }
            );

    if (!selection)
        return;

    loadDatabase(*selection);
}

void FileController::recoverInterruptedProfile()
{
    if (!m_workspaceCreationLifecycleAdapter)
    {
        DialogServices::showWarning(
            m_window,
            tr("Recover Interrupted Teacher Profile"),
            tr("Teacher Profile recovery is not available right now.")
            );
        return;
    }

    const QString recoveryDirectory = databaseDialogDirectory();
    auto candidates =
        m_workspaceCreationLifecycleAdapter
            ->normalCreateRecoveryCandidatesIn(recoveryDirectory);
    candidates += m_workspaceCreationLifecycleAdapter
                     ->initialSetupRecoveryCandidatesIn(recoveryDirectory);
    if (candidates.isEmpty())
    {
        DialogServices::showInformation(
            m_window,
            tr("Recover Interrupted Teacher Profile"),
            tr(
                "No interrupted profile recovery copies were found in:\n%1"
                )
                .arg(recoveryDirectory)
            );
        return;
    }

    ActionPromptRequest candidateRequest;
    candidateRequest.prompt.parent = m_window;
    candidateRequest.prompt.objectName =
        QStringLiteral("recoverInterruptedProfileCandidatePrompt");
    candidateRequest.prompt.automationId =
        QStringLiteral("recoverInterruptedProfileCandidatePrompt");
    candidateRequest.prompt.title =
        tr("Recover Interrupted Teacher Profile");
    candidateRequest.prompt.message = tr(
        "Choose a recovery copy to copy into a Teacher Profile file. "
        "The recovery copy will remain unchanged."
        );
    candidateRequest.informativeText = tr(
        "Candidates were found in:\n%1\n\n"
        "Target and recovery-copy paths:"
        ).arg(recoveryDirectory);
    candidateRequest.defaultActionId = QStringLiteral("cancel");
    candidateRequest.escapeActionId = QStringLiteral("cancel");

    for (qsizetype index = 0; index < candidates.size(); ++index)
    {
        const auto& candidate = candidates.at(index);
        const QString targetName =
            QFileInfo(candidate.targetPath).fileName();
        const QString recoveryName =
            QFileInfo(candidate.sourcePath).fileName();
        candidateRequest.informativeText += QStringLiteral("\n\n%1\n%2\n%3")
            .arg(targetName, candidate.targetPath, candidate.sourcePath);
        candidateRequest.actions.append(
            PromptAction{
                .id = QStringLiteral("candidate-%1").arg(index),
                .text = tr("%1 — %2").arg(targetName, recoveryName),
                .role = PromptActionRole::Action,
                .enabled = true
            }
            );
    }
    candidateRequest.actions.append(
        PromptAction{
            .id = QStringLiteral("cancel"),
            .text = tr("Cancel"),
            .role = PromptActionRole::Reject,
            .enabled = true
        }
        );

    const QString selectedAction =
        DialogServices::prompts().chooseAction(candidateRequest);
    bool selectedIndexValid = false;
    const qsizetype selectedIndex =
        selectedAction.startsWith(QStringLiteral("candidate-"))
            ? selectedAction.mid(QStringLiteral("candidate-").size())
                  .toLongLong(&selectedIndexValid)
            : -1;
    if (
        !selectedIndexValid
        || selectedIndex < 0
        || selectedIndex >= candidates.size()
    )
    {
        return;
    }

    const auto& candidate = candidates.at(selectedIndex);
    const QFileInfo targetInfo(candidate.targetPath);
    const QString suggestedName = targetInfo.exists()
        ? tr("Recovered %1").arg(targetInfo.fileName())
        : targetInfo.fileName();

    QString destinationPath;
    while (true)
    {
        const auto destinationSelection = DialogServices::fileDialogs().saveFile(
            SaveFileRequest{
                .parent = m_window,
                .title = tr("Save Recovered Teacher Profile"),
                .purpose = FileDialogPurpose::TeacherProfile,
                .initialDirectory = QFileInfo(candidate.sourcePath)
                    .absolutePath(),
                .suggestedFileName = suggestedName,
                .nameFilters = {
                    tr("ClassMngr Teacher Profile (*.tps)")
                },
                .defaultSuffix = QStringLiteral("tps"),
                .confirmOverwrite = false
            }
            );
        if (!destinationSelection)
        {
            return;
        }

        // Do not create missing parent directories here. A canceled or
        // rejected recovery choice must not change the filesystem.
        destinationPath = QDir::cleanPath(
            QFileInfo(
                DatabaseFileFormat::nativeOutputPath(*destinationSelection)
                ).absoluteFilePath()
            );
        if (destinationPath.trimmed().isEmpty())
        {
            DialogServices::showWarning(
                m_window,
                tr("Recover Interrupted Teacher Profile"),
                tr("Choose a valid destination for the recovered profile.")
                );
            continue;
        }

        if (!QFileInfo::exists(destinationPath))
        {
            break;
        }

        DialogServices::showWarning(
            m_window,
            tr("Recover Interrupted Teacher Profile"),
            tr(
                "The destination already exists and was left unchanged. "
                "Choose a different path:\n%1"
                ).arg(destinationPath)
            );
    }

    if (!confirmUnsavedChanges())
    {
        return;
    }

    if (
        !m_workspaceCreationLifecycleAdapter
             ->copyRecoveryCandidateNoReplace(
                 candidate.sourcePath,
                 destinationPath
                 )
    )
    {
        DialogServices::showError(
            m_window,
            tr("Recover Interrupted Teacher Profile"),
            tr(
                "The recovery copy could not be copied. The source remains "
                "at:\n%1\n\nThe destination, if created, remains at:\n%2"
                ).arg(candidate.sourcePath, destinationPath)
            );
        return;
    }

    QString openFailure;
    if (!loadDatabase(destinationPath, false, &openFailure))
    {
        DialogServices::showError(
            m_window,
            tr("Recover Interrupted Teacher Profile"),
            tr(
                "The copied profile could not be opened. The recovery copy "
                "remains at:\n%1\n\nThe original recovery file remains "
                "at:\n%2\n\n%3"
                ).arg(destinationPath, candidate.sourcePath, openFailure)
            );
    }
}

bool FileController::loadDatabase(
    const QString& filePath,
    bool showErrorMessage,
    QString* failureMessage
    )
{
    if (failureMessage)
    {
        failureMessage->clear();
    }

    const QString normalizedPath =
        normalizeInputFilePath(filePath);

    if (!QFileInfo::exists(normalizedPath))
    {
        if (failureMessage)
        {
            *failureMessage = tr("File not found:\n%1").arg(normalizedPath);
        }
        pruneRecentFile(filePath);
        populateRecentMenu();

        if (showErrorMessage)
        {
            DialogServices::showWarning(
                m_window,
                tr("Missing File"),
                tr("File not found:\n%1")
                    .arg(normalizedPath)
                );
        }

        if (!m_services || !m_services->hasOpenDatabase())
        {
            enterNoDatabaseState();
        }
        return false;
    }

    if (!m_services)
    {
        if (failureMessage)
        {
            *failureMessage = tr("Teacher Profile services are unavailable.");
        }
        enterNoDatabaseState();
        return false;
    }

    const auto opened =
        m_workspaceCoordinator->openWorkspace(
            ClassMngr::Next::Application::OpenWorkspaceRequest{
                ClassMngr::Next::Application::WorkspaceLocation(
                    normalizedPath.toUtf8().toStdString()
                    )
            }
            );

    if (!opened)
    {
        if (failureMessage)
        {
            *failureMessage = domainErrorMessage(opened.error());
        }
        if (showErrorMessage)
        {
            DialogServices::showWarning(
                m_window,
                tr("Open Teacher Profile"),
                domainErrorMessage(opened.error())
                );
        }

        if (!m_services->hasOpenDatabase())
        {
            enterNoDatabaseState();
        }
        return false;
    }

    m_currentFile = qtWorkspacePath(opened.value().location());

    updateRecentFiles(filePath);

    if (m_window)
    {
        m_window->applyDatabaseLoadedState();
    }
    else
    {
        setLoadedFileState();
    }

    return true;
}

void FileController::saveFile()
{
    if (
        !m_services
        || !m_services->hasOpenDatabase()
        )
    {
        return;
    }

    if (m_currentFile.isEmpty())
    {
        saveAsFile();
        return;
    }

    saveDatabase();
}

void FileController::saveAsFile()
{
    if (
        !m_services
        || !m_services->hasOpenDatabase()
        )
    {
        return;
    }

    const std::optional<QString> selection =
        DialogServices::fileDialogs().saveFile(
            SaveFileRequest{
                .parent = m_window,
                .title = tr("Save Teacher Profile"),
                .purpose = FileDialogPurpose::TeacherProfile,
                .initialDirectory = databaseDialogDirectory(),
                .nameFilters = {
                    tr("ClassMngr Teacher Profile (*.tps)")
                },
                .defaultSuffix = QStringLiteral("tps")
            }
            );

    if (!selection)
        return;

    saveDatabaseAs(*selection);
}

void FileController::exportAsFile()
{
    if (
        !m_services
        || !m_services->hasOpenDatabase()
        )
    {
        return;
    }

    const std::optional<QString> selection =
        DialogServices::fileDialogs().saveFile(
            SaveFileRequest{
                .parent = m_window,
                .title = tr("Export Teacher Profile As"),
                .purpose = FileDialogPurpose::TeacherProfile,
                .initialDirectory = databaseDialogDirectory(),
                .nameFilters = {
                    tr("ClassMngr Teacher Profile (*.tps)")
                },
                .defaultSuffix = QStringLiteral("tps")
            }
            );

    if (!selection)
        return;

    exportDatabaseAs(*selection);
}

void FileController::closeFile()
{
    if (!confirmUnsavedChanges())
        return;

    if (!closeActiveDatabase())
    {
        return;
    }

    enterNoDatabaseState();
}

void FileController::openSpecificFile(
    const QString& filePath
    )
{
    if (!confirmUnsavedChanges())
        return;

    const QString normalizedPath =
        normalizeInputFilePath(filePath);

    if (!QFileInfo::exists(normalizedPath))
    {
        pruneRecentFile(filePath);
        populateRecentMenu();

        DialogServices::showWarning(
            m_window,
            tr("Missing File"),
            tr("File not found:\n%1")
                .arg(normalizedPath)
            );

        return;
    }

    loadDatabase(filePath);
}

void FileController::updateRecentFiles(
    const QString& filePath
    )
{
    const QString normalizedPath =
        normalizeInputFilePath(filePath);

    ClassMngr::Next::Application::RecentWorkspaceHistory history =
        m_recentWorkspaceHistoryPort->load();

    const ClassMngr::Next::Application::RecentWorkspacePath rawPath(
        utf8Path(filePath)
        );
    const ClassMngr::Next::Application::RecentWorkspacePath normalizedPathValue(
        utf8Path(normalizedPath)
        );

    history =
        ClassMngr::Next::Application::RecentWorkspaceHistoryUseCase::record(
            history,
            rawPath,
            normalizedPathValue
            );
    const auto saved = m_recentWorkspaceHistoryPort->save(history);
    if (!saved)
    {
        DialogServices::showWarning(
            m_window,
            tr("Recent Files"),
            tr("The recent workspace history could not be saved."),
            domainErrorMessage(saved.error())
            );
    }

    rememberDatabaseDirectory(normalizedPath);

    populateRecentMenu();
}

void FileController::populateRecentMenu()
{
    if (
        !m_actions
        || !m_actions->recentFilesMenu
        )
    {
        return;
    }

    auto* menu =
        m_actions->recentFilesMenu;

    menu->clear();

    const ClassMngr::Next::Application::RecentWorkspaceHistory history =
        m_recentWorkspaceHistoryPort->load();

    if (history.paths.empty())
    {
        auto* emptyAction =
            menu->addAction(
                tr("(No Recent Files)")
                );

        emptyAction->setEnabled(false);
        return;
    }

    for (
        qsizetype index = 0;
        index < static_cast<qsizetype>(history.paths.size());
        ++index
        )
    {
        const QString filePath =
            qtPath(history.paths.at(static_cast<std::size_t>(index)));

        const QString nativePath =
            QDir::toNativeSeparators(filePath);

        QString displayPath =
            nativePath;

        displayPath.replace(
            QLatin1Char('&'),
            QStringLiteral("&&")
            );

        auto* fileAction =
            menu->addAction(
                tr("&%1 %2")
                    .arg(index + 1)
                    .arg(displayPath)
                );

        fileAction->setToolTip(nativePath);
        fileAction->setData(filePath);

        connect(
            fileAction,
            &QAction::triggered,
            this,
            [this, filePath]()
            {
                openSpecificFile(filePath);
            }
            );
    }

    menu->addSeparator();

    auto* clearAction =
        menu->addAction(
            tr("Clear Recent Files")
            );

    connect(
        clearAction,
        &QAction::triggered,
        this,
        &FileController::clearRecentFiles
        );
}

void FileController::clearRecentFiles()
{
    const auto cleared = m_recentWorkspaceHistoryPort->clear();

    populateRecentMenu();
    if (!cleared)
    {
        DialogServices::showWarning(
            m_window,
            tr("Recent Files"),
            tr("The recent workspace history could not be cleared."),
            domainErrorMessage(cleared.error())
            );
    }
}

void FileController::autosave()
{
    saveDatabase();
}

void FileController::saveDatabase()
{
    if (
        !m_services
        || !m_services->hasOpenDatabase()
        )
    {
        return;
    }

    if (
        m_workspaceState
        && m_workspaceState->snapshot().session().has_value()
        )
    {
        if (!m_workspaceCoordinator)
        {
            return;
        }

        const auto saved =
            m_workspaceCoordinator->saveWorkspace();

        if (!saved)
        {
            DialogServices::showWarning(
                m_window,
                tr("Save Teacher Profile"),
                domainErrorMessage(saved.error())
                );
        }

        return;
    }

    m_services
        ->saveDatabase();
}

bool FileController::saveDatabaseAs(
    const QString& filePath
    )
{
    const QString normalized =
        normalizeNativeOutputFilePath(filePath);

    if (
        !m_services
        || !m_services->hasOpenDatabase()
        )
    {
        return false;
    }

    if (
        m_workspaceState
        && m_workspaceState->snapshot().session().has_value()
        )
    {
        if (!m_workspaceCoordinator)
        {
            return false;
        }

        const auto saved =
            m_workspaceCoordinator->saveWorkspaceAs(
                ClassMngr::Next::Application::WorkspaceLocation(
                    normalized.toUtf8().toStdString()
                    )
                );

        if (!saved)
        {
            DialogServices::showWarning(
                m_window,
                tr("Save Teacher Profile"),
                domainErrorMessage(saved.error())
                );

            return false;
        }

        m_currentFile = normalizeInputFilePath(
            QString::fromUtf8(
                saved.value().value().data(),
                static_cast<qsizetype>(saved.value().value().size())
                )
            );

        updateRecentFiles(m_currentFile);

        if (m_window)
        {
            m_window->applyDatabaseLoadedState();
        }
        else
        {
            setLoadedFileState();
        }

        return true;
    }

    const Status saved =
        m_services
        ->saveDatabaseAs(normalized);

    if (!saved)
    {
        DialogServices::showWarning(
            m_window,
            tr("Save Teacher Profile"),
            saved.error()
            );

        return false;
    }

    return loadDatabase(normalized);
}

bool FileController::exportDatabaseAs(
    const QString& filePath
    )
{
    const QString normalized =
        normalizeNativeOutputFilePath(filePath);

    if (
        !m_services
        || !m_services->hasOpenDatabase()
        )
    {
        return false;
    }

    if (
        m_workspaceState
        && m_workspaceState->snapshot().session().has_value()
        )
    {
        if (!m_workspaceCoordinator)
        {
            return false;
        }

        const auto exported =
            m_workspaceCoordinator->exportWorkspace(
                ClassMngr::Next::Application::WorkspaceLocation(
                    normalized.toUtf8().toStdString()
                    )
                );

        if (!exported)
        {
            DialogServices::showWarning(
                m_window,
                tr("Export Teacher Profile"),
                domainErrorMessage(exported.error())
                );

            return false;
        }

        const QString returnedDestination =
            QString::fromUtf8(
                exported.value().value().data(),
                static_cast<qsizetype>(exported.value().value().size())
                );
        rememberDatabaseDirectory(returnedDestination);
        return true;
    }

    const Status exported =
        m_services
        ->exportDatabaseAs(normalized);

    if (!exported)
    {
        DialogServices::showWarning(
            m_window,
            tr("Export Teacher Profile"),
            exported.error()
            );

        return false;
    }

    if (exported)
    {
        rememberDatabaseDirectory(normalized);
    }

    return true;
}

bool FileController::closeActiveDatabase()
{
    if (
        m_workspaceState
        && m_workspaceState->snapshot().session().has_value()
        )
    {
        if (!m_workspaceCoordinator)
        {
            return false;
        }

        const auto closed =
            m_workspaceCoordinator->closeWorkspace();

        if (!closed)
        {
            return false;
        }

        m_currentFile.clear();

        if (m_window)
        {
            m_window->clearDatabaseBackedState();
        }

        return true;
    }

    const bool hadOpenDatabase =
        m_services
        && m_services->hasOpenDatabase();

    if (m_services)
    {
        m_services->closeDatabase();
    }

    m_currentFile.clear();

    if (hadOpenDatabase && m_window)
    {
        m_window->clearDatabaseBackedState();
    }

    return true;
}

QString FileController::normalizeInputFilePath(
    const QString& filePath
    ) const
{
    return absoluteFilePath(
        DatabaseFileFormat::supportedInputPath(
            filePath
            ),
        false
        );
}

QString FileController::normalizeNativeOutputFilePath(
    const QString& filePath
    ) const
{
    return absoluteFilePath(
        DatabaseFileFormat::nativeOutputPath(
            filePath
            ),
        true
        );
}

QString FileController::absoluteFilePath(
    const QString& filePath,
    bool createDirectories
    ) const
{
    QFileInfo info(filePath);

    if (
        createDirectories
        && !info.absolutePath().isEmpty()
        )
    {
        QDir().mkpath(
            info.absolutePath()
            );
    }

    return info.absoluteFilePath();
}

void FileController::enterNoDatabaseState()
{
    m_currentFile.clear();

    if (m_window)
    {
        m_window->applyNoDatabaseState();
    }
    else
    {
        setNoFileState();
    }
}

QString FileController::mostRecentDatabasePath() const
{
    const ClassMngr::Next::Application::RecentWorkspaceHistory history =
        m_recentWorkspaceHistoryPort->load();

    const std::optional<
        ClassMngr::Next::Application::RecentWorkspacePath
        > recentPath = history.mostRecentDatabasePath();
    return recentPath.has_value()
        ? qtPath(*recentPath)
        : QString();
}

void FileController::pruneRecentFile(
    const QString& filePath
    )
{
    const QString normalizedPath =
        normalizeInputFilePath(filePath);

    ClassMngr::Next::Application::RecentWorkspaceHistory history =
        m_recentWorkspaceHistoryPort->load();

    const ClassMngr::Next::Application::RecentWorkspacePath rawPath(
        utf8Path(filePath)
        );
    const ClassMngr::Next::Application::RecentWorkspacePath normalizedPathValue(
        utf8Path(normalizedPath)
        );

    history =
        ClassMngr::Next::Application::RecentWorkspaceHistoryUseCase::prune(
            history,
            rawPath,
            normalizedPathValue
            );

    const auto saved = m_recentWorkspaceHistoryPort->save(history);
    if (!saved)
    {
        DialogServices::showWarning(
            m_window,
            tr("Recent Files"),
            tr("The recent workspace history could not be updated."),
            domainErrorMessage(saved.error())
            );
    }
}

QString FileController::databaseDialogDirectory() const
{
    const QString activePath =
        !m_currentFile.trimmed().isEmpty()
            ? m_currentFile
            : (
                m_services
                && m_services->hasOpenDatabase()
                    ? m_services->currentDatabasePath()
                    : QString()
                );

    if (!activePath.trimmed().isEmpty())
    {
        const QDir activeDirectory =
            QFileInfo(activePath).absoluteDir();

        if (activeDirectory.exists())
        {
            return activeDirectory.absolutePath();
        }
    }

    const auto lastDirectoryValue =
        m_lastDatabaseDirectoryPort->read();
    const QString lastDirectory =
        QString::fromUtf8(
            lastDirectoryValue.value().data(),
            static_cast<qsizetype>(lastDirectoryValue.value().size())
            );

    if (
        !lastDirectory.trimmed().isEmpty()
        && QDir(lastDirectory).exists()
        )
    {
        return QFileInfo(lastDirectory)
            .absoluteFilePath();
    }

    return defaultDatabaseDirectory();
}

QString FileController::defaultDatabaseDirectory() const
{
    QString baseDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation
            );

    if (baseDirectory.trimmed().isEmpty())
    {
        baseDirectory =
            QStandardPaths::writableLocation(
                QStandardPaths::HomeLocation
                );
    }

    if (baseDirectory.trimmed().isEmpty())
    {
        baseDirectory =
            QDir::homePath();
    }

    QDir directory(baseDirectory);

    const QString defaultPath =
        directory.filePath(
            QStringLiteral("ClassMngr/Databases")
            );

    if (QDir().mkpath(defaultPath))
    {
        return QFileInfo(defaultPath)
            .absoluteFilePath();
    }

    return QFileInfo(baseDirectory)
        .absoluteFilePath();
}

void FileController::rememberDatabaseDirectory(
    const QString& filePath
    )
{
    const QString directoryPath =
        QFileInfo(filePath)
            .absoluteDir()
            .absolutePath();

    if (directoryPath.trimmed().isEmpty())
    {
        return;
    }

    const auto result =
        m_lastDatabaseDirectoryPort->write(
            ClassMngr::Next::Application::DirectoryPreferencePath(
                utf8Path(directoryPath)
                )
            );
    if (!result)
    {
        DialogServices::showWarning(
            m_window,
            tr("Settings"),
            domainErrorMessage(result.error())
            );
    }
}

void FileController::setLoadedFileState()
{
    if (!m_actions)
        return;

    m_actions->saveFile->setEnabled(true);
    m_actions->closeFile->setEnabled(true);
}

void FileController::setNoFileState()
{
    if (!m_actions)
        return;

    m_actions->saveFile->setEnabled(false);
    m_actions->closeFile->setEnabled(false);
}
