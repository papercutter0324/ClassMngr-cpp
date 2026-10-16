#include "next/application/speaking_evaluation_schedule_label_projection.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

using ClassMngr::Next::Application::SpeakingEvaluationScheduleLabel;
using ClassMngr::Next::Application::projectSpeakingEvaluationScheduleLabel;

namespace
{

bool projectsLabelsAsExpected(
    const std::vector<SpeakingEvaluationScheduleLabel>& labels,
    const std::u16string_view expected
    )
{
    return projectSpeakingEvaluationScheduleLabel(labels) == expected;
}

bool emptyAndSingleGroupFormattingArePreserved()
{
    return projectsLabelsAsExpected({}, u"")
        && projectsLabelsAsExpected(
            {{u"M", u"4pm"}},
            u"M - 4pm"
            )
        && projectsLabelsAsExpected(
            {{u"", u"4pm"}},
            u"4pm"
            )
        && projectsLabelsAsExpected(
            {{u"M", u""}},
            u"M"
            );
}

bool sameTimeDaysRetainFirstSeenOrderAndDeduplicateWithinTheGroup()
{
    return projectsLabelsAsExpected(
        {
            {u"W", u"4pm"},
            {u"M", u"4pm"},
            {u"W", u"4pm"},
            {u"F", u"4pm"}
        },
        u"WMF - 4pm"
        );
}

bool mixedTimesRetainFirstSeenGroupOrderAndAllowDaysAcrossGroups()
{
    return projectsLabelsAsExpected(
        {
            {u"W", u"5pm"},
            {u"M", u"4pm"},
            {u"M", u"5pm"},
            {u"W", u"4pm"}
        },
        u"WM - 5pm, MW - 4pm"
        );
}

bool missingDayOrTimeRendersTheAvailableSideAndEmptyPairsAreIgnored()
{
    return projectsLabelsAsExpected(
        {
            {u"M", u""},
            {u"", u"4pm"},
            {u"", u""}
        },
        u"M, 4pm"
        );
}

bool exactTimeLabelsDefineDistinctGroups()
{
    return projectsLabelsAsExpected(
        {
            {u"M", u"4pm"},
            {u"W", u"4PM"}
        },
        u"M - 4pm, W - 4PM"
        );
}

} // namespace

int main()
{
    if (!emptyAndSingleGroupFormattingArePreserved())
    {
        std::fprintf(stderr, "Single-group schedule label projection was incorrect.\n");
        return EXIT_FAILURE;
    }
    if (!sameTimeDaysRetainFirstSeenOrderAndDeduplicateWithinTheGroup())
    {
        std::fprintf(stderr, "Same-time schedule day grouping was incorrect.\n");
        return EXIT_FAILURE;
    }
    if (!mixedTimesRetainFirstSeenGroupOrderAndAllowDaysAcrossGroups())
    {
        std::fprintf(stderr, "Mixed-time schedule grouping was incorrect.\n");
        return EXIT_FAILURE;
    }
    if (!missingDayOrTimeRendersTheAvailableSideAndEmptyPairsAreIgnored())
    {
        std::fprintf(stderr, "Schedule labels with missing sides were incorrect.\n");
        return EXIT_FAILURE;
    }
    if (!exactTimeLabelsDefineDistinctGroups())
    {
        std::fprintf(stderr, "Exact schedule time grouping was incorrect.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
