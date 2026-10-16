#include "next/application/sub_prep_selected_class_ids.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

bool emptyInputsReturnNoClassIds()
{
    return projectSubPrepSelectedClassIds({}, {}).empty()
        && projectSubPrepSelectedClassIds(
            {u"Monday"},
            {}
            ).empty()
        && projectSubPrepSelectedClassIds(
            {},
            {{.cells = {{.day = u"Monday", .entries = {{42}}}}}}
            ).empty();
}

bool selectedDayMatchingIsCanonicalAndExact()
{
    const std::vector<SubPrepSelectedClassScheduleRow> rows{
        {
            .cells = {
                {.day = u"Monday", .entries = {{42}}}
            }
        }
    };

    return projectSubPrepSelectedClassIds({u"monday"}, rows).empty()
        && projectSubPrepSelectedClassIds({u" Monday"}, rows).empty()
        && projectSubPrepSelectedClassIds({u"Monday "}, rows).empty()
        && projectSubPrepSelectedClassIds({u"Monday"}, rows)
            == std::vector<int>{42};
}

bool positiveIdsAreDeduplicatedGloballyAtTheirFirstOccurrence()
{
    const std::vector<SubPrepSelectedClassScheduleRow> rows{
        {
            .cells = {
                {
                    .day = u"Monday",
                    .entries = {
                        {-3},
                        {0},
                        {42},
                        {17},
                        {42}
                    }
                },
                {
                    .day = u"Monday",
                    .entries = {
                        {17},
                        {99},
                        {0}
                    }
                }
            }
        },
        {
            .cells = {
                {
                    .day = u"Monday",
                    .entries = {{42}, {101}}
                }
            }
        }
    };

    return projectSubPrepSelectedClassIds({u"Monday"}, rows)
        == std::vector<int>{42, 17, 99, 101};
}

bool scheduleTraversalDefinesOutputOrderIndependentOfSelectionOrder()
{
    const std::vector<SubPrepSelectedClassScheduleRow> rows{
        {
            .cells = {
                {.day = u"Monday", .entries = {{10}, {11}}},
                {.day = u"Wednesday", .entries = {{20}}}
            }
        },
        {
            .cells = {
                {.day = u"Friday", .entries = {{30}}},
                {.day = u"Monday", .entries = {{12}}}
            }
        }
    };

    return projectSubPrepSelectedClassIds(
        {u"Wednesday", u"Monday"},
        rows
        ) == std::vector<int>{10, 11, 20, 12};
}

bool regularAndTestingEntriesAreBothIncluded()
{
    const std::vector<SubPrepSelectedClassScheduleRow> rows{
        {
            .cells = {
                {
                    .day = u"Tuesday",
                    .entries = {
                        {
                            .classId = 51,
                            .kind = SubPrepSelectedClassScheduleEntryKind::
                                RegularClass
                        },
                        {
                            .classId = 62,
                            .kind = SubPrepSelectedClassScheduleEntryKind::
                                TestingClass
                        }
                    }
                }
            }
        }
    };

    return projectSubPrepSelectedClassIds({u"Tuesday"}, rows)
        == std::vector<int>{51, 62};
}

} // namespace

int main()
{
    if (!emptyInputsReturnNoClassIds())
    {
        std::fprintf(stderr, "Empty Sub Prep schedule inputs should return no IDs.\n");
        return EXIT_FAILURE;
    }
    if (!selectedDayMatchingIsCanonicalAndExact())
    {
        std::fprintf(stderr, "Sub Prep selected days should match exact canonical text.\n");
        return EXIT_FAILURE;
    }
    if (!positiveIdsAreDeduplicatedGloballyAtTheirFirstOccurrence())
    {
        std::fprintf(stderr, "Sub Prep IDs should be positive and globally first-seen.\n");
        return EXIT_FAILURE;
    }
    if (!scheduleTraversalDefinesOutputOrderIndependentOfSelectionOrder())
    {
        std::fprintf(stderr, "Sub Prep ID output should follow schedule traversal.\n");
        return EXIT_FAILURE;
    }
    if (!regularAndTestingEntriesAreBothIncluded())
    {
        std::fprintf(stderr, "Sub Prep projection should include all entry kinds.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
