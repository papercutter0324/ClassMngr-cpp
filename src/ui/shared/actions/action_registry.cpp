#include "action_registry.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/state/option_state.h"
#include "ui/shared/state/option_state_keys.h"
#include "ui/shared/state/ai_comment_options.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/styles/themed_icon_utils.h"
#include "next/platform/settings_manager_automatic_update_preferences_port.h"
#include "next/platform/settings_manager_ai_comment_custom_website_port.h"
#include "next/platform/settings_manager_ai_comment_provider_preferences_port.h"
#include "next/platform/settings_manager_font_size_preferences_port.h"
#include "next/platform/settings_manager_theme_preferences_port.h"
#include "next/platform/settings_manager_language_preferences_port.h"
#include "next/platform/settings_manager_document_page_spacing_preferences_port.h"
#include "next/platform/settings_manager_document_viewer_background_preferences_port.h"
#include "next/platform/settings_manager_ai_comment_voice_preferences_port.h"
#include "next/platform/settings_manager_powerpoint_data_access_notice_port.h"
#include "next/platform/settings_manager_save_mode_preferences_port.h"
#include "next/platform/settings_manager_sidebar_display_preferences_port.h"

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QApplication>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QStyle>
#include <QWidget>

#include <string>

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
    createPrintExportActions();
    createOptionActions();
    createHelpActions();
    createAdminActions();
}

