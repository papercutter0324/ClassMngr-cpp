#pragma once

#include "core/settingsmanager.h"
#include "next/application/ai_comment_voice_preferences.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the AI-comment voice read. The legacy
// SettingsManager singleton and integer conversion stay here; ActionRegistry
// remains the compatibility writer for this option.
class SettingsManagerAiCommentVoicePreferencesPort final
    : public Application::AiCommentVoicePreferencesPort
{
public:
    SettingsManagerAiCommentVoicePreferencesPort() = default;

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

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::AiCommentVoice
            );
    }
};

} // namespace ClassMngr::Next::Platform
