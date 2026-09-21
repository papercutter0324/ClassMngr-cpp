#pragma once

namespace ClassMngr::Next::Application
{

// The application boundary carries only the typed AI-comment provider.
// Persistence and legacy integer conversion belong to the outer adapter.
enum class AiCommentProvider
{
    ChatGPT = 0,
    Gemini = 1,
    Claude = 2,
    MicrosoftCopilot = 3,
    CustomWebsite = 4
};

class AiCommentProviderPreferencesPort
{
public:
    virtual ~AiCommentProviderPreferencesPort() = default;

    [[nodiscard]] virtual AiCommentProvider read() const = 0;

    virtual void write(
        AiCommentProvider provider
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
