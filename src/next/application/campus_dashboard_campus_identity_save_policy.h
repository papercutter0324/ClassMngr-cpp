#pragma once

#include "next/application/campus_dashboard_campus_save_port.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Application
{

namespace CampusDashboardCampusIdentitySavePolicyDetail
{

[[nodiscard]] constexpr bool asciiCaseInsensitiveEqual(
    const std::string_view left,
    const std::string_view right
    ) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        char leftCharacter = left[index];
        char rightCharacter = right[index];
        if (leftCharacter >= 'A' && leftCharacter <= 'Z')
        {
            leftCharacter = static_cast<char>(
                leftCharacter + ('a' - 'A')
                );
        }
        if (rightCharacter >= 'A' && rightCharacter <= 'Z')
        {
            rightCharacter = static_cast<char>(
                rightCharacter + ('a' - 'A')
                );
        }

        if (leftCharacter != rightCharacter)
        {
            return false;
        }
    }

    return true;
}

} // namespace CampusDashboardCampusIdentitySavePolicyDetail

// The JSON repository reserves this identity for its fallback campus and
// filters matching IDs and display names from normal list/read results.
class CampusDashboardCampusIdentitySavePolicy final
{
public:
    [[nodiscard]] static Domain::Result<void> validate(
        const CampusDashboardCampusSnapshot& campus,
        const CampusDashboardCampusSaveIntent intent,
        const CampusDashboardCampusNameOrigin nameOrigin =
            CampusDashboardCampusNameOrigin::UserEntered
        )
    {
        const bool reservedId =
            CampusDashboardCampusIdentitySavePolicyDetail::
                asciiCaseInsensitiveEqual(campus.id.value(), "default");
        const bool reservedName =
            nameOrigin
                == CampusDashboardCampusNameOrigin::LocalizedDefaultFallback
            || CampusDashboardCampusIdentitySavePolicyDetail::
                asciiCaseInsensitiveEqual(campus.campusName, "Default");
        if (!reservedId && !reservedName)
        {
            return Domain::Result<void>::success();
        }

        const char* operation =
            intent == CampusDashboardCampusSaveIntent::Create
                ? "create"
                : "update";
        std::string message = "Cannot ";
        message += operation;
        message += " a campus with the reserved Default identity. ";
        if (reservedId && reservedName)
        {
            message +=
                "Choose a different campus name so its ID is not 'default'.";
        }
        else if (reservedId)
        {
            message += "The campus ID 'default' is reserved.";
        }
        else
        {
            message += "Choose a different campus name.";
        }

        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = std::move(message),
            .recoverable = false
        });
    }
};

} // namespace ClassMngr::Next::Application
