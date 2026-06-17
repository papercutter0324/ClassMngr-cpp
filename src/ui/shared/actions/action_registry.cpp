#include "action_registry.h"
#include "core/settingsmanager.h"
#include "ui/shared/state/option_state.h"
#include "ui/shared/state/option_state_keys.h"
#include "ui/shared/constants/options.h"

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QApplication>
#include <QStyle>

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
    const QString& statusTip)
{
    return createAction(QIcon(), text, statusTip);
}

QAction* ActionRegistry::createAction(
    const QIcon& icon,
    const QString& text,
    const QString& statusTip)
{
    auto* action = new QAction(icon, text, this);
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
            QApplication::style()->standardIcon(
                QStyle::SP_FileIcon),
            tr("New Database..."),
            tr("Create a new database")
            );

    openFile =
        createAction(
            QApplication::style()->standardIcon(
                QStyle::SP_DialogOpenButton),
            tr("Open..."),
            tr("Open an existing file")
            );

    saveFile =
        createAction(
            QApplication::style()->standardIcon(
                QStyle::SP_DialogSaveButton),
            tr("Save"),
            tr("Save the current file")
            );

    saveAsFile =
        createAction(
            QApplication::style()->standardIcon(
                QStyle::SP_DialogSaveButton),
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
            QApplication::style()->standardIcon(
                QStyle::SP_DialogCloseButton),
            tr("Close"),
            tr("Close the current file")
            );

    exitApp =
        createAction(
            QApplication::style()->standardIcon(
                QStyle::SP_MessageBoxInformation),
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
            QIcon::fromTheme(QIcon::ThemeIcon::EditUndo),
            tr("Undo"),
            tr("Undo the last action")
            );

    redo =
        createAction(
            QIcon::fromTheme(QIcon::ThemeIcon::EditRedo),
            tr("Redo"),
            tr("Redo the last undone action")
            );

    cut =
        createAction(
            QIcon::fromTheme(QIcon::ThemeIcon::EditCut),
            tr("Cut"),
            tr("Cut the selected content")
            );

    copy =
        createAction(
            QIcon::fromTheme(QIcon::ThemeIcon::EditCopy),
            tr("Copy"),
            tr("Copy the selected content")
            );

    paste =
        createAction(
            QIcon::fromTheme(QIcon::ThemeIcon::EditPaste),
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
        Theme::Dark
    );


    showSidebarTooltips =
        createCheckableAction(
            tr("Show Sidebar Tooltips"),
            tr("Show full sidebar names in tooltips when they do not fit")
            );

    animateSidebarText =
        createCheckableAction(
            tr("Animate Overflowing Sidebar Text"),
            tr("Animate overflowing sidebar names on hover")
            );

    showSidebarTooltips->setChecked(
        SettingsManager::instance().sidebarTooltipsEnabled()
        );

    animateSidebarText->setChecked(
        SettingsManager::instance().sidebarMarqueeEnabled()
        );

    connect(
        showSidebarTooltips,
        &QAction::toggled,
        this,
        [](bool enabled)
        {
            SettingsManager::instance().setSidebarTooltipsEnabled(
                enabled
                );
        }
        );

    connect(
        animateSidebarText,
        &QAction::toggled,
        this,
        [](bool enabled)
        {
            SettingsManager::instance().setSidebarMarqueeEnabled(
                enabled
                );
        }
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
