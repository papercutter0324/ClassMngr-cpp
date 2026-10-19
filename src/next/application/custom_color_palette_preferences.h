#pragma once

#include "next/domain/operation_result.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// An exact, owned UTF-8 palette entry. Validation and canonicalization belong
// to the platform and UI boundaries; this value preserves its supplied text.
class CustomColorPaletteEntry final
{
public:
    CustomColorPaletteEntry() = default;

    explicit CustomColorPaletteEntry(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const CustomColorPaletteEntry&,
        const CustomColorPaletteEntry&
        ) = default;

private:
    std::string m_value;
};

// The application boundary carries the fixed 16-entry custom color palette.
// QColor and legacy persistence formats belong to the outer platform adapter
// and the UI layer respectively.
struct CustomColorPalette final
{
    static constexpr std::size_t EntryCount = 16;

    std::array<CustomColorPaletteEntry, EntryCount> hexColors;

    friend bool operator==(
        const CustomColorPalette&,
        const CustomColorPalette&
        ) = default;
};

[[nodiscard]] inline CustomColorPalette defaultCustomColorPalette()
{
    return {
        .hexColors = {
            CustomColorPaletteEntry("#F94144"),
            CustomColorPaletteEntry("#F3722C"),
            CustomColorPaletteEntry("#F8961E"),
            CustomColorPaletteEntry("#F9C74F"),
            CustomColorPaletteEntry("#90BE6D"),
            CustomColorPaletteEntry("#43AA8B"),
            CustomColorPaletteEntry("#577590"),
            CustomColorPaletteEntry("#277DA1"),
            CustomColorPaletteEntry("#9B5DE5"),
            CustomColorPaletteEntry("#F15BB5"),
            CustomColorPaletteEntry("#00BBF9"),
            CustomColorPaletteEntry("#00F5D4"),
            CustomColorPaletteEntry("#FFFFFF"),
            CustomColorPaletteEntry("#D9D9D9"),
            CustomColorPaletteEntry("#808080"),
            CustomColorPaletteEntry("#000000")
        }
    };
}

class CustomColorPalettePreferencesPort
{
public:
    virtual ~CustomColorPalettePreferencesPort() = default;

    [[nodiscard]] virtual Domain::Result<CustomColorPalette> read()
        const = 0;

    [[nodiscard]] virtual Domain::Result<void> write(
        const CustomColorPalette& palette
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
