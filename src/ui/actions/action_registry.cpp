#include "action_registry.h"
#include "ui/state/option_state.h"
#include "ui/constants/options.h"

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>

// =========================================================
// Constructor
// =========================================================

ActionRegistry::ActionRegistry(QObject* parent)
    : QObject(parent)
{
}

// =========================================================
// Helpers
// =========================================================

QAction* ActionRegistry::createAction(
    const QString& text,
    const QString& statusTip
    )
{
    auto* action =
        new QAction(text, this);

    action->setStatusTip(statusTip);

    return action;
}

QAction* ActionRegistry::createCheckableAction(
    const QString& text,
    const QString& statusTip
    )
{
    auto* action =
        createAction(text, statusTip);

    action->setCheckable(true);

    return action;
}

// =========================================================
// Creation
// =========================================================

void ActionRegistry::createActions()
{
    createFileActions();
    createEditActions();
    createClassActions();
    createOptionActions();
    createHelpActions();
    createAdminActions();
}

// =========================================================
// File Actions
// =========================================================

void ActionRegistry::createFileActions()
{
    newFile =
        createAction(
            tr("New"),
            tr("Create a new file")
            );

    openFile =
        createAction(
            tr("Open..."),
            tr("Open an existing file")
            );

    saveFile =
        createAction(
            tr("Save"),
            tr("Save the current file")
            );

    saveAsFile =
        createAction(
            tr("Save As..."),
            tr("Save the file with a new name")
            );

    exportAsFile =
        createAction(
            tr("Export As..."),
            tr("Export the current file")
            );

    closeFile =
        createAction(
            tr("Close"),
            tr("Close the current file")
            );

    exitApp =
        createAction(
            tr("Exit"),
            tr("Exit the application")
            );

    newFile->setShortcut(QKeySequence::New);
    openFile->setShortcut(QKeySequence::Open);
    saveFile->setShortcut(QKeySequence::Save);
    saveAsFile->setShortcut(QKeySequence::SaveAs);
}

// =========================================================
// Edit Actions
// =========================================================

void ActionRegistry::createEditActions()
{
    undo =
        createAction(
            tr("Undo"),
            tr("Undo the last action")
            );

    redo =
        createAction(
            tr("Redo"),
            tr("Redo the last undone action")
            );

    cut =
        createAction(
            tr("Cut"),
            tr("Cut the selected content")
            );

    copy =
        createAction(
            tr("Copy"),
            tr("Copy the selected content")
            );

    paste =
        createAction(
            tr("Paste"),
            tr("Paste content from the clipboard")
            );

    undo->setShortcut(QKeySequence::Undo);
    redo->setShortcut(QKeySequence::Redo);

    cut->setShortcut(QKeySequence::Cut);
    copy->setShortcut(QKeySequence::Copy);
    paste->setShortcut(QKeySequence::Paste);
}

// =========================================================
// Class Actions
// =========================================================

void ActionRegistry::createClassActions()
{
    newClass =
        createAction(
            tr("New Class"),
            tr("Create a new class")
            );

    deleteClass =
        createAction(
            tr("Delete Class"),
            tr("Delete the selected class")
            );

    newTeacher =
        createAction(
            tr("New Teacher"),
            tr("Create a new teacher")
            );

    deleteTeacher =
        createAction(
            tr("Delete Teacher"),
            tr("Delete the selected teacher")
            );
}

// =========================================================
// Option Actions
// =========================================================

void ActionRegistry::createOptionActions()
{
    saveModeState =
        new OptionState<SaveMode>(OptionKeys::SaveMode, this);

    auto automaticSaveAction =
        createCheckableAction(
            tr("Automatic"),
            tr("Automatically save changes")
        );

    auto manualSaveAction =
        createCheckableAction(
            tr("Manual"),
            tr("Save changes manually")
        );

    saveModeState->addOption(SaveMode::Automatic, automaticSaveAction);
    saveModeState->addOption(SaveMode::Manual, manualSaveAction);

    // LOAD from settings (THIS is the correct place)
    saveModeState->loadFromSettings(
        OptionKeys::SaveMode,
        SaveMode::Automatic
        );


    themeState =
        new OptionState<Theme>(OptionKeys::Theme, this);

    auto darkThemeAction =
        createCheckableAction(
            tr("Dark Theme"),
            tr("Use dark theme")
        );

    auto lightThemeAction =
        createCheckableAction(
            tr("Light Theme"),
            tr("Use light theme")
        );

    themeState->addOption(Theme::Dark, darkThemeAction);
    themeState->addOption(Theme::Light, lightThemeAction);

    // LOAD from settings (THIS is the correct place)
    themeState->loadFromSettings(
        OptionKeys::Theme,
        Theme::Dark
    );
}

// =========================================================
// Help Actions
// =========================================================

void ActionRegistry::createHelpActions()
{
    about =
        createAction(
            tr("About"),
            tr("Show application information")
            );
}

// =========================================================
// Admin Actions
// =========================================================

void ActionRegistry::createAdminActions()
{
    manageCampuses =
        createAction(
            tr("Manage Campuses"),
            tr("Manage campus settings")
            );
}