#pragma once

#include "next/application/campus_dashboard_selected_campus_read_port.h"
#include "next/application/qt_compatible_text.h"
#include "next/domain/domain_types.h"

#include <string>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kCampusDashboardCampusIdMaxLength = 256;

struct CampusDashboardSelectedCampusReadRequest final
{
    Domain::CampusId campusId;
};

namespace CampusDashboardSelectedCampusReadDetail
{

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

        const std::string& campusId = request.campusId.value();
        if (
            isBlankUtf8Text(campusId)
            || campusId.size() > kCampusDashboardCampusIdMaxLength
            )
        {
            return CampusDashboardSelectedCampusReadResult::failure(
                invalidCampusId()
                );
        }

        CampusDashboardSelectedCampusReadResult result =
            port.loadCampus(request.campusId);
        if (!result)
        {
            return result;
        }

        if (
            result.value().has_value()
            && result.value()->id != request.campusId
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
