#include "controllers/file_controller.h"

#include "app/mainwindow.h"
#include "core/application_services.h"
#include "services/dataservice.h"

#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

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

void FileController::newFile()
{
    if (!confirmUnsavedChanges())
        return;

    m_currentFile.clear();

    setNoFileState();
}

void FileController::openFile()
{
    if (!confirmUnsavedChanges())
        return;

    const QString filePath =
        QFileDialog::getOpenFileName(
            nullptr,
            tr("Open Database"),
            QString(),
            tr("Database Files (*.db)")
            );

    if (filePath.isEmpty())
        return;

    loadDatabase(filePath);
}

void FileController::loadDatabase(
    const QString& filePath
    )
{
    m_currentFile = filePath;

    setLoadedFileState();

    updateRecentFiles(filePath);
}

void FileController::saveFile()
{
    if (m_currentFile.isEmpty())
    {
        saveAsFile();
        return;
    }

    saveDatabase();
}

void FileController::saveAsFile()
{
    QString filePath =
        QFileDialog::getSaveFileName(
            nullptr,
            tr("Save Database"),
            QString(),
            tr("Database Files (*.db)")
            );

    if (filePath.isEmpty())
        return;

    saveDatabaseAs(filePath);
}

void FileController::exportAsFile()
{
    const QString filePath =
        QFileDialog::getSaveFileName(
            nullptr,
            tr("Export Database As"),
            QString(),
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

    newFile();
}

void FileController::openSpecificFile(
    const QString& filePath
    )
{
    if (!confirmUnsavedChanges())
        return;

    if (!QFileInfo::exists(filePath))
    {
        QMessageBox::warning(
            nullptr,
            tr("Missing File"),
            tr("File not found:\n%1")
                .arg(filePath)
            );

        return;
    }

    loadDatabase(filePath);
}

void FileController::updateRecentFiles(
    const QString& filePath
    )
{
    Q_UNUSED(filePath);

    // TODO:
    // SettingsManager integration

    populateRecentMenu();
}

void FileController::populateRecentMenu()
{
    // TODO: Implement recent files menu
}

void FileController::clearRecentFiles()
{
    // TODO:
    // SettingsManager integration

    populateRecentMenu();
}

void FileController::autosave()
{
    saveDatabase();
}

void FileController::saveDatabase()
{
    if (!m_services)
        return;

    m_services
        ->dataService()
        ->save();
}

void FileController::saveDatabaseAs(
    const QString& filePath
    )
{
    const QString normalized =
        normalizeFilePath(filePath);

    if (!m_services)
        return;

    m_services
        ->dataService()
        ->saveAs(normalized);

    m_currentFile = normalized;

    setLoadedFileState();
}

void FileController::exportDatabaseAs(
    const QString& filePath
    )
{
    const QString normalized =
        normalizeFilePath(filePath);

    if (!m_services)
        return;

    m_services
        ->dataService()
        ->exportAs(normalized);
}

QString FileController::normalizeFilePath(
    const QString& filePath,
    const QString& extension
    ) const
{
    QFileInfo info(filePath);

    if (!info.absolutePath().isEmpty())
    {
        QDir().mkpath(
            info.absolutePath()
            );
    }

    QString result =
        filePath;

    if (!result.endsWith(extension))
    {
        result += extension;
    }

    return result;
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
