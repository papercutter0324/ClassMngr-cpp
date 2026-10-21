#include "next/application/initial_setup_personal_name_requirement.h"

#include <cstdlib>

int main()
{
    using ClassMngr::Next::Application::
        initialSetupPersonalNameRequirementMet;

    if (!initialSetupPersonalNameRequirementMet(true)
        || initialSetupPersonalNameRequirementMet(false))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
