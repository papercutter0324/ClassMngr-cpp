#include "next/application/roster_transfer_target_eligibility.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

using ClassMngr::Next::Application::isRosterTransferSourceEligible;
using ClassMngr::Next::Application::isRosterTransferTargetEligible;
using ClassMngr::Next::Application::shouldReadRosterTransferTargetClassInfo;

namespace
{

bool sourceEligibilityRequiresPositiveIdAndNonemptyTrimmedGrade()
{
    return isRosterTransferSourceEligible(4, u" Grade 7 ")
        && !isRosterTransferSourceEligible(0, u"Grade 7")
        && !isRosterTransferSourceEligible(-2, u"Grade 7")
        && !isRosterTransferSourceEligible(4, u"")
        && !isRosterTransferSourceEligible(4, u"\t\u00a0\u3000 ");
}

bool targetIdPrelookupGuardRejectsInvalidAndCurrentIds()
{
    return shouldReadRosterTransferTargetClassInfo(4, 9)
        && !shouldReadRosterTransferTargetClassInfo(0, 9)
        && !shouldReadRosterTransferTargetClassInfo(-2, 9)
        && !shouldReadRosterTransferTargetClassInfo(4, 0)
        && !shouldReadRosterTransferTargetClassInfo(4, -1)
        && !shouldReadRosterTransferTargetClassInfo(4, 4);
}

bool targetEligibilityTrimsGradesButKeepsExactCase()
{
    return isRosterTransferTargetEligible(
               4,
               u"\u00a0Grade 7\u3000",
               9,
               u"\tGrade 7 "
               )
        && !isRosterTransferTargetEligible(
            4,
            u"Grade 7",
            9,
            u"grade 7"
            )
        && !isRosterTransferTargetEligible(
            4,
            u"Grade 7",
            9,
            u"Grade 8"
            )
        && !isRosterTransferTargetEligible(
            4,
            u"Grade 7",
            9,
            u"\t\u00a0\u3000"
            );
}

bool fullEligibilityRejectsInvalidSourceAndTargetIds()
{
    return !isRosterTransferTargetEligible(0, u"Grade 7", 9, u"Grade 7")
        && !isRosterTransferTargetEligible(-1, u"Grade 7", 9, u"Grade 7")
        && !isRosterTransferTargetEligible(4, u"Grade 7", 0, u"Grade 7")
        && !isRosterTransferTargetEligible(4, u"Grade 7", -1, u"Grade 7")
        && !isRosterTransferTargetEligible(4, u"Grade 7", 4, u"Grade 7")
        && !isRosterTransferTargetEligible(4, u"\u00a0\t", 9, u"Grade 7");
}

bool candidateIterationKeepsOrderAndDuplicates()
{
    struct Candidate final
    {
        int classId;
        std::u16string_view grade;
    };

    const std::vector<Candidate> candidates{
        {9, u" Grade 7 "},
        {3, u"Grade 7"},
        {9, u"Grade 7"},
        {-1, u"Grade 7"},
        {4, u"Grade 7"}
    };
    std::vector<int> eligibleIds;
    for (const Candidate& candidate : candidates)
    {
        if (shouldReadRosterTransferTargetClassInfo(4, candidate.classId)
            && isRosterTransferTargetEligible(
                4,
                u"Grade 7",
                candidate.classId,
                candidate.grade
                ))
        {
            eligibleIds.push_back(candidate.classId);
        }
    }
    return eligibleIds == std::vector<int>{9, 3, 9};
}

} // namespace

int main()
{
    if (!sourceEligibilityRequiresPositiveIdAndNonemptyTrimmedGrade())
    {
        std::fprintf(stderr, "Invalid roster transfer sources were accepted.\n");
        return EXIT_FAILURE;
    }
    if (!targetIdPrelookupGuardRejectsInvalidAndCurrentIds())
    {
        std::fprintf(stderr, "An invalid/current target passed the prelookup guard.\n");
        return EXIT_FAILURE;
    }
    if (!targetEligibilityTrimsGradesButKeepsExactCase())
    {
        std::fprintf(stderr, "Grade matching did not preserve trimmed case-sensitive equality.\n");
        return EXIT_FAILURE;
    }
    if (!fullEligibilityRejectsInvalidSourceAndTargetIds())
    {
        std::fprintf(stderr, "Full roster transfer eligibility accepted invalid IDs or source grade.\n");
        return EXIT_FAILURE;
    }
    if (!candidateIterationKeepsOrderAndDuplicates())
    {
        std::fprintf(stderr, "Candidate order or duplicate candidates were changed.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
