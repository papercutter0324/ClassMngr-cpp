#pragma once

#include "next/application/campus_dashboard_campus_identity_save_policy.h"
#include "next/application/campus_edit_capability.h"

namespace ClassMngr::Next::Application
{

class CampusDashboardCampusSaveUseCase final
{
public:
    [[nodiscard]] static Domain::Result<void> execute(
        const CampusEditCapability capability,
        const CampusDashboardCampusSnapshot& campus,
        const CampusDashboardCampusSaveIntent intent,
        const CampusDashboardCampusSavePort& port,
        const CampusDashboardCampusNameOrigin nameOrigin =
            CampusDashboardCampusNameOrigin::UserEntered
        )
    {
        if (!capability.canEdit())
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Campus editing is disabled for this session.",
                .recoverable = false
            });
        }

        const Domain::Result<void> identityValidation =
            CampusDashboardCampusIdentitySavePolicy::validate(
                campus,
                intent,
                nameOrigin
                );
        if (!identityValidation)
        {
            return identityValidation;
        }

        return port.saveCampus(campus, intent, nameOrigin);
    }
};

} // namespace ClassMngr::Next::Application
