#pragma once

#include "next/application/campus_dashboard_selected_campus_read_port.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kCampusDashboardCampusIdMaxLength = 256;

struct CampusDashboardSelectedCampusReadRequest final
{
    // Kept as UTF-8 input so malformed blank/oversized identifiers can be
    // rejected before constructing the typed ID or invoking the port.
    std::string campusId;
};

namespace CampusDashboardSelectedCampusReadDetail
{

[[nodiscard]] inline bool isBlank(
    const std::string_view value
    ) noexcept
{
    return value.empty()
        || std::all_of(
            value.cbegin(),
            value.cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            );
}

[[nodiscard]] inline Domain::OperationError invalidCampusId()
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::InvalidInput,
        .message = "Campus identifier must be non-blank and bounded.",
        .recoverable = false
    };
}

[[nodiscard]] inline Domain::OperationError mismatchedCampusId()
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "Campus detail result did not match the requested campus.",
        .recoverable = false
    };
}

} // namespace CampusDashboardSelectedCampusReadDetail

class CampusDashboardSelectedCampusReadQuery final
{
public:
    [[nodiscard]] static CampusDashboardSelectedCampusReadResult execute(
        const CampusDashboardSelectedCampusReadRequest& request,
        const CampusDashboardSelectedCampusReadPort& port
        )
    {
        using namespace CampusDashboardSelectedCampusReadDetail;

        if (
            isBlank(request.campusId)
            || request.campusId.size() > kCampusDashboardCampusIdMaxLength
            )
        {
            return CampusDashboardSelectedCampusReadResult::failure(
                invalidCampusId()
                );
        }

        const auto campusId = Domain::CampusId::fromString(request.campusId);
        if (!campusId.has_value())
        {
            return CampusDashboardSelectedCampusReadResult::failure(
                invalidCampusId()
                );
        }

        CampusDashboardSelectedCampusReadResult result =
            port.loadCampus(campusId.value());
        if (!result)
        {
            return result;
        }

        if (
            result.value().has_value()
            && result.value()->id != campusId.value()
            )
        {
            return CampusDashboardSelectedCampusReadResult::failure(
                mismatchedCampusId()
                );
        }

        return result;
    }
};

} // namespace ClassMngr::Next::Application
