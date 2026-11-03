#pragma once

#include "next/application/font_family_availability_port.h"

#include <QFont>
#include <QString>

#include <optional>

namespace ClassMngr::Next::Ui
{

// Font-family preference is presentation policy. Keep matching here while
// taking the platform's ordered family list through the Application port.
class FontFamilySelectionPolicy final
{
public:
    [[nodiscard]] static std::optional<QString> firstExactMatch(
        const Application::FontFamilyAvailabilityPort& fontFamilies,
        const QString& requestedFamily
        )
    {
        for (const std::u16string& availableFamily
             : fontFamilies.availableFontFamilies())
        {
            const QString family = QString::fromStdU16String(availableFamily);
            if (family.compare(requestedFamily, Qt::CaseInsensitive) == 0)
            {
                return family;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::optional<QString> firstContainingMatch(
        const Application::FontFamilyAvailabilityPort& fontFamilies,
        const QString& requestedFragment
        )
    {
        for (const std::u16string& availableFamily
             : fontFamilies.availableFontFamilies())
        {
            const QString family = QString::fromStdU16String(availableFamily);
            if (family.contains(requestedFragment, Qt::CaseInsensitive))
            {
                return family;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static QFont withFirstContainingMatch(
        QFont font,
        const Application::FontFamilyAvailabilityPort& fontFamilies,
        const QString& requestedFragment
        )
    {
        if (const std::optional<QString> family = firstContainingMatch(
                fontFamilies, requestedFragment
                ))
        {
            font.setFamily(*family);
        }
        return font;
    }
};

}  // namespace ClassMngr::Next::Ui
