#pragma once

#include "next/application/gs_team_directory_save_policy.h"
#include "next/application/gs_team_directory_save_port.h"

namespace ClassMngr::Next::Application
{

class GsTeamDirectorySaveUseCase final
{
public:
    [[nodiscard]] static GsTeamDirectorySaveOutcome execute(
        const GsTeamDirectorySaveRequest& request,
        const GsTeamDirectorySavePort& port
        )
    {
        const GsTeamDirectorySaveValidation validation =
            validateGsTeamDirectorySave(request);
        if (!validation.isValid())
        {
            return GsTeamDirectorySaveOutcome::invalid(validation);
        }

        const Domain::Result<void> saved = port.saveGsTeamDirectory(request);
        if (!saved)
        {
            return GsTeamDirectorySaveOutcome::failure(saved.error());
        }

        return GsTeamDirectorySaveOutcome::success();
    }
};

} // namespace ClassMngr::Next::Application
