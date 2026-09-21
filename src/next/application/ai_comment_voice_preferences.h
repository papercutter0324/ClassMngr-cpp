#pragma once

namespace ClassMngr::Next::Application
{

// The application boundary carries only the typed AI-comment voice.
// Persistence and legacy integer conversion belong to the outer adapter.
enum class AiCommentVoice
{
    DirectToStudent = 0,
    ThirdPerson = 1
};

class AiCommentVoicePreferencesPort
{
public:
    virtual ~AiCommentVoicePreferencesPort() = default;

    [[nodiscard]] virtual AiCommentVoice read() const = 0;

    virtual void write(
        AiCommentVoice voice
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
