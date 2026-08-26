#include "app/controllers/file_controller.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/database_file_format.h"
#include "core/result.h"
#include "core/settingsmanager.h"
#include "data/data_service.h"
#include "ui/shared/dialogs/file_dialog_service.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QStandardPaths>
#include <QUuid>

namespace
{
constexpr int MaxRecentFiles = 10;
}

FileController::FileController(
    ApplicationServices* services,
    MainWindow* window,
    QObject* parent
    )
    : QObject(parent)
    , m_services(services)
    , m_window(window)
{
}

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
        !loadDatabase(normalizedPath, false)
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

    closeActiveDatabase();

    if (
        QFile::exists(normalizedPath)
        && !QFile::remove(normalizedPath)
        )
    {
        DialogServices::showWarning(
            m_window,
            tr("New Teacher Profile"),
            tr("Unable to replace existing Teacher Profile file:\n%1")
                .arg(normalizedPath)
            );

        enterNoDatabaseState();
        return false;
    }

    const Status opened =
        m_services->openDatabase(normalizedPath);

    if (!opened)
    {
        DialogServices::showWarning(
            m_window,
            tr("New Teacher Profile"),
            opened.error()
            );

        enterNoDatabaseState();
        return false;
    }

    m_currentFile =
        m_services->currentDatabasePath();

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

bool FileController::createInitialSetupDatabase(
    const QString& filePath
    )
{
    if (!m_services)
    {
        enterNoDatabaseState();
        return false;
    }

    closeActiveDatabase();

    m_initialSetupDatabasePath = filePath;
    m_initialSetupBackupPath.clear();

    if (QFile::exists(filePath))
    {
        m_initialSetupBackupPath = initialSetupBackupPath(filePath);
        if (!QFile::rename(filePath, m_initialSetupBackupPath))
        {
            DialogServices::showWarning(
                m_window,
                tr("New Teacher Profile"),
                tr("Unable to preserve the existing Teacher Profile file:\n%1")
                    .arg(filePath)
                );
            m_initialSetupDatabasePath.clear();
            m_initialSetupBackupPath.clear();
            enterNoDatabaseState();
            return false;
        }
    }

    const Status opened =
        m_services->openDatabase(filePath);

    if (!opened)
    {
        const QString error = opened.error();
        cancelInitialSetup();
        DialogServices::showWarning(
            m_window,
            tr("New Teacher Profile"),
            error
            );
        return false;
    }

    m_currentFile =
        m_services->currentDatabasePath();

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
    if (m_initialSetupDatabasePath.isEmpty())
    {
        return;
    }

    if (
        !m_initialSetupBackupPath.isEmpty()
        && !QFile::remove(m_initialSetupBackupPath)
        )
    {
        DialogServices::showWarning(
            m_window,
            tr("Initial Setup"),
            tr("Setup is complete, but the replaced Teacher Profile could not be removed:\n%1")
                .arg(m_initialSetupBackupPath)
            );
    }

    m_initialSetupDatabasePath.clear();
    m_initialSetupBackupPath.clear();

    if (!m_currentFile.isEmpty())
    {
        updateRecentFiles(m_currentFile);
    }
}

