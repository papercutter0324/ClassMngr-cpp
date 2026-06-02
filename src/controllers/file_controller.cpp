#include "controllers/file_controller.h"

#include "ui/actions/action_registry.h"

#include <QAction>
#include <QFileDialog>

FileController::FileController(QObject* parent)
    : QObject(parent)
{
}

void FileController::connectActions(ActionRegistry& actions)
{
    m_actions = &actions;

    if (!actions.saveModeState)
        return;

    connect(actions.newFile, &QAction::triggered,
            this, [this] { newFile(); });

    connect(actions.openFile, &QAction::triggered,
            this, [this] { openFile(); });

    connect(actions.saveFile, &QAction::triggered,
            this, [this] { saveFile(); });

    connect(actions.saveAsFile, &QAction::triggered,
            this, [this] { saveAsFile(); });

    connect(actions.exportAsFile, &QAction::triggered,
            this, [this] { exportAsFile(); });

    connect(actions.closeFile, &QAction::triggered,
            this, [this] { closeFile(); });

    actions.saveModeState->onChanged =
        [this](SaveMode mode)
    {
        setSaveMode(mode);
    };
}

void FileController::setSaveMode(SaveMode mode)
{
    switch (mode)
    {
    case SaveMode::Automatic:
        // enable autosave system
        break;

    case SaveMode::Manual:
        // disable autosave system
        break;
    }
}

void FileController::newFile()
{
    // TODO: implement real file creation logic
}

void FileController::openFile()
{
    QString filename =
        QFileDialog::getOpenFileName(
            nullptr,
            tr("Open File")
            );

    if (filename.isEmpty())
        return;

    // TODO: load file
}

void FileController::saveFile()
{
    // TODO: save current file
}

void FileController::saveAsFile()
{
    QString filename =
        QFileDialog::getSaveFileName(
            nullptr,
            tr("Save As")
            );

    if (filename.isEmpty())
        return;

    // TODO: save file to new path
}

void FileController::exportAsFile()
{
    // TODO: export logic
}

void FileController::closeFile()
{
    // TODO: close file logic
}