#pragma once

namespace ClassMngr::Next::Application
{

// The application boundary carries the typed document-page spacing.
// Persistence and legacy integer conversion belong to the platform adapter.
enum class DocumentPageSpacing
{
    None = 0,
    Small = 1,
    Medium = 2,
    Large = 3
};

class DocumentPageSpacingPreferencesPort
{
public:
    virtual ~DocumentPageSpacingPreferencesPort() = default;

    [[nodiscard]] virtual DocumentPageSpacing read() const = 0;
};

} // namespace ClassMngr::Next::Application
