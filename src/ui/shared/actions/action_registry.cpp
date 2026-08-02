#include "action_registry.h"
#include "core/settingsmanager.h"
#include "ui/shared/state/option_state.h"
#include "ui/shared/state/option_state_keys.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/styles/themed_icon_utils.h"

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QApplication>
#include <QIcon>
#include <QStyle>

namespace
{
void updateActionText(
    QAction* action,
    const QString& text,
    const QString& statusTip
    )
{
    if (!action)
    {
        return;
    }

    action->setText(text);
    action->setStatusTip(statusTip);
}

QIcon themedThemeIcon(
    QIcon::ThemeIcon icon
    )
{
    return ThemedIconUtils::recolor(
        QIcon::fromTheme(icon),
        QApplication::palette()
        );
}
}

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

void ActionRegistry::retranslate()
{
    updateActionText(
        newFile,
        tr("New Database..."),
        tr("Create a new database")
        );
    updateActionText(
        openFile,
        tr("Open..."),
        tr("Open an existing file")
        );
    updateActionText(
        saveFile,
        tr("Save"),
        tr("Save the current file")
        );
    updateActionText(
        saveAsFile,
        tr("Save As..."),
        tr("Save the file with a new name")
        );
    updateActionText(
        exportAsFile,
        tr("Export As..."),
        tr("Export the current file")
        );
    updateActionText(
        closeFile,
        tr("Close"),
        tr("Close the current file")
        );
    updateActionText(
        exitApp,
        tr("Exit"),
        tr("Exit the application")
        );

    updateActionText(
        undo,
        tr("Undo"),
        tr("Undo the last action")
        );
    updateActionText(
        redo,
        tr("Redo"),
        tr("Redo the last undone action")
        );
    updateActionText(
        cut,
        tr("Cut"),
        tr("Cut the selected content")
        );
    updateActionText(
        copy,
        tr("Copy"),
        tr("Copy the selected content")
        );
    updateActionText(
        paste,
        tr("Paste"),
        tr("Paste content from the clipboard")
        );

    updateActionText(
        newClass,
        tr("New Class"),
        tr("Create a new class")
        );
    updateActionText(
        deleteClass,
        tr("Delete Class"),
        tr("Delete the selected class")
        );
    updateActionText(
        importClasses,
        tr("Import Classes..."),
        tr("Import classes from a class package")
        );
    updateActionText(
        exportClasses,
        tr("Export Classes..."),
        tr("Export selected classes to a class package")
        );
    updateActionText(
        newTeacher,
        tr("New Teacher"),
        tr("Create a new teacher")
        );
    updateActionText(
        deleteTeacher,
        tr("Delete Teacher"),
        tr("Delete the selected teacher")
        );
    updateActionText(
        importTeachers,
        tr("Import Teachers..."),
        tr("Import teachers and campus staff from an Excel workbook")
        );

    if (saveModeState)
    {
        updateActionText(
            saveModeState->action(SaveMode::Automatic),
            tr("Automatic"),
            tr("Automatically save changes")
            );
        updateActionText(
            saveModeState->action(SaveMode::Manual),
            tr("Manual"),
            tr("Save changes manually")
            );
    }

    if (themeState)
    {
        updateActionText(
            themeState->action(Theme::Dark),
            tr("Dark Theme"),
            tr("Use dark theme")
            );
        updateActionText(
            themeState->action(Theme::Light),
            tr("Light Theme"),
            tr("Use light theme")
            );
    }

    if (languageState)
    {
        updateActionText(
            languageState->action(Language::SystemDefault),
            tr("System Default"),
            tr("Use the system language")
            );
        updateActionText(
            languageState->action(Language::English),
            tr("English"),
            tr("Use English")
            );
        updateActionText(
            languageState->action(Language::Korean),
            tr("Korean"),
            tr("Use Korean")
            );
    }

    if (fontSizeState)
    {
        updateActionText(
            fontSizeState->action(FontSize::Small),
            tr("Small"),
            tr("Use small font size")
            );
        updateActionText(
            fontSizeState->action(FontSize::Normal),
            tr("Normal"),
            tr("Use normal font size")
            );
        updateActionText(
            fontSizeState->action(FontSize::Large),
            tr("Large"),
            tr("Use large font size")
            );
        updateActionText(
            fontSizeState->action(FontSize::ExtraLarge),
            tr("Extra Large"),
            tr("Use extra large font size")
            );
    }

    if (documentPageSpacingState)
    {
        updateActionText(
            documentPageSpacingState->action(DocumentPageSpacing::None),
            tr("None"),
            tr("Show PDF pages with no added spacing")
            );
        updateActionText(
            documentPageSpacingState->action(DocumentPageSpacing::Small),
            tr("Small"),
            tr("Show PDF pages with small spacing")
            );
        updateActionText(
            documentPageSpacingState->action(DocumentPageSpacing::Medium),
            tr("Medium"),
            tr("Show PDF pages with medium spacing")
            );
        updateActionText(
            documentPageSpacingState->action(DocumentPageSpacing::Large),
            tr("Large"),
            tr("Show PDF pages with large spacing")
            );
    }

    if (documentViewerBackgroundState)
    {
        updateActionText(
            documentViewerBackgroundState->action(DocumentViewerBackground::Default),
            tr("Default"),
            tr("Use the current theme's PDF viewer background")
            );
        updateActionText(
            documentViewerBackgroundState->action(DocumentViewerBackground::White),
            tr("White"),
            tr("Use a white PDF viewer background")
            );
        updateActionText(
            documentViewerBackgroundState->action(DocumentViewerBackground::Black),
            tr("Black"),
            tr("Use a black PDF viewer background")
            );
    }

    updateActionText(
        showSidebarTooltips,
        tr("Show Sidebar Tooltips"),
        tr("Show full sidebar names in tooltips when they do not fit")
        );
    updateActionText(
        animateSidebarText,
        tr("Animate Overflowing Sidebar Text"),
        tr("Animate overflowing sidebar names on hover")
        );
#ifdef Q_OS_MACOS
    updateActionText(
        showPowerPointDataAccessNotice,
        tr("Show Data Access Notice Before Export"),
        tr("Show a notice before PowerPoint accesses its protected workspace")
        );
#endif
    updateActionText(
        checkForUpdates,
        tr("Check for Updates..."),
        tr("Check for a newer version of ClassMngr")
        );
    updateActionText(
        about,
        tr("About"),
        tr("Show application information")
        );
    updateActionText(
        manageCampuses,
        tr("Manage Campuses"),
        tr("Manage campus settings")
        );
}

