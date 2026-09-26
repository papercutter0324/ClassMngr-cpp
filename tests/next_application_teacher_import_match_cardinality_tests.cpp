#include "next/application/teacher_import_match_cardinality.h"

#include <cstdlib>

using ClassMngr::Next::Application::TeacherImportMatchCardinality;
using ClassMngr::Next::Application::classifyTeacherImportMatchCardinality;

int main()
{
    if (classifyTeacherImportMatchCardinality(0)
        != TeacherImportMatchCardinality::NoMatch)
    {
        return EXIT_FAILURE;
    }
    if (classifyTeacherImportMatchCardinality(1)
        != TeacherImportMatchCardinality::UniqueMatch)
    {
        return EXIT_FAILURE;
    }
    if (classifyTeacherImportMatchCardinality(2)
        != TeacherImportMatchCardinality::MultipleMatches)
    {
        return EXIT_FAILURE;
    }
    if (classifyTeacherImportMatchCardinality(9)
        != TeacherImportMatchCardinality::MultipleMatches)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
