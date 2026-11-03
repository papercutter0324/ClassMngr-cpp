#pragma once

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// Reports the platform's currently available font-family names in platform
// order. UTF-16 preserves the Unicode family names exposed by Qt without
// making the Application boundary depend on Qt types.
class FontFamilyAvailabilityPort
{
public:
    virtual ~FontFamilyAvailabilityPort() = default;

    [[nodiscard]] virtual std::vector<std::u16string>
    availableFontFamilies() const = 0;
};

}  // namespace ClassMngr::Next::Application