void ActionRegistry::refreshThemedIcons()
{
    if (newFile)
    {
        newFile->setIcon(
            QApplication::style()->standardIcon(
                QStyle::SP_FileIcon
                )
            );
    }

    if (openFile)
    {
        openFile->setIcon(
            QApplication::style()->standardIcon(
                QStyle::SP_DialogOpenButton
                )
            );
    }

    if (saveFile)
    {
        saveFile->setIcon(
            QApplication::style()->standardIcon(
                QStyle::SP_DialogSaveButton
                )
            );
    }

    if (saveAsFile)
    {
        saveAsFile->setIcon(
            QApplication::style()->standardIcon(
                QStyle::SP_DialogSaveButton
                )
            );
    }

    if (closeFile)
    {
        closeFile->setIcon(
            QApplication::style()->standardIcon(
                QStyle::SP_DialogCloseButton
                )
            );
    }

    if (exitApp)
    {
        exitApp->setIcon(
            QApplication::style()->standardIcon(
                QStyle::SP_MessageBoxInformation
                )
            );
    }

    if (undo)
    {
        undo->setIcon(
            themedThemeIcon(QIcon::ThemeIcon::EditUndo)
            );
    }

    if (redo)
    {
        redo->setIcon(
            themedThemeIcon(QIcon::ThemeIcon::EditRedo)
            );
    }

    if (cut)
    {
        cut->setIcon(
            themedThemeIcon(QIcon::ThemeIcon::EditCut)
            );
    }

    if (copy)
    {
        copy->setIcon(
            themedThemeIcon(QIcon::ThemeIcon::EditCopy)
            );
    }

    if (paste)
    {
        paste->setIcon(
            themedThemeIcon(QIcon::ThemeIcon::EditPaste)
            );
    }
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
            themedThemeIcon(QIcon::ThemeIcon::EditUndo),
            tr("Undo"),
            tr("Undo the last action")
            );

    redo =
        createAction(
            themedThemeIcon(QIcon::ThemeIcon::EditRedo),
            tr("Redo"),
            tr("Redo the last undone action")
            );

    cut =
        createAction(
            themedThemeIcon(QIcon::ThemeIcon::EditCut),
            tr("Cut"),
            tr("Cut the selected content")
            );

    copy =
        createAction(
            themedThemeIcon(QIcon::ThemeIcon::EditCopy),
            tr("Copy"),
            tr("Copy the selected content")
            );

    paste =
        createAction(
            themedThemeIcon(QIcon::ThemeIcon::EditPaste),
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

    importClasses =
        createAction(
            tr("Import Classes..."),
            tr("Import classes from a class package")
            );

    exportClasses =
        createAction(
            tr("Export Classes..."),
            tr("Export selected classes to a class package")
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

    importTeachers =
        createAction(
            tr("Import Teachers..."),
            tr("Import teachers and campus staff from an Excel workbook")
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

    languageState =
        new OptionState<Language>(OptionKeys::Language, this);

    auto systemDefaultLanguageAction =
        createCheckableAction(
            tr("System Default"),
            tr("Use the system language")
            );

    auto englishAction =
        createCheckableAction(
            tr("English"),
            tr("Use English")
        );

    auto koreanLanguageAction =
        createCheckableAction(
            tr("Korean"),
            tr("Use Korean")
            );

    languageState->addOption(
        Language::SystemDefault,
        systemDefaultLanguageAction
        );
    languageState->addOption(
        Language::English,
        englishAction
        );
    languageState->addOption(
        Language::Korean,
        koreanLanguageAction
        );

    languageState->loadFromSettings(
        Language::SystemDefault
        );

    fontSizeState =
        new OptionState<FontSize>(OptionKeys::FontSize, this);

    auto smallFontAction =
        createCheckableAction(
            tr("Small"),
            tr("Use small font size")
            );

    auto normalFontAction =
        createCheckableAction(
            tr("Normal"),
            tr("Use normal font size")
            );

    auto largeFontAction =
        createCheckableAction(
            tr("Large"),
            tr("Use large font size")
            );

    auto extraLargeFontAction =
        createCheckableAction(
            tr("Extra Large"),
            tr("Use extra large font size")
            );

    fontSizeState->addOption(
        FontSize::Small,
        smallFontAction
        );
    fontSizeState->addOption(
        FontSize::Normal,
        normalFontAction
        );
    fontSizeState->addOption(
        FontSize::Large,
        largeFontAction
        );
    fontSizeState->addOption(
        FontSize::ExtraLarge,
        extraLargeFontAction
        );

    fontSizeState->loadFromSettings(
        FontSize::Normal
        );

    documentPageSpacingState =
        new OptionState<DocumentPageSpacing>(
            OptionKeys::DocumentPageSpacing,
            this
            );

    auto noDocumentPageSpacingAction =
        createCheckableAction(
            tr("None"),
            tr("Show PDF pages with no added spacing")
            );

    auto smallDocumentPageSpacingAction =
        createCheckableAction(
            tr("Small"),
            tr("Show PDF pages with small spacing")
            );

    auto mediumDocumentPageSpacingAction =
        createCheckableAction(
            tr("Medium"),
            tr("Show PDF pages with medium spacing")
            );

    auto largeDocumentPageSpacingAction =
        createCheckableAction(
            tr("Large"),
            tr("Show PDF pages with large spacing")
            );

    documentPageSpacingState->addOption(
        DocumentPageSpacing::None,
        noDocumentPageSpacingAction
        );
    documentPageSpacingState->addOption(
        DocumentPageSpacing::Small,
        smallDocumentPageSpacingAction
        );
    documentPageSpacingState->addOption(
        DocumentPageSpacing::Medium,
        mediumDocumentPageSpacingAction
        );
    documentPageSpacingState->addOption(
        DocumentPageSpacing::Large,
        largeDocumentPageSpacingAction
        );

    documentPageSpacingState->loadFromSettings(
        DocumentPageSpacing::Small
        );

    documentViewerBackgroundState =
        new OptionState<DocumentViewerBackground>(
            OptionKeys::DocumentViewerBackground,
            this
            );

    auto defaultDocumentViewerBackgroundAction =
        createCheckableAction(
            tr("Default"),
            tr("Use the current theme's PDF viewer background")
            );

    auto whiteDocumentViewerBackgroundAction =
        createCheckableAction(
            tr("White"),
            tr("Use a white PDF viewer background")
            );

    auto blackDocumentViewerBackgroundAction =
        createCheckableAction(
            tr("Black"),
            tr("Use a black PDF viewer background")
            );

    documentViewerBackgroundState->addOption(
        DocumentViewerBackground::Default,
        defaultDocumentViewerBackgroundAction
        );
    documentViewerBackgroundState->addOption(
        DocumentViewerBackground::White,
        whiteDocumentViewerBackgroundAction
        );
    documentViewerBackgroundState->addOption(
        DocumentViewerBackground::Black,
        blackDocumentViewerBackgroundAction
        );

    documentViewerBackgroundState->loadFromSettings(
        DocumentViewerBackground::Default
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

#ifdef Q_OS_MACOS
    showPowerPointDataAccessNotice =
        createCheckableAction(
            tr("Show Data Access Notice Before Export"),
            tr("Show a notice before PowerPoint accesses its protected workspace")
            );
    showPowerPointDataAccessNotice->setChecked(
        SettingsManager::instance()
            .showPowerPointDataAccessNotice()
        );
    connect(
        showPowerPointDataAccessNotice,
        &QAction::toggled,
        this,
        [](bool enabled)
        {
            SettingsManager::instance()
                .setShowPowerPointDataAccessNotice(enabled);
        }
        );
#endif

}

// =========================================================
// Help Actions
// =========================================================

void ActionRegistry::createHelpActions()
{
    checkForUpdates =
        createAction(
            tr("Check for Updates..."),
            tr("Check for a newer version of ClassMngr")
            );

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
