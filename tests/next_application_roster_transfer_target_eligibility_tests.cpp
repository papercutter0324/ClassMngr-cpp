#include "next/application/roster_transfer_target_eligibility.h"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using ClassMngr::Next::Domain::ClassId;
using ClassMngr::Next::Application::isRosterTransferSourceEligible;
using ClassMngr::Next::Application::isRosterTransferTargetEligible;
using ClassMngr::Next::Application::shouldReadRosterTransferTargetClassInfo;

ClassId classId(const std::string_view value)
{
    return *ClassId::fromString(value);
}

bool sourceEligibilityRequiresCanonicalPositiveIdAndNonemptyTrimmedGrade()
{
    return isRosterTransferSourceEligible(classId("4"), u" Grade 7 ")
        && isRosterTransferSourceEligible(
            classId(std::to_string(std::numeric_limits<int>::max())),
            u"Grade 7"
            )
        && !isRosterTransferSourceEligible(classId("0"), u"Grade 7")
        && !isRosterTransferSourceEligible(classId("-2"), u"Grade 7")
        && !isRosterTransferSourceEligible(classId("04"), u"Grade 7")
        && !isRosterTransferSourceEligible(classId("4tail"), u"Grade 7")
        && !isRosterTransferSourceEligible(classId("4 "), u"Grade 7")
        && !isRosterTransferSourceEligible(classId("2147483648"), u"Grade 7")
        && !isRosterTransferSourceEligible(classId("4"), u"")
        && !isRosterTransferSourceEligible(classId("4"), u"\t\u00a0\u3000 ");
}

bool targetIdPrelookupGuardRejectsInvalidAndCurrentIds()
{
    const ClassId source = classId("4");
    const ClassId other = classId("9");
    const std::vector<std::string_view> invalidIds{
        "0",
        "-2",
        "04",
        "4tail",
        "4 ",
        "2147483648"
    };

    if (!shouldReadRosterTransferTargetClassInfo(source, other)
        || shouldReadRosterTransferTargetClassInfo(source, source))
    {
        return false;
    }

    for (const std::string_view value : invalidIds)
    {
        const ClassId invalid = classId(value);
        if (shouldReadRosterTransferTargetClassInfo(source, invalid)
            || shouldReadRosterTransferTargetClassInfo(invalid, other))
        {
            return false;
        }
    }
    return true;
}

bool targetEligibilityTrimsGradesButKeepsExactCase()
{
    const ClassId source = classId("4");
    const ClassId target = classId("9");
    return isRosterTransferTargetEligible(
               source,
               u"\u00a0Grade 7\u3000",
               target,
               u"\tGrade 7 "
               )
        && !isRosterTransferTargetEligible(
            source,
            u"Grade 7",
            target,
            u"grade 7"
            )
        && !isRosterTransferTargetEligible(
            source,
            u"Grade 7",
            target,
            u"Grade 8"
            )
        && !isRosterTransferTargetEligible(
            source,
            u"Grade 7",
            target,
            u"\t\u00a0\u3000"
            );
}

bool fullEligibilityRejectsInvalidSourceAndTargetIds()
{
    const ClassId validSource = classId("4");
    const ClassId validTarget = classId("9");
    const std::vector<std::string_view> invalidIds{
        "0",
        "-1",
        "04",
        "4tail",
        "4 ",
        "2147483648"
    };

    if (!isRosterTransferTargetEligible(
            classId(std::to_string(std::numeric_limits<int>::max())),
            u"Grade 7",
            validTarget,
            u"Grade 7"
            )
        || !isRosterTransferTargetEligible(
            validSource,
            u"Grade 7",
            classId(std::to_string(std::numeric_limits<int>::max())),
            u"Grade 7"
            )
        || isRosterTransferTargetEligible(
            validSource,
            u"\u00a0\t",
            validTarget,
            u"Grade 7"
            ))
    {
        return false;
    }

    for (const std::string_view value : invalidIds)
    {
        const ClassId invalid = classId(value);
        if (isRosterTransferTargetEligible(
                invalid,
                u"Grade 7",
                validTarget,
                u"Grade 7"
                )
            || isRosterTransferTargetEligible(
                validSource,
                u"Grade 7",
                invalid,
                u"Grade 7"
                ))
        {
            return false;
        }
    }

    return !isRosterTransferTargetEligible(
        validSource,
        u"Grade 7",
        validSource,
        u"Grade 7"
        );
}

bool candidateIterationKeepsOrderAndDuplicates()
{
    struct Candidate final
    {
        ClassId classId;
        std::u16string_view grade;
    };

    const ClassId source = classId("4");
    const std::vector<Candidate> candidates{
        {classId("9"), u" Grade 7 "},
        {classId("3"), u"Grade 7"},
        {classId("9"), u"Grade 7"},
        {classId("-1"), u"Grade 7"},
        {source, u"Grade 7"}
    };
    std::vector<ClassId> eligibleIds;
    for (const Candidate& candidate : candidates)
    {
        if (shouldReadRosterTransferTargetClassInfo(source, candidate.classId)
            && isRosterTransferTargetEligible(
                source,
                u"Grade 7",
                candidate.classId,
                candidate.grade
                ))
        {
            eligibleIds.push_back(candidate.classId);
        }
    }
    return eligibleIds == std::vector<ClassId>{
        classId("9"), classId("3"), classId("9")
    };
}

} // namespace

int main()
{
    if (!sourceEligibilityRequiresCanonicalPositiveIdAndNonemptyTrimmedGrade())
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
