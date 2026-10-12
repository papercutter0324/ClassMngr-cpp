#include "action_registry.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/state/option_state.h"
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
#include <QSignalBlocker>
#include <QStyle>
#include <QWidget>

#include <string>
#include <utility>

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

void ActionRegistry::createActions(
    std::shared_ptr<
        ClassMngr::Next::Application::SaveModePreferencesPort
        > saveModePreferencesPort,
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> saveModePreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::AutomaticUpdatePreferencesPort
        > automaticUpdatePreferencesPort,
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> automaticUpdatePreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::FontSizePreferencesPort
        > fontSizePreferencesPort,
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> fontSizePreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::AiCommentVoicePreferencesPort
        > aiCommentVoicePreferencesPort,
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> aiCommentVoicePreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::AiCommentProviderPreferencesPort
        > aiCommentProviderPreferencesPort,
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> aiCommentProviderPreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::LanguagePreferencesPort
        > languagePreferencesPort,
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> languagePreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::SidebarDisplayPreferencesPort
        > sidebarDisplayPreferencesPort,
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> sidebarDisplayPreferencesErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::DocumentViewerBackgroundPreferencesPort
        > documentViewerBackgroundPreferencesPort,
    std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )> documentViewerBackgroundPreferenceErrorHandler
    )
{
    createFileActions();
    createEditActions();
    createClassActions();
    createPrintExportActions();
    createOptionActions(
        std::move(saveModePreferencesPort),
        saveModePreferenceErrorHandler,
        std::move(automaticUpdatePreferencesPort),
        automaticUpdatePreferenceErrorHandler,
        std::move(fontSizePreferencesPort),
        fontSizePreferenceErrorHandler,
        std::move(aiCommentVoicePreferencesPort),
        aiCommentVoicePreferenceErrorHandler,
        std::move(aiCommentProviderPreferencesPort),
        aiCommentProviderPreferenceErrorHandler,
        std::move(languagePreferencesPort),
        languagePreferenceErrorHandler,
        std::move(sidebarDisplayPreferencesPort),
        sidebarDisplayPreferencesErrorHandler,
        std::move(documentViewerBackgroundPreferencesPort),
        documentViewerBackgroundPreferenceErrorHandler
        );
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

void ActionRegistry::createOptionActions(
    std::shared_ptr<
        ClassMngr::Next::Application::SaveModePreferencesPort
        > saveModePreferencesPort,
    const std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )>& saveModePreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::AutomaticUpdatePreferencesPort
        > automaticUpdatePreferencesPort,
    const std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )>& automaticUpdatePreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::FontSizePreferencesPort
        > fontSizePreferencesPort,
    const std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )>& fontSizePreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::AiCommentVoicePreferencesPort
        > aiCommentVoicePreferencesPort,
    const std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )>& aiCommentVoicePreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::AiCommentProviderPreferencesPort
        > aiCommentProviderPreferencesPort,
    const std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )>& aiCommentProviderPreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::LanguagePreferencesPort
        > languagePreferencesPort,
    const std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )>& languagePreferenceErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::SidebarDisplayPreferencesPort
        > sidebarDisplayPreferencesPort,
    const std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )>& sidebarDisplayPreferencesErrorHandler,
    std::shared_ptr<
        ClassMngr::Next::Application::DocumentViewerBackgroundPreferencesPort
        > documentViewerBackgroundPreferencesPort,
    const std::function<void(
        const ClassMngr::Next::Domain::OperationError&
        )>& documentViewerBackgroundPreferenceErrorHandler
    )
{
    using ClassMngr::Next::Application::SaveModePreferencesPort;
    using PreferenceSaveMode = ClassMngr::Next::Application::SaveMode;

    saveModeState =
        new OptionState<::SaveMode>(this);

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

    saveModeState->addOption(
        ::SaveMode::Automatic,
        automaticSaveAction,
        false
        );
    saveModeState->addOption(
        ::SaveMode::Manual,
        manualSaveAction,
        false
        );

    if (!saveModePreferencesPort)
    {
        saveModePreferencesPort =
            std::make_shared<
                ClassMngr::Next::Platform::
                    SettingsManagerSaveModePreferencesPort
                >();
    }

    const auto storedSaveMode = saveModePreferencesPort->read();
    saveModeState->set(
        static_cast<::SaveMode>(
            storedSaveMode
            )
        );

    const auto writeSaveModePreference =
        [saveModePreferencesPort](const ::SaveMode mode)
            -> ClassMngr::Next::Domain::Result<void>
        {
            PreferenceSaveMode preference =
                PreferenceSaveMode::Automatic;
            switch (mode)
            {
            case ::SaveMode::Automatic:
                preference = PreferenceSaveMode::Automatic;
                break;

            case ::SaveMode::Manual:
                preference = PreferenceSaveMode::Manual;
                break;

            default:
                return ClassMngr::Next::Domain::Result<void>::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::InvalidInput,
                    .message = "Save Mode must be Automatic or Manual.",
                    .recoverable = false
                });
            }

            return saveModePreferencesPort->write(preference);
        };

    // Preserve programmatic OptionState::set() writes for existing callers.
    // QAction changes use the persistence-first handler below and suppress the
    // resulting onPersist callback for exactly that one successful commit.
    const auto skipPersistForActionCommit =
        std::make_shared<bool>(false);
    saveModeState->onPersist = [
        writeSaveModePreference,
        saveModePreferenceErrorHandler,
        skipPersistForActionCommit
    ](const ::SaveMode mode)
    {
        if (*skipPersistForActionCommit)
        {
            *skipPersistForActionCommit = false;
            return;
        }

        const auto saved = writeSaveModePreference(mode);
        if (!saved && saveModePreferenceErrorHandler)
        {
            saveModePreferenceErrorHandler(saved.error());
        }
    };

    const auto connectSaveModeAction =
        [this, writeSaveModePreference, &saveModePreferenceErrorHandler,
            skipPersistForActionCommit](
            QAction* action,
            const ::SaveMode mode
            )
        {
            connect(
                action,
                &QAction::triggered,
                this,
                [this, writeSaveModePreference,
                    saveModePreferenceErrorHandler,
                    skipPersistForActionCommit, mode]()
                {
                    if (!saveModeState || saveModeState->current() == mode)
                    {
                        return;
                    }

                    const auto saved =
                        writeSaveModePreference(mode);
                    if (!saved)
                    {
                        if (QAction* previousAction =
                                saveModeState->action(
                                    saveModeState->current()
                                    ))
                        {
                            previousAction->setChecked(true);
                        }

                        if (saveModePreferenceErrorHandler)
                        {
                            saveModePreferenceErrorHandler(saved.error());
                        }
                        return;
                    }

                    *skipPersistForActionCommit = true;
                    saveModeState->set(mode);
                }
                );
        };

    connectSaveModeAction(
        automaticSaveAction,
        ::SaveMode::Automatic
        );
    connectSaveModeAction(
        manualSaveAction,
        ::SaveMode::Manual
        );


    themeState =
        new OptionState<Theme>(this);

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

    const auto themePreferencesPort = std::make_shared<
        ClassMngr::Next::Platform::SettingsManagerThemePreferencesPort
        >();
    const auto storedTheme = themePreferencesPort->read();
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

    // Programmatic OptionState changes, including initial hydration, retain
    // the legacy typed persistence behavior. ThemeController's pre-commit
    // gate takes ownership after these actions are connected.
    themeState->onPersist = [themePreferencesPort](const ::Theme theme)
    {
        using PreferenceTheme =
            ClassMngr::Next::Application::Theme;
        PreferenceTheme preference = PreferenceTheme::SystemDefault;
        switch (theme)
        {
        case ::Theme::Dark:
            preference = PreferenceTheme::Dark;
            break;
        case ::Theme::Light:
            preference = PreferenceTheme::Light;
            break;
        case ::Theme::SystemDefault:
            preference = PreferenceTheme::SystemDefault;
            break;
        }

        (void) themePreferencesPort->write(preference);
    };

    themeState->set(
        legacyTheme
        );

    languageState =
        new OptionState<Language>(this);

    QAction* systemDefaultLanguageAction =
        createCheckableAction(
            tr("System Default"),
            tr("Use the system language")
            );

    QAction* englishAction =
        createCheckableAction(
            tr("English"),
            tr("Use English")
        );

    QAction* koreanLanguageAction =
        createCheckableAction(
            tr("Korean"),
            tr("Use Korean")
            );

    languageState->addOption(
        Language::SystemDefault,
        systemDefaultLanguageAction,
        false
        );
    languageState->addOption(
        Language::English,
        englishAction,
        false
        );
    languageState->addOption(
        Language::Korean,
        koreanLanguageAction,
        false
        );

    if (!languagePreferencesPort)
    {
        languagePreferencesPort = std::make_shared<
            ClassMngr::Next::Platform::
                SettingsManagerLanguagePreferencesPort
            >();
    }

    using PreferenceLanguage =
        ClassMngr::Next::Application::LanguagePreference;
    const auto writeLanguagePreference =
        [languagePreferencesPort](const ::Language language)
            -> ClassMngr::Next::Domain::Result<void>
        {
            switch (language)
            {
            case ::Language::SystemDefault:
                return languagePreferencesPort->write(
                    PreferenceLanguage::SystemDefault
                    );

            case ::Language::English:
                return languagePreferencesPort->write(
                    PreferenceLanguage::English
                    );

            case ::Language::Korean:
                return languagePreferencesPort->write(
                    PreferenceLanguage::Korean
                    );

            default:
                return ClassMngr::Next::Domain::Result<void>::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::InvalidInput,
                    .message = "Language selection is invalid.",
                    .recoverable = false
                });
            }
        };

    const auto storedLanguage =
        languagePreferencesPort->read();
    ::Language legacyLanguage = ::Language::SystemDefault;
    switch (storedLanguage)
    {
    case PreferenceLanguage::English:
        legacyLanguage = ::Language::English;
        break;
    case PreferenceLanguage::Korean:
        legacyLanguage = ::Language::Korean;
        break;
    case PreferenceLanguage::SystemDefault:
    default:
        break;
    }
    languageState->set(
        legacyLanguage
        );

    const auto skipLanguagePersistForActionCommit =
        std::make_shared<bool>(false);
    languageState->onPersist = [writeLanguagePreference,
        languagePreferenceErrorHandler,
        skipLanguagePersistForActionCommit](const ::Language language)
    {
        if (*skipLanguagePersistForActionCommit)
        {
            *skipLanguagePersistForActionCommit = false;
            return;
        }

        // Programmatic state commits retain their prior callback behavior;
        // QAction commits persist before changing this state.
        const auto saved = writeLanguagePreference(language);
        if (!saved && languagePreferenceErrorHandler)
        {
            languagePreferenceErrorHandler(saved.error());
        }
    };

    const auto restoreLanguageAction = [this](
        const ::Language previousLanguage
        )
    {
        if (QAction* previousAction =
                languageState->action(previousLanguage))
        {
            previousAction->setChecked(true);
        }
    };
    const auto connectLanguageAction =
        [this, writeLanguagePreference, languagePreferenceErrorHandler,
            skipLanguagePersistForActionCommit, restoreLanguageAction](
            QAction* action,
            const ::Language language
            )
        {
            connect(
                action,
                &QAction::triggered,
                this,
                [this, action, writeLanguagePreference,
                    languagePreferenceErrorHandler,
                    skipLanguagePersistForActionCommit,
                    restoreLanguageAction, language]()
                {
                    if (!languageState)
                    {
                        return;
                    }

                    if (languageState->current() == language)
                    {
                        if (QAction* selectedAction =
                                languageState->action(language))
                        {
                            selectedAction->setChecked(true);
                        }
                        return;
                    }

                    const ::Language previousLanguage =
                        languageState->current();
                    const auto saved = writeLanguagePreference(language);
                    if (!saved)
                    {
                        restoreLanguageAction(previousLanguage);
                        action->setChecked(false);
                        if (languagePreferenceErrorHandler)
                        {
                            languagePreferenceErrorHandler(saved.error());
                        }
                        return;
                    }

                    *skipLanguagePersistForActionCommit = true;
                    languageState->set(language);
                }
                );
        };

    connectLanguageAction(
        systemDefaultLanguageAction,
        ::Language::SystemDefault
        );
    connectLanguageAction(
        englishAction,
        ::Language::English
        );
    connectLanguageAction(
        koreanLanguageAction,
        ::Language::Korean
        );

    fontSizeState =
        new OptionState<FontSize>(this);

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
        smallFontAction,
        false
        );
    fontSizeState->addOption(
        FontSize::Normal,
        normalFontAction,
        false
        );
    fontSizeState->addOption(
        FontSize::Large,
        largeFontAction,
        false
        );
    fontSizeState->addOption(
        FontSize::ExtraLarge,
        extraLargeFontAction,
        false
        );

    if (!fontSizePreferencesPort)
    {
        fontSizePreferencesPort = std::make_shared<
            ClassMngr::Next::Platform::
                SettingsManagerFontSizePreferencesPort
            >();
    }

    const auto writeFontSizePreference =
        [fontSizePreferencesPort](const ::FontSize fontSize)
            -> ClassMngr::Next::Domain::Result<void>
        {
            ClassMngr::Next::Application::FontSize preference;
            switch (fontSize)
            {
            case ::FontSize::Small:
                preference =
                    ClassMngr::Next::Application::FontSize::Small;
                break;

            case ::FontSize::Normal:
                preference =
                    ClassMngr::Next::Application::FontSize::Normal;
                break;

            case ::FontSize::Large:
                preference =
                    ClassMngr::Next::Application::FontSize::Large;
                break;

            case ::FontSize::ExtraLarge:
                preference =
                    ClassMngr::Next::Application::FontSize::ExtraLarge;
                break;

            default:
                return ClassMngr::Next::Domain::Result<void>::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::InvalidInput,
                    .message =
                        "Font Size must be Small, Normal, Large, or Extra Large.",
                    .recoverable = false
                });
            }

            return fontSizePreferencesPort->write(preference);
        };

    // Preserve programmatic OptionState::set() writes for existing callers.
    // QAction changes persist first and suppress the duplicate onPersist call
    // for the one successful state commit.
    const auto skipFontSizePersistForActionCommit =
        std::make_shared<bool>(false);

    const auto connectFontSizeAction =
        [this, writeFontSizePreference, fontSizePreferenceErrorHandler,
            skipFontSizePersistForActionCommit](
            QAction* action,
            const ::FontSize fontSize
            )
        {
            connect(
                action,
                &QAction::triggered,
                this,
                [this, writeFontSizePreference,
                    fontSizePreferenceErrorHandler,
                    skipFontSizePersistForActionCommit,
                    fontSizeSize = fontSize]()
                {
                    if (!fontSizeState
                        || fontSizeState->current() == fontSizeSize)
                    {
                        return;
                    }

                    const auto saved =
                        writeFontSizePreference(fontSizeSize);
                    if (!saved)
                    {
                        if (QAction* previousAction =
                                fontSizeState->action(
                                    fontSizeState->current()
                                    ))
                        {
                            previousAction->setChecked(true);
                        }

                        if (fontSizePreferenceErrorHandler)
                        {
                            fontSizePreferenceErrorHandler(saved.error());
                        }
                        return;
                    }

                    *skipFontSizePersistForActionCommit = true;
                    fontSizeState->set(fontSizeSize);
                }
                );
        };

    connectFontSizeAction(smallFontAction, ::FontSize::Small);
    connectFontSizeAction(normalFontAction, ::FontSize::Normal);
    connectFontSizeAction(largeFontAction, ::FontSize::Large);
    connectFontSizeAction(extraLargeFontAction, ::FontSize::ExtraLarge);

    const auto storedFontSize = fontSizePreferencesPort->read();
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

    // Hydration selects the stored option without writing it back. Programmatic
    // state changes made after initialization retain the legacy persistence
    // callback, while QAction changes use the checked persistence-first path.
    fontSizeState->onPersist = [
        writeFontSizePreference,
        fontSizePreferenceErrorHandler,
        skipFontSizePersistForActionCommit
    ](const ::FontSize fontSize)
    {
        if (*skipFontSizePersistForActionCommit)
        {
            *skipFontSizePersistForActionCommit = false;
            return;
        }

        const auto saved = writeFontSizePreference(fontSize);
        if (!saved && fontSizePreferenceErrorHandler)
        {
            fontSizePreferenceErrorHandler(saved.error());
        }
    };

    documentPageSpacingState =
        new OptionState<DocumentPageSpacing>(this);

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

    documentPageSpacingState->onPersist =
        [](const ::DocumentPageSpacing spacing)
    {
        ClassMngr::Next::Application::DocumentPageSpacing preference;
        switch (spacing)
        {
        case ::DocumentPageSpacing::None:
            preference = ClassMngr::Next::Application::
                DocumentPageSpacing::None;
            break;

        case ::DocumentPageSpacing::Small:
            preference = ClassMngr::Next::Application::
                DocumentPageSpacing::Small;
            break;

        case ::DocumentPageSpacing::Medium:
            preference = ClassMngr::Next::Application::
                DocumentPageSpacing::Medium;
            break;

        case ::DocumentPageSpacing::Large:
            preference = ClassMngr::Next::Application::
                DocumentPageSpacing::Large;
            break;

        default:
            return;
        }

        ClassMngr::Next::Platform::
            SettingsManagerDocumentPageSpacingPreferencesPort().write(
                preference
                );
    };

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
        new OptionState<DocumentViewerBackground>(this);

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

    if (!documentViewerBackgroundPreferencesPort)
    {
        documentViewerBackgroundPreferencesPort = std::make_shared<
            ClassMngr::Next::Platform::
                SettingsManagerDocumentViewerBackgroundPreferencesPort
            >();
    }

    const auto storedDocumentViewerBackground =
        documentViewerBackgroundPreferencesPort->read();
    ::DocumentViewerBackground legacyDocumentViewerBackground =
        ::DocumentViewerBackground::Default;
    switch (storedDocumentViewerBackground)
    {
    case ClassMngr::Next::Application::DocumentViewerBackground::White:
        legacyDocumentViewerBackground = ::DocumentViewerBackground::White;
        break;
    case ClassMngr::Next::Application::DocumentViewerBackground::Black:
        legacyDocumentViewerBackground = ::DocumentViewerBackground::Black;
        break;
    case ClassMngr::Next::Application::DocumentViewerBackground::Default:
    default:
        break;
    }
    documentViewerBackgroundState->set(
        legacyDocumentViewerBackground
        );

    using PreferenceDocumentViewerBackground =
        ClassMngr::Next::Application::DocumentViewerBackground;
    documentViewerBackgroundState->onBeforeCommit =
        [documentViewerBackgroundPreferencesPort](
            const ::DocumentViewerBackground background
            ) -> ClassMngr::Next::Domain::Result<void>
        {
            switch (background)
            {
            case ::DocumentViewerBackground::Default:
                return documentViewerBackgroundPreferencesPort->write(
                    PreferenceDocumentViewerBackground::Default
                    );

            case ::DocumentViewerBackground::White:
                return documentViewerBackgroundPreferencesPort->write(
                    PreferenceDocumentViewerBackground::White
                    );

            case ::DocumentViewerBackground::Black:
                return documentViewerBackgroundPreferencesPort->write(
                    PreferenceDocumentViewerBackground::Black
                    );

            default:
                return ClassMngr::Next::Domain::Result<void>::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::InvalidInput,
                    .message =
                        "Document Viewer Background selection is invalid.",
                    .recoverable = false
                });
            }
        };
    documentViewerBackgroundState->onCommitFailed =
        documentViewerBackgroundPreferenceErrorHandler;

    aiCommentProviderState =
        new OptionState<AiCommentProvider>(this);

    QAction* chatGptProviderAction = aiCommentProviderState->addOption(
        AiCommentProvider::ChatGPT,
        createCheckableAction(
            tr("ChatGPT"),
            tr("Open ChatGPT for AI comment prompts")
            ),
        false
        );
    QAction* geminiProviderAction = aiCommentProviderState->addOption(
        AiCommentProvider::Gemini,
        createCheckableAction(
            tr("Gemini"),
            tr("Open Gemini for AI comment prompts")
            ),
        false
        );
    QAction* claudeProviderAction = aiCommentProviderState->addOption(
        AiCommentProvider::Claude,
        createCheckableAction(
            tr("Claude"),
            tr("Open Claude for AI comment prompts")
            ),
        false
        );
    QAction* copilotProviderAction = aiCommentProviderState->addOption(
        AiCommentProvider::MicrosoftCopilot,
        createCheckableAction(
            tr("Microsoft Copilot"),
            tr("Open Microsoft Copilot for AI comment prompts")
            ),
        false
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

    if (!aiCommentProviderPreferencesPort)
    {
        aiCommentProviderPreferencesPort = std::make_shared<
            ClassMngr::Next::Platform::
                SettingsManagerAiCommentProviderPreferencesPort
            >();
    }

    using PreferenceAiCommentProvider =
        ClassMngr::Next::Application::AiCommentProvider;
    const auto writeAiCommentProviderPreference =
        [aiCommentProviderPreferencesPort](
            const ::AiCommentProvider provider
            ) -> ClassMngr::Next::Domain::Result<void>
        {
            switch (provider)
            {
            case ::AiCommentProvider::ChatGPT:
                return aiCommentProviderPreferencesPort->write(
                    PreferenceAiCommentProvider::ChatGPT
                    );

            case ::AiCommentProvider::Gemini:
                return aiCommentProviderPreferencesPort->write(
                    PreferenceAiCommentProvider::Gemini
                    );

            case ::AiCommentProvider::Claude:
                return aiCommentProviderPreferencesPort->write(
                    PreferenceAiCommentProvider::Claude
                    );

            case ::AiCommentProvider::MicrosoftCopilot:
                return aiCommentProviderPreferencesPort->write(
                    PreferenceAiCommentProvider::MicrosoftCopilot
                    );

            case ::AiCommentProvider::CustomWebsite:
                return aiCommentProviderPreferencesPort->write(
                    PreferenceAiCommentProvider::CustomWebsite
                    );

            default:
                return ClassMngr::Next::Domain::Result<void>::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::InvalidInput,
                    .message = "AI Comment Provider selection is invalid.",
                    .recoverable = false
                });
            }
        };

    const auto storedAiCommentProvider =
        aiCommentProviderPreferencesPort->read();
    ::AiCommentProvider legacyAiCommentProvider =
        ::AiCommentProvider::ChatGPT;
    switch (storedAiCommentProvider)
    {
    case PreferenceAiCommentProvider::Gemini:
        legacyAiCommentProvider = ::AiCommentProvider::Gemini;
        break;
    case PreferenceAiCommentProvider::Claude:
        legacyAiCommentProvider = ::AiCommentProvider::Claude;
        break;
    case PreferenceAiCommentProvider::MicrosoftCopilot:
        legacyAiCommentProvider = ::AiCommentProvider::MicrosoftCopilot;
        break;
    case PreferenceAiCommentProvider::CustomWebsite:
        legacyAiCommentProvider = ::AiCommentProvider::CustomWebsite;
        break;
    case PreferenceAiCommentProvider::ChatGPT:
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

    const auto skipAiCommentProviderPersistForActionCommit =
        std::make_shared<bool>(false);
    aiCommentProviderState->onPersist =
        [writeAiCommentProviderPreference,
            aiCommentProviderPreferenceErrorHandler,
            skipAiCommentProviderPersistForActionCommit](
            const ::AiCommentProvider provider
            )
        {
            if (*skipAiCommentProviderPersistForActionCommit)
            {
                *skipAiCommentProviderPersistForActionCommit = false;
                return;
            }

            // Preserve the existing programmatic OptionState::set behavior;
            // action-triggered writes use the checked path before set().
            const auto saved = writeAiCommentProviderPreference(provider);
            if (!saved && aiCommentProviderPreferenceErrorHandler)
            {
                aiCommentProviderPreferenceErrorHandler(saved.error());
            }
        };

    const auto restoreProviderAction = [this](
        const ::AiCommentProvider previousProvider
        )
    {
        if (QAction* previousAction =
                aiCommentProviderState->action(previousProvider))
        {
            previousAction->setChecked(true);
        }
    };
    const auto connectProviderAction =
        [this, writeAiCommentProviderPreference,
            aiCommentProviderPreferenceErrorHandler,
            skipAiCommentProviderPersistForActionCommit,
            restoreProviderAction](
            QAction* action,
            const ::AiCommentProvider provider
            )
        {
            connect(
                action,
                &QAction::triggered,
                this,
                [this, action, writeAiCommentProviderPreference,
                    aiCommentProviderPreferenceErrorHandler,
                    skipAiCommentProviderPersistForActionCommit,
                    restoreProviderAction, provider]()
                {
                    if (!aiCommentProviderState)
                    {
                        return;
                    }

                    if (aiCommentProviderState->current() == provider)
                    {
                        if (QAction* selectedAction =
                                aiCommentProviderState->action(provider))
                        {
                            selectedAction->setChecked(true);
                        }
                        return;
                    }

                    const ::AiCommentProvider previousProvider =
                        aiCommentProviderState->current();
                    const auto saved =
                        writeAiCommentProviderPreference(provider);
                    if (!saved)
                    {
                        restoreProviderAction(previousProvider);
                        action->setChecked(false);
                        if (aiCommentProviderPreferenceErrorHandler)
                        {
                            aiCommentProviderPreferenceErrorHandler(
                                saved.error()
                                );
                        }
                        return;
                    }

                    *skipAiCommentProviderPersistForActionCommit = true;
                    aiCommentProviderState->set(provider);
                }
                );
        };

    connectProviderAction(
        chatGptProviderAction,
        ::AiCommentProvider::ChatGPT
        );
    connectProviderAction(
        geminiProviderAction,
        ::AiCommentProvider::Gemini
        );
    connectProviderAction(
        claudeProviderAction,
        ::AiCommentProvider::Claude
        );
    connectProviderAction(
        copilotProviderAction,
        ::AiCommentProvider::MicrosoftCopilot
        );

    connect(
        customAiWebsiteAction,
        &QAction::triggered,
        this,
        [this, customAiWebsiteAction,
            aiCommentProviderPreferenceErrorHandler,
            writeAiCommentProviderPreference,
            skipAiCommentProviderPersistForActionCommit,
            restoreProviderAction]()
        {
            const ::AiCommentProvider previousProvider =
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
                restoreProviderAction(previousProvider);
                return;
            }

            if (!isValidCustomAiWebsiteUrl(enteredUrl))
            {
                restoreProviderAction(previousProvider);
                DialogServices::showWarning(
                    qobject_cast<QWidget*>(parent()),
                    tr("Invalid AI Website"),
                    tr("Enter a valid HTTPS website URL.")
                    );
                return;
            }

            if (previousProvider != ::AiCommentProvider::CustomWebsite)
            {
                const auto saved = writeAiCommentProviderPreference(
                    ::AiCommentProvider::CustomWebsite
                    );
                if (!saved)
                {
                    restoreProviderAction(previousProvider);
                    customAiWebsiteAction->setChecked(false);
                    if (aiCommentProviderPreferenceErrorHandler)
                    {
                        aiCommentProviderPreferenceErrorHandler(
                            saved.error()
                            );
                    }
                    return;
                }
            }

            // Keep the established URL adapter behavior, but only after the
            // provider preference has committed successfully.
            customWebsitePort.write(
                enteredUrl.toUtf8().toStdString()
                );
            if (previousProvider != ::AiCommentProvider::CustomWebsite)
            {
                *skipAiCommentProviderPersistForActionCommit = true;
            }
            aiCommentProviderState->set(
                ::AiCommentProvider::CustomWebsite
                );
        }
        );

    aiCommentVoiceState =
        new OptionState<AiCommentVoice>(this);
    auto directToStudentVoiceAction =
        createCheckableAction(
            tr("Direct to Student"),
            tr("Write AI comments directly to the student")
            );
    auto thirdPersonVoiceAction =
        createCheckableAction(
            tr("Third Person"),
            tr("Write AI comments for a parent or guardian")
            );
    aiCommentVoiceState->addOption(
        AiCommentVoice::DirectToStudent,
        directToStudentVoiceAction,
        false
        );
    aiCommentVoiceState->addOption(
        AiCommentVoice::ThirdPerson,
        thirdPersonVoiceAction,
        false
        );

    if (!aiCommentVoicePreferencesPort)
    {
        aiCommentVoicePreferencesPort = std::make_shared<
            ClassMngr::Next::Platform::
                SettingsManagerAiCommentVoicePreferencesPort
            >();
    }

    using PreferenceAiCommentVoice =
        ClassMngr::Next::Application::AiCommentVoice;
    const auto writeAiCommentVoicePreference =
        [aiCommentVoicePreferencesPort](const ::AiCommentVoice voice)
            -> ClassMngr::Next::Domain::Result<void>
        {
            PreferenceAiCommentVoice preference;
            switch (voice)
            {
            case ::AiCommentVoice::DirectToStudent:
                preference = PreferenceAiCommentVoice::DirectToStudent;
                break;

            case ::AiCommentVoice::ThirdPerson:
                preference = PreferenceAiCommentVoice::ThirdPerson;
                break;

            default:
                return ClassMngr::Next::Domain::Result<void>::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::InvalidInput,
                    .message =
                        "AI Comment Voice must be Direct to Student or Third Person.",
                    .recoverable = false
                });
            }

            return aiCommentVoicePreferencesPort->write(preference);
        };

    const auto skipAiCommentVoicePersistForActionCommit =
        std::make_shared<bool>(false);
    const auto connectAiCommentVoiceAction =
        [this, writeAiCommentVoicePreference,
            aiCommentVoicePreferenceErrorHandler,
            skipAiCommentVoicePersistForActionCommit](
            QAction* action,
            const ::AiCommentVoice voice
            )
        {
            connect(
                action,
                &QAction::triggered,
                this,
                [this, action, writeAiCommentVoicePreference,
                    aiCommentVoicePreferenceErrorHandler,
                    skipAiCommentVoicePersistForActionCommit, voice]()
                {
                    if (!aiCommentVoiceState
                        || aiCommentVoiceState->current() == voice)
                    {
                        return;
                    }

                    const auto saved =
                        writeAiCommentVoicePreference(voice);
                    if (!saved)
                    {
                        if (QAction* previousAction =
                                aiCommentVoiceState->action(
                                    aiCommentVoiceState->current()
                                    ))
                        {
                            previousAction->setChecked(true);
                        }
                        else
                        {
                            action->setChecked(false);
                        }

                        if (aiCommentVoicePreferenceErrorHandler)
                        {
                            aiCommentVoicePreferenceErrorHandler(
                                saved.error()
                                );
                        }
                        return;
                    }

                    *skipAiCommentVoicePersistForActionCommit = true;
                    aiCommentVoiceState->set(voice);
                }
                );
        };

    connectAiCommentVoiceAction(
        directToStudentVoiceAction,
        ::AiCommentVoice::DirectToStudent
        );
    connectAiCommentVoiceAction(
        thirdPersonVoiceAction,
        ::AiCommentVoice::ThirdPerson
        );

    const auto storedAiCommentVoice =
        aiCommentVoicePreferencesPort->read();
    aiCommentVoiceState->set(
        storedAiCommentVoice == PreferenceAiCommentVoice::ThirdPerson
            ? ::AiCommentVoice::ThirdPerson
            : ::AiCommentVoice::DirectToStudent
        );

    // Attach after startup hydration so selecting the stored value does not
    // write it back. Programmatic OptionState::set() retains its established
    // persistence callback; action changes use the checked path above.
    aiCommentVoiceState->onPersist = [
        writeAiCommentVoicePreference,
        aiCommentVoicePreferenceErrorHandler,
        skipAiCommentVoicePersistForActionCommit
    ](const ::AiCommentVoice voice)
    {
        if (*skipAiCommentVoicePersistForActionCommit)
        {
            *skipAiCommentVoicePersistForActionCommit = false;
            return;
        }

        const auto saved = writeAiCommentVoicePreference(voice);
        if (!saved && aiCommentVoicePreferenceErrorHandler)
        {
            aiCommentVoicePreferenceErrorHandler(saved.error());
        }
    };


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
    if (!sidebarDisplayPreferencesPort)
    {
        sidebarDisplayPreferencesPort = std::make_shared<
            ClassMngr::Next::Platform::
                SettingsManagerSidebarDisplayPreferencesPort
            >();
    }

    if (const auto loaded = sidebarDisplayPreferencesPort->load(); loaded)
    {
        sidebarDisplayPreferences = loaded.value();
    }

    showSidebarTooltips->setChecked(
        sidebarDisplayPreferences.sidebarTooltipsEnabled
        );

    animateSidebarText->setChecked(
        sidebarDisplayPreferences.sidebarMarqueeEnabled
        );

    const auto lastSavedSidebarDisplayPreferences =
        std::make_shared<
            ClassMngr::Next::Application::SidebarDisplayPreferences
            >(sidebarDisplayPreferences);
    const auto skipSidebarDisplaySaveForRollback =
        std::make_shared<bool>(false);
    const auto saveSidebarDisplayPreferences =
        [this, sidebarDisplayPreferencesPort,
            sidebarDisplayPreferencesErrorHandler,
            lastSavedSidebarDisplayPreferences,
            skipSidebarDisplaySaveForRollback](QAction* triggeringAction)
    {
        if (*skipSidebarDisplaySaveForRollback)
        {
            *skipSidebarDisplaySaveForRollback = false;
            return;
        }

        const ClassMngr::Next::Application::SidebarDisplayPreferences candidate{
            .sidebarTooltipsEnabled = showSidebarTooltips->isChecked(),
            .sidebarMarqueeEnabled = animateSidebarText->isChecked()
        };
        const auto saved = sidebarDisplayPreferencesPort->save(candidate);
        if (saved)
        {
            *lastSavedSidebarDisplayPreferences = candidate;
            return;
        }

        const bool previousChecked = triggeringAction == showSidebarTooltips
            ? lastSavedSidebarDisplayPreferences->sidebarTooltipsEnabled
            : lastSavedSidebarDisplayPreferences->sidebarMarqueeEnabled;
        *skipSidebarDisplaySaveForRollback = true;
        triggeringAction->setChecked(previousChecked);
        // setChecked emits toggled synchronously when it changes the value;
        // the nested handler consumes the flag. Clear it if it did not emit.
        if (*skipSidebarDisplaySaveForRollback)
        {
            *skipSidebarDisplaySaveForRollback = false;
        }

        if (sidebarDisplayPreferencesErrorHandler)
        {
            sidebarDisplayPreferencesErrorHandler(saved.error());
        }
    };

    connect(
        showSidebarTooltips,
        &QAction::toggled,
        this,
        [saveSidebarDisplayPreferences, this](const bool)
        {
            saveSidebarDisplayPreferences(showSidebarTooltips);
        }
        );

    connect(
        animateSidebarText,
        &QAction::toggled,
        this,
        [saveSidebarDisplayPreferences, this](const bool)
        {
            saveSidebarDisplayPreferences(animateSidebarText);
        }
        );

    automaticallyCheckForUpdates =
        createCheckableAction(
            tr("Automatically Check for Updates"),
            tr("Check GitHub Releases for a newer version when ClassMngr starts")
            );
    if (!automaticUpdatePreferencesPort)
    {
        automaticUpdatePreferencesPort =
            std::make_shared<
                ClassMngr::Next::Platform::
                    SettingsManagerAutomaticUpdatePreferencesPort
                >();
    }

    const bool initiallyEnabled =
        automaticUpdatePreferencesPort->read().automaticChecksEnabled;
    automaticallyCheckForUpdates->setChecked(
        initiallyEnabled
        );
    const auto committedAutomaticUpdateChecksEnabled =
        std::make_shared<bool>(initiallyEnabled);
    connect(
        automaticallyCheckForUpdates,
        &QAction::toggled,
        this,
        [this,
            automaticUpdatePreferencesPort,
            automaticUpdatePreferenceErrorHandler,
            committedAutomaticUpdateChecksEnabled](const bool enabled)
        {
            const auto saved = automaticUpdatePreferencesPort->write({
                .automaticChecksEnabled = enabled
            });
            if (!saved)
            {
                {
                    const QSignalBlocker blocker(
                        automaticallyCheckForUpdates
                        );
                    automaticallyCheckForUpdates->setChecked(
                        *committedAutomaticUpdateChecksEnabled
                        );
                }

                if (automaticUpdatePreferenceErrorHandler)
                {
                    automaticUpdatePreferenceErrorHandler(saved.error());
                }
                return;
            }

            *committedAutomaticUpdateChecksEnabled = enabled;
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
