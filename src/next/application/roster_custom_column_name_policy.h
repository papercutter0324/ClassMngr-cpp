#pragma once

#include "next/application/qt_compatible_text.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class RosterCustomColumnNameRejection
{
    Empty,
    Duplicate,
    RequiredColumn
};

struct RosterCustomColumnNameAdmission final
{
    std::u16string normalizedName;
    std::optional<RosterCustomColumnNameRejection> rejection;

    [[nodiscard]] bool accepted() const noexcept
    {
        return !rejection.has_value();
    }
};

// QString::simplified() trims leading/trailing Unicode whitespace and
// compresses each internal run to one ASCII space. Case-insensitive equality
// is supplied by the adapter so it can preserve its platform's exact Unicode
// comparison rules without introducing Qt into Application.
[[nodiscard]] inline std::u16string simplifyQtWhitespace(
    const std::u16string_view value
    )
{
    std::u16string result;
    result.reserve(value.size());

    bool hasContent = false;
    bool pendingSpace = false;
    for (const char16_t character : value)
    {
        if (isQtWhitespace(character))
        {
            pendingSpace = hasContent;
            continue;
        }

        if (pendingSpace)
        {
            result.push_back(u' ');
        }
        result.push_back(character);
        hasContent = true;
        pendingSpace = false;
    }
    return result;
}

template <typename CaseInsensitiveEquals>
[[nodiscard]] inline std::u16string normalizeRosterCustomColumnName(
    const std::u16string_view name,
    CaseInsensitiveEquals&& caseInsensitiveEquals
    )
{
    std::u16string normalized = simplifyQtWhitespace(name);
    if (std::invoke(
            caseInsensitiveEquals,
            std::u16string_view(normalized),
            std::u16string_view(u"Autumn")
            ))
    {
        normalized = u"Fall";
    }
    return normalized;
}

// Admission keeps the existing order: empty name, duplicate name, then
// required-column collision. The caller supplies Qt-compatible equality.
template <typename CaseInsensitiveEquals>
[[nodiscard]] inline RosterCustomColumnNameAdmission
admitRosterCustomColumnName(
    const std::u16string_view name,
    const std::vector<std::u16string>& existingColumnNames,
    const std::vector<std::u16string>& requiredColumnNames,
    CaseInsensitiveEquals&& caseInsensitiveEquals
    )
{
    std::u16string normalized = normalizeRosterCustomColumnName(
        name,
        caseInsensitiveEquals
        );
    if (normalized.empty())
    {
        return {
            .normalizedName = std::move(normalized),
            .rejection = RosterCustomColumnNameRejection::Empty
        };
    }

    for (const std::u16string& existingName : existingColumnNames)
    {
        const std::u16string normalizedExisting =
            normalizeRosterCustomColumnName(
                existingName,
                caseInsensitiveEquals
                );
        if (std::invoke(
                caseInsensitiveEquals,
                std::u16string_view(normalized),
                std::u16string_view(normalizedExisting)
                ))
        {
            return {
                .normalizedName = std::move(normalized),
                .rejection = RosterCustomColumnNameRejection::Duplicate
            };
        }
    }

    for (const std::u16string& requiredName : requiredColumnNames)
    {
        const std::u16string normalizedRequired =
            normalizeRosterCustomColumnName(
                requiredName,
                caseInsensitiveEquals
                );
        if (std::invoke(
                caseInsensitiveEquals,
                std::u16string_view(normalized),
                std::u16string_view(normalizedRequired)
                ))
        {
            return {
                .normalizedName = std::move(normalized),
                .rejection = RosterCustomColumnNameRejection::RequiredColumn
            };
        }
    }

    return {
        .normalizedName = std::move(normalized),
        .rejection = std::nullopt
    };
}

} // namespace ClassMngr::Next::Application
