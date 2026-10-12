#pragma once

#include "core/settingsmanager.h"
#include "next/application/ai_comment_provider_preferences.h"

#include <QString>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the AI-comment provider preference. The legacy
// SettingsManager singleton, checked write, and integer conversion stay here.
class SettingsManagerAiCommentProviderPreferencesPort final
    : public Application::AiCommentProviderPreferencesPort
{
public:
    explicit SettingsManagerAiCommentProviderPreferencesPort(
        SettingsManager::AiCommentProviderPreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

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

    [[nodiscard]] Domain::Result<void> write(
        const Application::AiCommentProvider provider
        ) const override
    {
        int storedValue = 0;
        switch (provider)
        {
        case Application::AiCommentProvider::ChatGPT:
            storedValue = 0;
            break;

        case Application::AiCommentProvider::Gemini:
            storedValue = 1;
            break;

        case Application::AiCommentProvider::Claude:
            storedValue = 2;
            break;

        case Application::AiCommentProvider::MicrosoftCopilot:
            storedValue = 3;
            break;

        case Application::AiCommentProvider::CustomWebsite:
            storedValue = 4;
            break;

        default:
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message =
                    "AI Comment Provider must be ChatGPT, Gemini, Claude, "
                    "Microsoft Copilot, or Custom Website.",
                .recoverable = false
            });
        }

        const QSettings::Status status =
            SettingsManager::instance().writeAiCommentProviderPreference(
                storedValue,
                m_synchronize
                );
        if (status != QSettings::NoError)
        {
            const char* statusName = status == QSettings::AccessError
                ? "AccessError (settings could not be written)"
                : "FormatError (settings contain invalid data)";
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = std::string(
                    "Unable to save AI Comment Provider preference: "
                    "QSettings "
                    ) + statusName + ".",
                .recoverable = true
            });
        }

        return Domain::Result<void>::success();
    }

private:
    SettingsManager::AiCommentProviderPreferenceSync m_synchronize;

    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            SettingsManager::Keys::AI_COMMENT_PROVIDER_PREFERENCE
            );
    }
};

} // namespace ClassMngr::Next::Platform
