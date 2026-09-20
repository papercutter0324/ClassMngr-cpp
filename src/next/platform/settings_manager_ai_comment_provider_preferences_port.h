#pragma once

#include "core/settingsmanager.h"
#include "next/application/ai_comment_provider_preferences.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the AI-comment provider read. The legacy
// SettingsManager singleton and integer conversion stay here; ActionRegistry
// remains the compatibility writer and UI owner for this option.
class SettingsManagerAiCommentProviderPreferencesPort final
    : public Application::AiCommentProviderPreferencesPort
{
public:
    SettingsManagerAiCommentProviderPreferencesPort() = default;

    [[nodiscard]] Application::AiCommentProvider read() const override
    {
        const int storedValue =
            SettingsManager::instance().get(
                key(),
                static_cast<int>(
                    Application::AiCommentProvider::ChatGPT
                    )
                ).toInt();

        switch (storedValue)
        {
        case static_cast<int>(Application::AiCommentProvider::ChatGPT):
            return Application::AiCommentProvider::ChatGPT;
        case static_cast<int>(Application::AiCommentProvider::Gemini):
            return Application::AiCommentProvider::Gemini;
        case static_cast<int>(Application::AiCommentProvider::Claude):
            return Application::AiCommentProvider::Claude;
        case static_cast<int>(
            Application::AiCommentProvider::MicrosoftCopilot
            ):
            return Application::AiCommentProvider::MicrosoftCopilot;
        case static_cast<int>(
            Application::AiCommentProvider::CustomWebsite
            ):
            return Application::AiCommentProvider::CustomWebsite;
        default:
            return Application::AiCommentProvider::ChatGPT;
        }
    }

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::AiCommentProvider
            );
    }
};

} // namespace ClassMngr::Next::Platform
