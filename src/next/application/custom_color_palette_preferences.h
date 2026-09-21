#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries the fixed, canonical 16-entry custom
// color palette. QColor and legacy persistence formats belong to the outer
// platform adapter and the UI layer respectively.
struct CustomColorPalette final
{
    static constexpr std::size_t EntryCount = 16;

    std::array<std::string, EntryCount> hexColors;

    friend bool operator==(
        const CustomColorPalette&,
        const CustomColorPalette&
        ) = default;
};

[[nodiscard]] inline CustomColorPalette defaultCustomColorPalette()
{
    return {
        .hexColors = {
            "#F94144",
            "#F3722C",
            "#F8961E",
            "#F9C74F",
            "#90BE6D",
            "#43AA8B",
            "#577590",
            "#277DA1",
            "#9B5DE5",
            "#F15BB5",
            "#00BBF9",
            "#00F5D4",
            "#FFFFFF",
            "#D9D9D9",
            "#808080",
            "#000000"
        }
    };
}

class CustomColorPalettePreferencesPort
{
public:
    virtual ~CustomColorPalettePreferencesPort() = default;

    [[nodiscard]] virtual CustomColorPalette read() const = 0;

    virtual void write(const CustomColorPalette& palette) const = 0;
};

} // namespace ClassMngr::Next::Application