void FileController::cancelInitialSetup()
{
    const QString databasePath = m_initialSetupDatabasePath;
    const QString backupPath = m_initialSetupBackupPath;

    closeActiveDatabase();

    m_initialSetupDatabasePath.clear();
    m_initialSetupBackupPath.clear();

    if (!databasePath.isEmpty())
    {
        if (backupPath.isEmpty())
        {
            if (QFile::exists(databasePath) && !QFile::remove(databasePath))
            {
                DialogServices::showWarning(
                    m_window,
                    tr("Initial Setup"),
                    tr("The incomplete Teacher Profile could not be removed:\n%1")
                        .arg(databasePath)
                    );
            }
        }
        else
        {
            const QString incompletePath = initialSetupBackupPath(databasePath);
            bool movedIncompleteProfile = true;
            if (QFile::exists(databasePath))
            {
                movedIncompleteProfile =
                    QFile::rename(databasePath, incompletePath);
            }

            if (
                !movedIncompleteProfile
                || !QFile::rename(backupPath, databasePath)
                )
            {
                if (
                    movedIncompleteProfile
                    && QFile::exists(incompletePath)
                    )
                {
                    QFile::rename(incompletePath, databasePath);
                }

                DialogServices::showWarning(
                    m_window,
                    tr("Initial Setup"),
                    tr("The original Teacher Profile could not be restored:\n%1")
                        .arg(backupPath)
                    );
            }
            else if (QFile::exists(incompletePath))
            {
                QFile::remove(incompletePath);
            }
        }
    }

    enterNoDatabaseState();
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

bool FileController::loadDatabase(
    const QString& filePath,
    bool showErrorMessage
    )
{
    const QString normalizedPath =
        normalizeInputFilePath(filePath);

    if (!QFileInfo::exists(normalizedPath))
    {
        pruneRecentFile(normalizedPath);
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
        enterNoDatabaseState();
        return false;
    }

    closeActiveDatabase();

    const Status opened =
        m_services->openDatabase(normalizedPath);

    if (!opened)
    {
        if (showErrorMessage)
        {
            DialogServices::showWarning(
                m_window,
                tr("Open Teacher Profile"),
                opened.error()
                );
        }

        enterNoDatabaseState();
        return false;
    }

    m_currentFile =
        m_services->currentDatabasePath();

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

    closeActiveDatabase();

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
        pruneRecentFile(normalizedPath);
        populateRecentMenu();

        DialogServices::showWarning(
            m_window,
            tr("Missing File"),
            tr("File not found:\n%1")
                .arg(normalizedPath)
            );

        return;
    }

    loadDatabase(normalizedPath);
}

void FileController::updateRecentFiles(
    const QString& filePath
    )
{
    const QString normalizedPath =
        normalizeInputFilePath(filePath);

    QStringList files =
        SettingsManager::instance()
            .getRecentFiles();

    files.removeAll(filePath);
    files.removeAll(normalizedPath);
    files.prepend(normalizedPath);

    while (files.size() > MaxRecentFiles)
    {
        files.removeLast();
    }

    SettingsManager::instance()
        .setRecentFiles(files);

    SettingsManager::instance()
        .setLastFile(normalizedPath);

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

    const QStringList files =
        SettingsManager::instance()
            .getRecentFiles();

    if (files.isEmpty())
    {
        auto* emptyAction =
            menu->addAction(
                tr("(No Recent Files)")
                );

        emptyAction->setEnabled(false);
        return;
    }

    for (int index = 0; index < files.size(); ++index)
    {
        const QString filePath =
            files.at(index);

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
    SettingsManager::instance()
        .clearRecentFiles();

    SettingsManager::instance()
        .setLastFile(QString());

    populateRecentMenu();
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
        )
    {
        return false;
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
        )
    {
        return false;
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

void FileController::closeActiveDatabase()
{
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
}

QString FileController::initialSetupBackupPath(
    const QString& filePath
    ) const
{
    const QFileInfo info(filePath);
    return info.absoluteDir().filePath(
        QStringLiteral(".%1.initial-setup-%2.backup")
            .arg(
                info.fileName(),
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
        );
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
    const QStringList recentFiles =
        SettingsManager::instance()
            .getRecentFiles();

    if (!recentFiles.isEmpty())
    {
        return recentFiles.first();
    }

    return SettingsManager::instance()
        .getLastFile();
}

void FileController::pruneRecentFile(
    const QString& filePath
    )
{
    const QString normalizedPath =
        normalizeInputFilePath(filePath);

    QStringList files =
        SettingsManager::instance()
            .getRecentFiles();

    files.removeAll(filePath);
    files.removeAll(normalizedPath);

    SettingsManager::instance()
        .setRecentFiles(files);

    if (
        SettingsManager::instance()
            .getLastFile()
        == filePath
        || SettingsManager::instance()
            .getLastFile()
        == normalizedPath
        )
    {
        SettingsManager::instance()
            .setLastFile(QString());
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

    const QString lastDirectory =
        SettingsManager::instance()
            .getLastDatabaseDirectory();

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

    SettingsManager::instance()
        .setLastDatabaseDirectory(directoryPath);
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
