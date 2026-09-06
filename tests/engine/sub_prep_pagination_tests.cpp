#include "classmngr/engine/sub_prep_pagination.h"

#include <iostream>
#include <string_view>

namespace
{
using namespace classmngr::engine;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSubPrepPaginationTests: "
              << message
              << '\n';
    return false;
}
} // namespace

int main()
{
    bool passed = true;

    const std::vector<int> spanning =
        SubPrepPaginationService::teacherSectionsThatSpanPages(
            {
                {90.0, 20.0},
                {10.0, 20.0},
                {180.0, 100.0}
            },
            100.0
            );
    passed &= expect(
        spanning == std::vector<int>{0},
        "teacher section page-span detection changed"
        );

    passed &= expect(
        SubPrepPaginationService::shouldStartSubNotesOnNewPage(80.0, 100.0)
            && !SubPrepPaginationService::shouldStartSubNotesOnNewPage(
                60.0,
                100.0
                )
            && !SubPrepPaginationService::shouldStartSubNotesOnNewPage(
                80.0,
                0.0
                ),
        "sub-notes page threshold changed"
        );

    const auto lastPagePlacement =
        SubPrepPaginationService::fallbackSubNotesTop(
            140.0,
            2,
            100.0,
            21.0
            );
    const auto crowdedPlacement =
        SubPrepPaginationService::fallbackSubNotesTop(
            160.0,
            2,
            100.0,
            21.0
            );
    passed &= expect(
        lastPagePlacement.has_value()
            && *lastPagePlacement == 61.0
            && !crowdedPlacement.has_value(),
        "fallback sub-notes placement changed"
        );

    return passed ? 0 : 1;
}
