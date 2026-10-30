#pragma once

#include "next/application/ai_comment_custom_website_port.h"
#include "next/application/ai_comment_provider_preferences.h"
#include "next/application/ai_comment_voice_preferences.h"
#include "next/application/document_page_spacing_preferences.h"
#include "next/application/document_viewer_background_preferences.h"
#include "next/application/preferences_model.h"
#include "next/application/save_mode_preferences.h"

#include <QDialog>

#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QTabWidget;

namespace ClassMngr::Next::Application
{
class ThemePreferencesPort;
}

namespace ClassMngr::Next::Ui
{

struct PreferencesDialogPorts final
{
    Application::SaveModePreferencesPort& saveMode;
    Application::DocumentPageSpacingPreferencesPort& documentSpacing;
    Application::DocumentViewerBackgroundPreferencesPort& documentBackground;
    Application::AiCommentProviderPreferencesPort& aiProvider;
    Application::AiCommentVoicePreferencesPort& aiVoice;
    Application::AiCommentCustomWebsitePort& customWebsite;
};

struct PreferencesDialogActions final
{
    std::function<Domain::Result<void>(Application::ThemePreference)> applyTheme;
    std::function<Domain::Result<void>(Application::LanguagePreference)> applyLanguage;
    std::function<Domain::Result<void>(Application::FontSize)> applyFontSize;
    std::function<void(Application::SaveMode)> saveModeChanged;
};

// Preferences are applied and saved as the user changes each control, matching
// the Legacy dialog. The supplied ports and model are borrowed for this dialog.
class PreferencesDialog final : public QDialog
{
public:
    PreferencesDialog(
        Application::PreferencesModel& preferences,
        PreferencesDialogPorts ports,
        PreferencesDialogActions actions = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] bool applicationPreferencesLoaded() const noexcept;

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildUi();
    void populateValues();
    void retranslateUi();
    void showFailure(const char* fieldSource, const Domain::OperationError& error);
    void clearFailure();
    [[nodiscard]] Domain::Result<void> saveApplicationPreference(
        const char* fieldSource,
        const std::function<Domain::Result<void>(Application::UserPreferencesState&)>& update
        );
    void changeTheme(int index);
    void changeLanguage(int index);
    void changeFontSize(int index);
    void changeSaveMode(int index);
    void changeDocumentSpacing(int index);
    void changeDocumentBackground(int index);
    void changeAiProvider(int index);
    void changeAiVoice(int index);
    void updateCustomWebsiteText();
    void updateControlAvailability();

    Application::PreferencesModel& m_preferences;
    PreferencesDialogPorts m_ports;
    PreferencesDialogActions m_actions;
    bool m_applicationPreferencesLoaded = false;
    bool m_typedPreferencesLoaded = false;
    bool m_populating = false;
    QString m_failureFieldSource;
    QString m_failureDetail;
    Application::SaveMode m_savedSaveMode = Application::SaveMode::Automatic;
    Application::DocumentPageSpacing m_savedDocumentSpacing =
        Application::DocumentPageSpacing::Small;
    Application::DocumentViewerBackground m_savedDocumentBackground =
        Application::DocumentViewerBackground::Default;
    Application::AiCommentProvider m_savedAiProvider =
        Application::AiCommentProvider::ChatGPT;
    Application::AiCommentVoice m_savedAiVoice =
        Application::AiCommentVoice::DirectToStudent;
    std::string m_savedCustomWebsite;

    QLabel* m_error = nullptr;
    QTabWidget* m_tabs = nullptr;
    QComboBox* m_saveMode = nullptr;
    QCheckBox* m_autoUpdates = nullptr;
    QCheckBox* m_sidebarTooltips = nullptr;
    QCheckBox* m_sidebarAnimation = nullptr;
    QComboBox* m_excelTimeout = nullptr;
    QCheckBox* m_powerPointNotice = nullptr;
    QComboBox* m_theme = nullptr;
    QComboBox* m_language = nullptr;
    QComboBox* m_fontSize = nullptr;
    QComboBox* m_documentSpacing = nullptr;
    QComboBox* m_documentBackground = nullptr;
    QComboBox* m_aiProvider = nullptr;
    QComboBox* m_aiVoice = nullptr;
    QLabel* m_customWebsite = nullptr;
};

}  // namespace ClassMngr::Next::Ui
