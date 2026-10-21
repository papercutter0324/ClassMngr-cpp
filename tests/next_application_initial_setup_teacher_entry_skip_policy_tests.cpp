#include "next/application/initial_setup_teacher_entry_skip_policy.h"

#include <cstdlib>

int main()
{
    using ClassMngr::Next::Application::
        initialSetupTeacherEntryShouldBeSkipped;

    if (initialSetupTeacherEntryShouldBeSkipped(false, false)
        || initialSetupTeacherEntryShouldBeSkipped(false, true)
        || initialSetupTeacherEntryShouldBeSkipped(true, false)
        || !initialSetupTeacherEntryShouldBeSkipped(true, true))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
