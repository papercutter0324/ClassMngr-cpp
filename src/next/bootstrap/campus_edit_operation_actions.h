#pragma once

#include "next/application/campus_dashboard_campus_save_use_case.h"
#include "next/application/campus_legacy_import_use_case.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

namespace ClassMngr::Next::Bootstrap
{

using CampusSaveAction = std::function<Domain::Result<void>(
    const Application::CampusDashboardCampusSnapshot&,
    Application::CampusDashboardCampusSaveIntent)>;
using CampusLegacyImportAction =
    std::function<Domain::Result<std::size_t>()>;

[[nodiscard]] inline CampusSaveAction campusSaveAction(
    const Application::CampusEditCapability capability,
    std::shared_ptr<const Application::CampusDashboardCampusSavePort> port
    )
{
    return [capability, port = std::move(port)](
               const Application::CampusDashboardCampusSnapshot& campus,
               const Application::CampusDashboardCampusSaveIntent intent
               ) {
        if (!port)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Campus saving is unavailable.",
                .recoverable = false
            });
        }
        return Application::CampusDashboardCampusSaveUseCase::execute(
            capability,
            campus,
            intent,
            *port
            );
    };
}

[[nodiscard]] inline CampusLegacyImportAction campusLegacyImportAction(
    const Application::CampusEditCapability capability,
    std::shared_ptr<const Application::CampusLegacyImportPort> port
    )
{
    return [capability, port = std::move(port)] {
        if (!port)
        {
            return Domain::Result<std::size_t>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Campus import is unavailable.",
                .recoverable = false
            });
        }
        return Application::CampusLegacyImportUseCase::execute(
            capability,
            *port
            );
    };
}

} // namespace ClassMngr::Next::Bootstrap
