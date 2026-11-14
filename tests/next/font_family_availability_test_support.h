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
        std::vector<std::u16string> families = {},
        std::u16string outputFallbackFontFamily = {},
        int outputFallbackEnglishPointSize = 14,
        int applicationBaseFontPointSize = 14,
        int teacherProfileKoreanFontPointSize = 15
        )
        : m_families(std::move(families))
        , m_outputFallbackFontFamily(std::move(outputFallbackFontFamily))
        , m_outputFallbackEnglishPointSize(outputFallbackEnglishPointSize)
        , m_applicationBaseFontPointSize(applicationBaseFontPointSize)
        , m_teacherProfileKoreanFontPointSize(teacherProfileKoreanFontPointSize)
    {
    }

    [[nodiscard]] std::vector<std::u16string>
    availableFontFamilies() const override
    {
        return m_families;
    }

    [[nodiscard]] std::u16string outputFallbackFontFamily() const override
    {
        return m_outputFallbackFontFamily;
    }

    [[nodiscard]] int outputFallbackEnglishPointSize() const override
    {
        return m_outputFallbackEnglishPointSize;
    }

    [[nodiscard]] int applicationBaseFontPointSize() const override
    {
        return m_applicationBaseFontPointSize;
    }

    [[nodiscard]] int teacherProfileKoreanFontPointSize() const override
    {
        return m_teacherProfileKoreanFontPointSize;
    }

private:
    std::vector<std::u16string> m_families;
    std::u16string m_outputFallbackFontFamily;
    int m_outputFallbackEnglishPointSize = 14;
    int m_applicationBaseFontPointSize = 14;
    int m_teacherProfileKoreanFontPointSize = 15;
};

inline Application::FontFamilyAvailabilityPort& emptyFontFamilyAvailabilityPort()
{
    static FixedFontFamilyAvailabilityPort port;
    return port;
}

}  // namespace ClassMngr::Next::Test
