#pragma once

#include "next/application/font_family_availability_port.h"
#include "next/application/font_size_preferences.h"
#include "next/ui/font_family_selection_policy.h"

#include <QFont>
#include <QStringList>

#include <algorithm>
#include <optional>

namespace ClassMngr::Next::Ui
{

class NavigationFontPolicy final
{
public:
    static constexpr int StandardPointSize = 14;
    static constexpr int MinimumRowHeightPixels = 30;

    // Preserve the scoped application font family and fallback chain while
    // matching the legacy tree's 14 pt base and its font-size preferences.
    [[nodiscard]] static QFont standardFont(
        QFont inheritedFont,
        const Application::FontSize preference,
        const Application::FontFamilyAvailabilityPort& fontFamilies
        )
    {
        int offset = 0;
        switch (preference)
        {
        case Application::FontSize::Small:
            offset = -2;
            break;
        case Application::FontSize::Normal:
            break;
        case Application::FontSize::Large:
            offset = 2;
            break;
        case Application::FontSize::ExtraLarge:
            offset = 4;
            break;
        }
        preferRegisteredInter(inheritedFont, fontFamilies);
        inheritedFont.setPointSize(StandardPointSize + offset);
        inheritedFont.setWeight(QFont::Normal);
        inheritedFont.setItalic(false);
        return inheritedFont;
    }

private:
    static void preferRegisteredInter(
        QFont& font,
        const Application::FontFamilyAvailabilityPort& fontFamilies
        )
    {
        const std::optional<QString> interFamily =
            FontFamilySelectionPolicy::firstExactMatch(
                fontFamilies, QStringLiteral("Inter")
                );
        if (!interFamily)
        {
            return;
        }

        QStringList families{*interFamily};
        for (const QString& inheritedFamily : font.families())
        {
            const bool alreadyPresent = std::any_of(
                families.cbegin(),
                families.cend(),
                [&inheritedFamily](const QString& family) {
                    return family.compare(inheritedFamily,
                                          Qt::CaseInsensitive) == 0;
                }
                );
            if (!inheritedFamily.isEmpty() && !alreadyPresent)
            {
                families.push_back(inheritedFamily);
            }
        }
        font.setFamilies(families);
    }
};

}  // namespace ClassMngr::Next::Ui
