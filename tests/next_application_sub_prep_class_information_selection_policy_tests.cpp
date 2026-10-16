#include "next/application/sub_prep_class_information_selection_policy.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

ClassId classId(const std::string& value)
{
    return *ClassId::fromString(value);
}

bool retainsSelectedClassWhenItIsNotFirstVisible()
{
    const ClassId selected = classId("class-2");
    const auto resolved = selectSubPrepClassInformationId(
        selected,
        {classId("class-1"), selected, classId("class-3")}
        );

    return resolved.has_value() && *resolved == selected;
}

bool missingSelectionFallsBackToTheFirstVisibleClass()
{
    const std::vector<ClassId> visibleClassIds{
        classId("class-2"),
        classId("class-1")
    };
    const ClassId firstVisible = visibleClassIds.front();

    const auto missingSelection = selectSubPrepClassInformationId(
        classId("class-3"),
        visibleClassIds
        );
    const auto absentSelection = selectSubPrepClassInformationId(
        std::nullopt,
        visibleClassIds
        );

    return missingSelection == firstVisible
        && absentSelection == firstVisible;
}

bool emptyVisibleListReturnsNoSelection()
{
    const auto selected = selectSubPrepClassInformationId(
        classId("class-1"),
        {}
        );
    return !selected.has_value();
}

} // namespace

int main()
{
    if (!retainsSelectedClassWhenItIsNotFirstVisible())
    {
        std::cerr
            << "Sub Prep class selection should retain any visible current class.\n";
        return EXIT_FAILURE;
    }
    if (!missingSelectionFallsBackToTheFirstVisibleClass())
    {
        std::cerr
            << "Sub Prep class selection should use the first visible row when missing.\n";
        return EXIT_FAILURE;
    }
    if (!emptyVisibleListReturnsNoSelection())
    {
        std::cerr
            << "Sub Prep class selection should be empty when there are no visible rows.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
