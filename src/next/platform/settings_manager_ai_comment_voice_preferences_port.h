#pragma once

#include "core/settingsmanager.h"
#include "next/application/ai_comment_voice_preferences.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the AI-comment voice preference. The legacy
// SettingsManager singleton, checked write, and integer conversion stay here.
class SettingsManagerAiCommentVoicePreferencesPort final
    : public Application::AiCommentVoicePreferencesPort
{
public:
    explicit SettingsManagerAiCommentVoicePreferencesPort(
        SettingsManager::AiCommentVoicePreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

    [[nodiscard]] Application::AiCommentVoice read() const override
    {
        const int storedValue =
            SettingsManager::instance().get(
                key(),
                static_cast<int>(
                    Application::AiCommentVoice::DirectToStudent
                    )
                ).toInt();

        return storedValue == static_cast<int>(
            Application::AiCommentVoice::ThirdPerson
            )
            ? Application::AiCommentVoice::ThirdPerson
            : Application::AiCommentVoice::DirectToStudent;
    }

    [[nodiscard]] Domain::Result<void> write(
        const Application::AiCommentVoice voice
        ) const override
    {
        int storedValue = 0;
        switch (voice)
        {
        case Application::AiCommentVoice::DirectToStudent:
            storedValue = 0;
            break;

        case Application::AiCommentVoice::ThirdPerson:
            storedValue = 1;
            break;

        default:
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message =
                    "AI Comment Voice must be Direct to Student or Third Person.",
                .recoverable = false
            });
        }

        const QSettings::Status status =
            SettingsManager::instance().writeAiCommentVoicePreference(
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
                    "Unable to save AI Comment Voice preference: QSettings "
                    ) + statusName + ".",
                .recoverable = true
            });
        }

        return Domain::Result<void>::success();
    }

private:
    SettingsManager::AiCommentVoicePreferenceSync m_synchronize;

    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::AiCommentVoice
            );
    }
};

} // namespace ClassMngr::Next::Platform
