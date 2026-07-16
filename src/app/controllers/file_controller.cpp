#include "app/controllers/file_controller.h"

#include "app/mainwindow.h"
#include "core/application_services.h"
#include "core/result.h"
#include "core/settingsmanager.h"
#include "data/data_service.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>

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
        normalizeFilePath(
            recentPath,
            QStringLiteral(".db"),
            false
            );

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

void FileController::newFile()
{
    if (!confirmUnsavedChanges())
        return;

    const QString filePath =
        QFileDialog::getSaveFileName(
            nullptr,
            tr("New Database"),
            databaseDialogDirectory(),
            tr("Database Files (*.db)")
            );

    if (filePath.isEmpty())
        return;

    const QString normalizedPath =
        normalizeFilePath(
            filePath,
            QStringLiteral(".db"),
            true
            );

    if (!m_services)
    {
        enterNoDatabaseState();
        return;
    }

    closeActiveDatabase();

    if (
        QFile::exists(normalizedPath)
        && !QFile::remove(normalizedPath)
        )
    {
        QMessageBox::warning(
            nullptr,
            tr("New Database"),
            tr("Unable to replace existing database file:\n%1")
                .arg(normalizedPath)
            );

        enterNoDatabaseState();
        return;
    }

    const Status opened =
        m_services->openDatabase(normalizedPath);

    if (!opened)
    {
        QMessageBox::warning(
            nullptr,
            tr("New Database"),
            opened.error()
            );

        enterNoDatabaseState();
        return;
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
}

void FileController::openFile()
{
    if (!confirmUnsavedChanges())
        return;

    const QString filePath =
        QFileDialog::getOpenFileName(
            nullptr,
            tr("Open Database"),
            databaseDialogDirectory(),
            tr("Database Files (*.db)")
            );

    if (filePath.isEmpty())
        return;

    loadDatabase(filePath);
}

bool FileController::loadDatabase(
    const QString& filePath,
    bool showErrorMessage
    )
{
    const QString normalizedPath =
        normalizeFilePath(
            filePath,
            QStringLiteral(".db"),
            false
            );

    if (!QFileInfo::exists(normalizedPath))
    {
        pruneRecentFile(normalizedPath);
        populateRecentMenu();

        if (showErrorMessage)
        {
            QMessageBox::warning(
                nullptr,
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
            QMessageBox::warning(
                nullptr,
                tr("Open Database"),
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

    QString filePath =
        QFileDialog::getSaveFileName(
            nullptr,
            tr("Save Database"),
            databaseDialogDirectory(),
            tr("Database Files (*.db)")
            );

    if (filePath.isEmpty())
        return;

    saveDatabaseAs(filePath);
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

    const QString filePath =
        QFileDialog::getSaveFileName(
            nullptr,
            tr("Export Database As"),
            databaseDialogDirectory(),
            tr("SQLite Database (*.db)")
            );

    if (filePath.isEmpty())
        return;

    exportDatabaseAs(filePath);
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
        normalizeFilePath(
            filePath,
            QStringLiteral(".db"),
            false
            );

    if (!QFileInfo::exists(normalizedPath))
    {
        pruneRecentFile(normalizedPath);
        populateRecentMenu();

        QMessageBox::warning(
            nullptr,
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
        normalizeFilePath(
            filePath,
            QStringLiteral(".db"),
            false
            );

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
        ->dataService()
        ->save();
}

bool FileController::saveDatabaseAs(
    const QString& filePath
    )
{
    const QString normalized =
        normalizeFilePath(
            filePath,
            QStringLiteral(".db"),
            true
            );

    if (
        !m_services
        || !m_services->dataService()
        )
    {
        return false;
    }

    const Status saved =
        m_services
        ->dataService()
        ->saveAs(normalized);

    if (!saved)
    {
        QMessageBox::warning(
            nullptr,
            tr("Save Database"),
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
        normalizeFilePath(
            filePath,
            QStringLiteral(".db"),
            true
            );

    if (
        !m_services
        || !m_services->dataService()
        )
    {
        return false;
    }

    const Status exported =
        m_services
        ->dataService()
        ->exportAs(normalized);

    if (!exported)
    {
        QMessageBox::warning(
            nullptr,
            tr("Export Database"),
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
    if (m_services)
    {
        m_services->closeDatabase();
    }

    m_currentFile.clear();

    if (m_window)
    {
        m_window->clearDatabaseBackedState();
    }
}

QString FileController::normalizeFilePath(
    const QString& filePath,
    const QString& extension,
    bool createDirectories
    ) const
{
    QString result =
        filePath;

    if (!result.endsWith(extension, Qt::CaseInsensitive))
    {
        result += extension;
    }

    QFileInfo info(result);

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
        normalizeFilePath(
            filePath,
            QStringLiteral(".db"),
            false
            );

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
