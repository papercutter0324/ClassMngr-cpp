#include "next/application/sub_prep_selected_class_ids.h"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;
using ClassMngr::Next::Domain::ClassId;

namespace
{

ClassId typedClassId(std::string_view value)
{
    return *ClassId::fromString(value);
}

using Projection = decltype(projectSubPrepSelectedClassIds(
    std::declval<const std::vector<std::u16string>&>(),
    std::declval<const std::vector<SubPrepSelectedClassScheduleRow>&>()
    ));

static_assert(std::is_same_v<
    decltype(SubPrepSelectedClassScheduleEntry::classId),
    ClassId
    >);
static_assert(std::is_same_v<Projection, std::vector<ClassId>>);

bool emptyInputsReturnNoClassIds()
{
    return projectSubPrepSelectedClassIds({}, {}).empty()
        && projectSubPrepSelectedClassIds(
            {u"Monday"},
            {}
            ).empty()
        && projectSubPrepSelectedClassIds(
            {},
            {{.cells = {{
                .day = u"Monday",
                .entries = {{typedClassId("42")}}
            }}}}
            ).empty();
}

bool selectedDayMatchingIsCanonicalAndExact()
{
    const std::vector<SubPrepSelectedClassScheduleRow> rows{
        {
            .cells = {
                {
                    .day = u"Monday",
                    .entries = {{typedClassId("42")}}
                }
            }
        }
    };

    return projectSubPrepSelectedClassIds({u"monday"}, rows).empty()
        && projectSubPrepSelectedClassIds({u" Monday"}, rows).empty()
        && projectSubPrepSelectedClassIds({u"Monday "}, rows).empty()
        && projectSubPrepSelectedClassIds({u"Monday"}, rows)
            == std::vector<ClassId>{typedClassId("42")};
}

bool positiveIdsAreDeduplicatedNumericallyAndCanonicalized()
{
    const std::vector<SubPrepSelectedClassScheduleRow> rows{
        {
            .cells = {
                {
                    .day = u"Monday",
                    .entries = {
                        {typedClassId("-3")},
                        {typedClassId("0")},
                        {typedClassId("00042")},
                        {typedClassId("17")},
                        {typedClassId("42")}
                    }
                },
                {
                    .day = u"Monday",
                    .entries = {
                        {typedClassId("017")},
                        {typedClassId("99")},
                        {typedClassId("00099")},
                        {typedClassId("0")}
                    }
                }
            }
        },
        {
            .cells = {
                {
                    .day = u"Monday",
                    .entries = {
                        {typedClassId("42")},
                        {typedClassId("101")}
                    }
                }
            }
        }
    };

    return projectSubPrepSelectedClassIds({u"Monday"}, rows)
        == std::vector<ClassId>{
            typedClassId("42"),
            typedClassId("17"),
            typedClassId("99"),
            typedClassId("101")
        };
}

bool onlyFullyParsedPositiveIntValuesAreReturned()
{
    const std::string maximum = std::to_string(
        std::numeric_limits<int>::max()
        );
    const std::string overflow = std::to_string(
        static_cast<long long>(std::numeric_limits<int>::max()) + 1
        );
    const std::string underflow = std::to_string(
        static_cast<long long>(std::numeric_limits<int>::min()) - 1
        );
    const std::vector<SubPrepSelectedClassScheduleRow> rows{
        {
            .cells = {
                {
                    .day = u"Monday",
                    .entries = {
                        {typedClassId("1")},
                        {typedClassId(maximum)},
                        {typedClassId("not-a-number")},
                        {typedClassId("42tail")},
                        {typedClassId(" 42")},
                        {typedClassId("42 ")},
                        {typedClassId("0x2a")},
                        {typedClassId("+42")},
                        {typedClassId(overflow)},
                        {typedClassId("0")},
                        {typedClassId("-0")},
                        {typedClassId("-42")},
                        {typedClassId(underflow)}
                    }
                }
            }
        }
    };

    return projectSubPrepSelectedClassIds({u"Monday"}, rows)
        == std::vector<ClassId>{typedClassId("1"), typedClassId(maximum)};
}

bool scheduleTraversalDefinesOutputOrderIndependentOfSelectionOrder()
{
    const std::vector<SubPrepSelectedClassScheduleRow> rows{
        {
            .cells = {
                {
                    .day = u"Monday",
                    .entries = {{typedClassId("10")}, {typedClassId("11")}}
                },
                {
                    .day = u"Wednesday",
                    .entries = {{typedClassId("20")}}
                }
            }
        },
        {
            .cells = {
                {
                    .day = u"Friday",
                    .entries = {{typedClassId("30")}}
                },
                {
                    .day = u"Monday",
                    .entries = {{typedClassId("12")}}
                }
            }
        }
    };

    return projectSubPrepSelectedClassIds(
        {u"Wednesday", u"Monday"},
        rows
        ) == std::vector<ClassId>{
            typedClassId("10"),
            typedClassId("11"),
            typedClassId("20"),
            typedClassId("12")
        };
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
                            .classId = typedClassId("51"),
                            .kind = SubPrepSelectedClassScheduleEntryKind::
                                RegularClass
                        },
                        {
                            .classId = typedClassId("62"),
                            .kind = SubPrepSelectedClassScheduleEntryKind::
                                TestingClass
                        }
                    }
                }
            }
        }
    };

    return projectSubPrepSelectedClassIds({u"Tuesday"}, rows)
        == std::vector<ClassId>{typedClassId("51"), typedClassId("62")};
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
    if (!positiveIdsAreDeduplicatedNumericallyAndCanonicalized())
    {
        std::fprintf(stderr, "Sub Prep IDs should be positive, canonical, and first-seen.\n");
        return EXIT_FAILURE;
    }
    if (!onlyFullyParsedPositiveIntValuesAreReturned())
    {
        std::fprintf(stderr, "Sub Prep IDs should be fully parsed positive ints.\n");
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
