#pragma once

#include "next/application/font_family_availability_port.h"

namespace ClassMngr::Next::Platform
{

class QtFontFamilyAvailabilityAdapter final
    : public Application::FontFamilyAvailabilityPort
{
public:
    [[nodiscard]] std::vector<std::u16string> availableFontFamilies() const override;
};

}  // namespace ClassMngr::Next::Platform
