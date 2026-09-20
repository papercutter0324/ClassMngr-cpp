#pragma once

namespace ClassMngr::Next::Application
{

// The application boundary carries the font-size preference as its persisted
// offset. Persistence and legacy value conversion belong to the adapter.
enum class FontSize
{
    Small = -2,
    Normal = 0,
    Large = 2,
    ExtraLarge = 4
};

class FontSizePreferencesPort
{
public:
    virtual ~FontSizePreferencesPort() = default;

    [[nodiscard]] virtual FontSize read() const = 0;
};

} // namespace ClassMngr::Next::Application
