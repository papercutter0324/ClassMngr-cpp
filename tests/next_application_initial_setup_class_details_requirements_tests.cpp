#include "next/application/initial_setup_class_details_requirements.h"

#include <cstdlib>
#include <type_traits>

using ClassMngr::Next::Application::
    InitialSetupClassDetailsRequirements;
using ClassMngr::Next::Application::initialSetupClassDetailsRequirements;

static_assert(!std::is_aggregate_v<InitialSetupClassDetailsRequirements>);
static_assert(
    !std::is_default_constructible_v<InitialSetupClassDetailsRequirements>
    );

int main()
{
    for (unsigned int combination = 0; combination < 16; ++combination)
    {
        const bool teacherPresent = (combination & 1U) != 0;
        const bool classColorChosen = (combination & 2U) != 0;
        const bool gradePresent = (combination & 4U) != 0;
        const bool levelPresent = (combination & 8U) != 0;
        const unsigned int requirementsMet =
            static_cast<unsigned int>(teacherPresent)
            + static_cast<unsigned int>(classColorChosen)
            + static_cast<unsigned int>(gradePresent)
            + static_cast<unsigned int>(levelPresent);
        const bool complete = requirementsMet == 4U;

        const auto requirements = initialSetupClassDetailsRequirements(
            teacherPresent,
            classColorChosen,
            gradePresent,
            levelPresent
            );
        if (requirements.complete() != complete
            || requirements.koreanTeacherMissing() == teacherPresent
            || requirements.classColorMissing() == classColorChosen
            || requirements.gradeMissing() == gradePresent
            || requirements.levelMissing() == levelPresent)
        {
            return EXIT_FAILURE;
        }

        const unsigned int missingRequirements =
            static_cast<unsigned int>(requirements.koreanTeacherMissing())
            + static_cast<unsigned int>(requirements.classColorMissing())
            + static_cast<unsigned int>(requirements.gradeMissing())
            + static_cast<unsigned int>(requirements.levelMissing());
        if (requirements.complete()
                != (missingRequirements == 0U)
            || requirementsMet + missingRequirements != 4U)
        {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
