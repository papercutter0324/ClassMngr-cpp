#pragma once

#include "next/application/font_presentation_port.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// Reports the platform's currently available font-family names in platform
// order. UTF-16 preserves the Unicode family names exposed by Qt without
// making the Application boundary depend on Qt types.
class FontFamilyAvailabilityPort : public FontPresentationPort
{
public:
    virtual ~FontFamilyAvailabilityPort() = default;

    [[nodiscard]] virtual std::vector<std::u16string>
    availableFontFamilies() const = 0;

    // Supplies the host-composed final family for generated output. The
    // platform adapter owns any platform-specific choice and keeps Qt types
    // outside the Application boundary.
    [[nodiscard]] virtual std::u16string outputFallbackFontFamily() const = 0;

    // Supplies Schedule's host-specific English fallback size when the
    // inherited application font does not have a usable point size.
    [[nodiscard]] virtual int outputFallbackEnglishPointSize() const = 0;
};

}  // namespace ClassMngr::Next::Application
