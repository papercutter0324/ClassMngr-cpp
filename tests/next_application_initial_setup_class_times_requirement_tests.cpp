#include "next/application/initial_setup_class_times_requirement.h"

#include <cstdlib>

int main()
{
    using ClassMngr::Next::Application::
        initialSetupClassTimesRequirementMet;

    if (initialSetupClassTimesRequirementMet(false, false)
        || !initialSetupClassTimesRequirementMet(true, false)
        || !initialSetupClassTimesRequirementMet(false, true)
        || !initialSetupClassTimesRequirementMet(true, true))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
