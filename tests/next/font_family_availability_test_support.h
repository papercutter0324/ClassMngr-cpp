#pragma once

#include "next/application/font_family_availability_port.h"

#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Test
{

class FixedFontFamilyAvailabilityPort final
    : public Application::FontFamilyAvailabilityPort
{
public:
    explicit FixedFontFamilyAvailabilityPort(
        std::vector<std::u16string> families = {}
        )
        : m_families(std::move(families))
    {
    }

    [[nodiscard]] std::vector<std::u16string>
    availableFontFamilies() const override
    {
        return m_families;
    }

private:
    std::vector<std::u16string> m_families;
};

inline Application::FontFamilyAvailabilityPort& emptyFontFamilyAvailabilityPort()
{
    static FixedFontFamilyAvailabilityPort port;
    return port;
}

}  // namespace ClassMngr::Next::Test
