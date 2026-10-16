#include "next/application/initial_setup_teacher_identity_validation.h"

#include <cstdlib>

using ClassMngr::Next::Application::
    initialSetupTeacherIdentityRequirements;

int main()
{
    for (unsigned int combination = 0; combination < 8; ++combination)
    {
        const bool englishPresent = (combination & 1U) != 0;
        const bool koreanPresent = (combination & 2U) != 0;
        const bool preferredSpellingPresent = (combination & 4U) != 0;
        const unsigned int fieldsPresent =
            static_cast<unsigned int>(englishPresent)
            + static_cast<unsigned int>(koreanPresent)
            + static_cast<unsigned int>(preferredSpellingPresent);
        const bool complete = fieldsPresent >= 2;

        const auto requirements = initialSetupTeacherIdentityRequirements(
            englishPresent,
            koreanPresent,
            preferredSpellingPresent
            );
        if (requirements.complete != complete
            || requirements.additionalFieldsRequired
                != (complete ? 0U : 2U - fieldsPresent)
            || requirements.englishNameMissing == englishPresent
            || requirements.koreanNameMissing == koreanPresent
            || requirements.preferredSpellingMissing
                == preferredSpellingPresent)
        {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
