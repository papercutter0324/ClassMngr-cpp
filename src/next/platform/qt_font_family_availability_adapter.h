#pragma once

#include "next/application/font_family_availability_port.h"

namespace ClassMngr::Next::Platform
{

class QtFontFamilyAvailabilityAdapter final
    : public Application::FontFamilyAvailabilityPort
{
public:
    [[nodiscard]] std::vector<std::u16string> availableFontFamilies() const override;
    [[nodiscard]] std::u16string outputFallbackFontFamily() const override;
    [[nodiscard]] int outputFallbackEnglishPointSize() const override;
    [[nodiscard]] int applicationBaseFontPointSize() const override;
    [[nodiscard]] int teacherProfileKoreanFontPointSize() const override;
};

}  // namespace ClassMngr::Next::Platform
