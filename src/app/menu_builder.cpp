#include "menu_builder.h"

#include "mainwindow.h"
#include "ui/actions/action_registry.h"
#include "ui/menus/menu_utils.h"

#include <QMenu>
#include <QMenuBar>

// Missing safety: menu duplication risk
// Option: Store menus in MainWindow
//  QMenu* fileMenu;
//  QMenu* editMenu;
//  ...

void MenuBuilder::build(MainWindow* window)
{
    buildFileMenu(window);
    buildEditMenu(window);
    buildClassMenu(window);
    buildOptionsMenu(window);
    buildHelpMenu(window);

    if (window->isAdmin())
    {
        buildAdminMenu(window);
    }
}

void MenuBuilder::buildFileMenu(MainWindow* window)
{
    auto& actions = window->actions();

    QMenu* fileMenu =
        window->menuBar()->addMenu(
        QCoreApplication::translate("MenuBuilder", "File")
    );

    fileMenu->addAction(actions.newFile);
    fileMenu->addAction(actions.openFile);

    fileMenu->addSeparator();

    fileMenu->addAction(actions.saveFile);
    fileMenu->addAction(actions.saveAsFile);
    fileMenu->addAction(actions.exportAsFile);

    fileMenu->addSeparator();

    fileMenu->addAction(actions.closeFile);

    fileMenu->addSeparator();

    fileMenu->addAction(actions.exitApp);
}

void MenuBuilder::buildEditMenu(MainWindow* window)
{
    auto& a = window->actions();

    QMenu* menu =
        window->menuBar()->addMenu(
        QCoreApplication::translate("MenuBuilder", "Edit")
    );

    menu->addAction(a.undo);
    menu->addAction(a.redo);

    menu->addSeparator();

    menu->addAction(a.cut);
    menu->addAction(a.copy);
    menu->addAction(a.paste);
}

void MenuBuilder::buildClassMenu(MainWindow* window)
{
    auto& a = window->actions();

    QMenu* menu =
        window->menuBar()->addMenu(
        QCoreApplication::translate("MenuBuilder", "Classes")
    );

    menu->addAction(a.newClass);
    menu->addAction(a.deleteClass);

    menu->addSeparator();

    menu->addAction(a.newTeacher);
    menu->addAction(a.deleteTeacher);
}

void MenuBuilder::buildOptionsMenu(MainWindow* window)
{
    auto& actions = window->actions();

    QMenu* options =
        window->menuBar()->addMenu(
        QCoreApplication::translate("MenuBuilder", "Options")
    );

    //
    // Save Mode
    //
    QMenu* saveMenu =
        options->addMenu(
        QCoreApplication::translate("MenuBuilder", "Save Mode")
    );

    addOptionMenu<SaveMode>(
        saveMenu,
        actions.saveModeState,
        {
            SaveMode::Automatic,
            SaveMode::Manual
        }
        );

    //
    // Theme
    //
    QMenu* themeMenu =
        options->addMenu(
        QCoreApplication::translate("MenuBuilder", "Theme")
    );

    addOptionMenu<Theme>(
        themeMenu,
        actions.themeState,
        {
            Theme::Dark,
            Theme::Light
        }
        );
}

void MenuBuilder::buildHelpMenu(MainWindow* window)
{
    auto& a = window->actions();

    QMenu* menu =
        window->menuBar()->addMenu(
        QCoreApplication::translate("MenuBuilder", "Help")
    );

    menu->addAction(a.about);
}

void MenuBuilder::buildAdminMenu(MainWindow* window)
{
    auto& a = window->actions();

    QMenu* menu =
        window->menuBar()->addMenu(
        QCoreApplication::translate("MenuBuilder", "Admin")
    );

    menu->addAction(a.manageCampuses);
}