void ActionRegistry::retranslate()
{
    updateActionText(
        newFile,
        tr("New Teacher Profile..."),
        tr("Create a new Teacher Profile")
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
        printCurrentPage,
        tr("Print"),
        tr("Print from the current page")
        );
    updateActionText(
        saveCurrentPageAs,
        tr("Save As..."),
        tr("Save output from the current page")
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
        upcomingBirthdays,
        tr("Upcoming Birthdays..."),
        tr("View staff birthdays for the next two weeks")
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
            themeState->action(Theme::SystemDefault),
            tr("System Default"),
            tr("Use the system theme")
            );
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

    if (aiCommentProviderState)
    {
        updateActionText(
            aiCommentProviderState->action(AiCommentProvider::ChatGPT),
            tr("ChatGPT"),
            tr("Open ChatGPT for AI comment prompts")
            );
        updateActionText(
            aiCommentProviderState->action(AiCommentProvider::Gemini),
            tr("Gemini"),
            tr("Open Gemini for AI comment prompts")
            );
        updateActionText(
            aiCommentProviderState->action(AiCommentProvider::Claude),
            tr("Claude"),
            tr("Open Claude for AI comment prompts")
            );
        updateActionText(
            aiCommentProviderState->action(
                AiCommentProvider::MicrosoftCopilot
                ),
            tr("Microsoft Copilot"),
            tr("Open Microsoft Copilot for AI comment prompts")
            );
        updateActionText(
            aiCommentProviderState->action(
                AiCommentProvider::CustomWebsite
                ),
            tr("Custom Website"),
            tr("Choose a custom HTTPS AI website")
            );
    }

    if (aiCommentVoiceState)
    {
        updateActionText(
            aiCommentVoiceState->action(
                AiCommentVoice::DirectToStudent
                ),
            tr("Direct to Student"),
            tr("Write AI comments directly to the student")
            );
        updateActionText(
            aiCommentVoiceState->action(
                AiCommentVoice::ThirdPerson
                ),
            tr("Third Person"),
            tr("Write AI comments for a parent or guardian")
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
    updateActionText(
        automaticallyCheckForUpdates,
        tr("Automatically Check for Updates"),
        tr("Check GitHub Releases for a newer version when ClassMngr starts")
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
            tr("New Teacher Profile..."),
            tr("Create a new Teacher Profile")
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

void ActionRegistry::createPrintExportActions()
{
    printCurrentPage =
        createAction(
            tr("Print"),
            tr("Print from the current page")
            );
    saveCurrentPageAs =
        createAction(
            tr("Save As..."),
            tr("Save output from the current page")
            );

    printCurrentPage->setEnabled(false);
    saveCurrentPageAs->setEnabled(false);
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

    upcomingBirthdays =
        createAction(
            tr("Upcoming Birthdays..."),
            tr("View staff birthdays for the next two weeks")
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

    saveModeState->onPersist = [](const SaveMode mode)
    {
        ClassMngr::Next::Application::SaveMode preference;
        switch (mode)
        {
        case SaveMode::Automatic:
            preference = ClassMngr::Next::Application::SaveMode::Automatic;
            break;

        case SaveMode::Manual:
            preference = ClassMngr::Next::Application::SaveMode::Manual;
            break;

        default:
            return;
        }

        ClassMngr::Next::Platform::
            SettingsManagerSaveModePreferencesPort().write(
                preference
                );
    };

    const auto storedSaveMode =
        ClassMngr::Next::Platform::
            SettingsManagerSaveModePreferencesPort()
            .read();
    saveModeState->set(
        static_cast<::SaveMode>(
            storedSaveMode
            )
        );


    themeState =
        new OptionState<Theme>(OptionKeys::Theme, this);

    auto systemDefaultThemeAction =
        createCheckableAction(
            tr("System Default"),
            tr("Use the system theme")
        );

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

    themeState->addOption(
        Theme::SystemDefault,
        systemDefaultThemeAction
        );
    themeState->addOption(Theme::Dark, darkThemeAction);
    themeState->addOption(Theme::Light, lightThemeAction);

    const auto storedTheme =
        ClassMngr::Next::Platform::
            SettingsManagerThemePreferencesPort()
            .read();
    ::Theme legacyTheme = ::Theme::SystemDefault;
    switch (storedTheme)
    {
    case ClassMngr::Next::Application::Theme::Dark:
        legacyTheme = ::Theme::Dark;
        break;
    case ClassMngr::Next::Application::Theme::Light:
        legacyTheme = ::Theme::Light;
        break;
    case ClassMngr::Next::Application::Theme::SystemDefault:
    default:
        break;
    }
    themeState->set(
        legacyTheme
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

    languageState->onPersist = [](const Language language)
    {
        ClassMngr::Next::Application::LanguagePreference preference;
        switch (language)
        {
        case Language::SystemDefault:
            preference = ClassMngr::Next::Application::
                LanguagePreference::SystemDefault;
            break;

        case Language::English:
            preference = ClassMngr::Next::Application::
                LanguagePreference::English;
            break;

        case Language::Korean:
            preference = ClassMngr::Next::Application::
                LanguagePreference::Korean;
            break;

        default:
            return;
        }

        ClassMngr::Next::Platform::
            SettingsManagerLanguagePreferencesPort().write(
                preference
                );
    };

    const auto storedLanguage =
        ClassMngr::Next::Platform::
            SettingsManagerLanguagePreferencesPort()
            .read();
    ::Language legacyLanguage = ::Language::SystemDefault;
    switch (storedLanguage)
    {
    case ClassMngr::Next::Application::LanguagePreference::English:
        legacyLanguage = ::Language::English;
        break;
    case ClassMngr::Next::Application::LanguagePreference::Korean:
        legacyLanguage = ::Language::Korean;
        break;
    case ClassMngr::Next::Application::LanguagePreference::SystemDefault:
    default:
        break;
    }
    languageState->set(
        legacyLanguage
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

    const auto storedFontSize =
        ClassMngr::Next::Platform::
            SettingsManagerFontSizePreferencesPort()
            .read();
    ::FontSize legacyFontSize = ::FontSize::Normal;
    switch (storedFontSize)
    {
    case ClassMngr::Next::Application::FontSize::Small:
        legacyFontSize = ::FontSize::Small;
        break;
    case ClassMngr::Next::Application::FontSize::Large:
        legacyFontSize = ::FontSize::Large;
        break;
    case ClassMngr::Next::Application::FontSize::ExtraLarge:
        legacyFontSize = ::FontSize::ExtraLarge;
        break;
    case ClassMngr::Next::Application::FontSize::Normal:
    default:
        break;
    }
    fontSizeState->set(
        legacyFontSize
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

    const auto storedDocumentPageSpacing =
        ClassMngr::Next::Platform::
            SettingsManagerDocumentPageSpacingPreferencesPort()
            .read();
    documentPageSpacingState->set(
        static_cast<::DocumentPageSpacing>(
            storedDocumentPageSpacing
            )
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

    const auto storedDocumentViewerBackground =
        ClassMngr::Next::Platform::
            SettingsManagerDocumentViewerBackgroundPreferencesPort()
            .read();
    documentViewerBackgroundState->set(
        static_cast<::DocumentViewerBackground>(
            storedDocumentViewerBackground
            )
        );

    aiCommentProviderState =
        new OptionState<AiCommentProvider>(
            OptionKeys::AiCommentProvider,
            this
            );

    aiCommentProviderState->addOption(
        AiCommentProvider::ChatGPT,
        createCheckableAction(
            tr("ChatGPT"),
            tr("Open ChatGPT for AI comment prompts")
            )
        );
    aiCommentProviderState->addOption(
        AiCommentProvider::Gemini,
        createCheckableAction(
            tr("Gemini"),
            tr("Open Gemini for AI comment prompts")
            )
        );
    aiCommentProviderState->addOption(
        AiCommentProvider::Claude,
        createCheckableAction(
            tr("Claude"),
            tr("Open Claude for AI comment prompts")
            )
        );
    aiCommentProviderState->addOption(
        AiCommentProvider::MicrosoftCopilot,
        createCheckableAction(
            tr("Microsoft Copilot"),
            tr("Open Microsoft Copilot for AI comment prompts")
            )
        );
    QAction* customAiWebsiteAction =
        aiCommentProviderState->addOption(
            AiCommentProvider::CustomWebsite,
            createCheckableAction(
                tr("Custom Website"),
                tr("Choose a custom HTTPS AI website")
                ),
            false
            );

    const auto storedAiCommentProvider =
        ClassMngr::Next::Platform::
            SettingsManagerAiCommentProviderPreferencesPort()
            .read();
    ::AiCommentProvider legacyAiCommentProvider =
        ::AiCommentProvider::ChatGPT;
    switch (storedAiCommentProvider)
    {
    case ClassMngr::Next::Application::AiCommentProvider::Gemini:
        legacyAiCommentProvider = ::AiCommentProvider::Gemini;
        break;
    case ClassMngr::Next::Application::AiCommentProvider::Claude:
        legacyAiCommentProvider = ::AiCommentProvider::Claude;
        break;
    case ClassMngr::Next::Application::AiCommentProvider::MicrosoftCopilot:
        legacyAiCommentProvider = ::AiCommentProvider::MicrosoftCopilot;
        break;
    case ClassMngr::Next::Application::AiCommentProvider::CustomWebsite:
        legacyAiCommentProvider = ::AiCommentProvider::CustomWebsite;
        break;
    case ClassMngr::Next::Application::AiCommentProvider::ChatGPT:
    default:
        break;
    }
    aiCommentProviderState->set(
        legacyAiCommentProvider
        );
    const ClassMngr::Next::Platform::
        SettingsManagerAiCommentCustomWebsitePort
        customWebsitePort;
    const std::string storedCustomAiWebsiteValue =
        customWebsitePort.read();
    const QString storedCustomAiWebsite =
        QString::fromUtf8(
            storedCustomAiWebsiteValue.data(),
            static_cast<qsizetype>(storedCustomAiWebsiteValue.size())
            );
    if (
        aiCommentProviderState->current()
            == AiCommentProvider::CustomWebsite
        && !isValidCustomAiWebsiteUrl(storedCustomAiWebsite)
        )
    {
        aiCommentProviderState->set(
            AiCommentProvider::ChatGPT
            );
    }

    connect(
        customAiWebsiteAction,
        &QAction::triggered,
        this,
        [this]()
        {
            const AiCommentProvider previousProvider =
                aiCommentProviderState->current();
            const ClassMngr::Next::Platform::
                SettingsManagerAiCommentCustomWebsitePort
                customWebsitePort;
            const std::string existingUrlValue =
                customWebsitePort.read();
            const QString existingUrl =
                QString::fromUtf8(
                    existingUrlValue.data(),
                    static_cast<qsizetype>(existingUrlValue.size())
                    );
            bool accepted = false;
            const QString enteredUrl =
                QInputDialog::getText(
                    qobject_cast<QWidget*>(parent()),
                    tr("Custom AI Website"),
                    tr("HTTPS URL:"),
                    QLineEdit::Normal,
                    existingUrl,
                    &accepted
                    ).trimmed();

            if (!accepted)
            {
                aiCommentProviderState
                    ->action(previousProvider)
                    ->setChecked(true);
                return;
            }

            if (!isValidCustomAiWebsiteUrl(enteredUrl))
            {
                aiCommentProviderState
                    ->action(previousProvider)
                    ->setChecked(true);
                DialogServices::showWarning(
                    qobject_cast<QWidget*>(parent()),
                    tr("Invalid AI Website"),
                    tr("Enter a valid HTTPS website URL.")
                    );
                return;
            }

            customWebsitePort.write(
                enteredUrl.toUtf8().toStdString()
                );
            aiCommentProviderState->set(
                AiCommentProvider::CustomWebsite
                );
        }
        );

    aiCommentVoiceState =
        new OptionState<AiCommentVoice>(
            OptionKeys::AiCommentVoice,
            this
            );
    aiCommentVoiceState->addOption(
        AiCommentVoice::DirectToStudent,
        createCheckableAction(
            tr("Direct to Student"),
            tr("Write AI comments directly to the student")
            )
        );
    aiCommentVoiceState->addOption(
        AiCommentVoice::ThirdPerson,
        createCheckableAction(
            tr("Third Person"),
            tr("Write AI comments for a parent or guardian")
            )
        );
    const auto storedAiCommentVoice =
        ClassMngr::Next::Platform::
            SettingsManagerAiCommentVoicePreferencesPort()
            .read();
    aiCommentVoiceState->set(
        storedAiCommentVoice ==
                ClassMngr::Next::Application::AiCommentVoice::ThirdPerson
            ? ::AiCommentVoice::ThirdPerson
            : ::AiCommentVoice::DirectToStudent
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

    ClassMngr::Next::Application::SidebarDisplayPreferences
        sidebarDisplayPreferences;
    ClassMngr::Next::Platform::
        SettingsManagerSidebarDisplayPreferencesPort
        sidebarDisplayPreferencesPort;
    if (const auto loaded = sidebarDisplayPreferencesPort.load(); loaded)
    {
        sidebarDisplayPreferences = loaded.value();
    }

    showSidebarTooltips->setChecked(
        sidebarDisplayPreferences.sidebarTooltipsEnabled
        );

    animateSidebarText->setChecked(
        sidebarDisplayPreferences.sidebarMarqueeEnabled
        );

    const auto saveSidebarDisplayPreferences = [this]()
    {
        ClassMngr::Next::Platform::
            SettingsManagerSidebarDisplayPreferencesPort port;
        (void)port.save({
            .sidebarTooltipsEnabled = showSidebarTooltips->isChecked(),
            .sidebarMarqueeEnabled = animateSidebarText->isChecked()
        });
    };

    connect(
        showSidebarTooltips,
        &QAction::toggled,
        this,
        [saveSidebarDisplayPreferences](const bool)
        {
            saveSidebarDisplayPreferences();
        }
        );

    connect(
        animateSidebarText,
        &QAction::toggled,
        this,
        [saveSidebarDisplayPreferences](const bool)
        {
            saveSidebarDisplayPreferences();
        }
        );

    automaticallyCheckForUpdates =
        createCheckableAction(
            tr("Automatically Check for Updates"),
            tr("Check GitHub Releases for a newer version when ClassMngr starts")
            );
    const ClassMngr::Next::Platform::
        SettingsManagerAutomaticUpdatePreferencesPort
        automaticUpdatePreferencesPort;
    automaticallyCheckForUpdates->setChecked(
        automaticUpdatePreferencesPort.read().automaticChecksEnabled
        );
    connect(
        automaticallyCheckForUpdates,
        &QAction::toggled,
        this,
        [](bool enabled)
        {
            const ClassMngr::Next::Platform::
                SettingsManagerAutomaticUpdatePreferencesPort port;
            port.write({
                .automaticChecksEnabled = enabled
            });
        }
        );

#ifdef Q_OS_MACOS
    showPowerPointDataAccessNotice =
        createCheckableAction(
            tr("Show Data Access Notice Before Export"),
            tr("Show a notice before PowerPoint accesses its protected workspace")
            );
    const ClassMngr::Next::Platform::
        SettingsManagerPowerPointDataAccessNoticePort
        powerPointDataAccessNoticePort;
    showPowerPointDataAccessNotice->setChecked(
        powerPointDataAccessNoticePort.read()
            .showPowerPointDataAccessNotice
        );
    connect(
        showPowerPointDataAccessNotice,
        &QAction::toggled,
        this,
        [](bool enabled)
        {
            const ClassMngr::Next::Platform::
                SettingsManagerPowerPointDataAccessNoticePort port;
            port.write({
                .showPowerPointDataAccessNotice = enabled
            });
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
