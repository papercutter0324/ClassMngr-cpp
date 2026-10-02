#include "next/application/student_name_pair_lookup.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

using ClassMngr::Next::Application::StudentNamePairText;
using ClassMngr::Next::Application::lookupStudentNamePairPeers;
using ClassMngr::Next::Application::studentNamePairLookupKey;

namespace
{

StudentNamePairText names(
    std::u16string englishName,
    std::u16string koreanName
    )
{
    return {
        std::move(englishName),
        std::move(koreanName)
    };
}

bool invalidSelectedRowsReturnNoPeers()
{
    const std::vector<StudentNamePairText> rows{
        names(u"Alex", u"\uAE40\uBBFC\uC9C0"),
        names(u"Alex", u"\uAE40\uBBFC\uC9C0")
    };

    return lookupStudentNamePairPeers({}, 0).empty()
        && lookupStudentNamePairPeers(rows, -1).empty()
        && lookupStudentNamePairPeers(rows, 2).empty();
}

bool incompleteSelectedAndCandidatePairsDoNotMatch()
{
    const std::vector<StudentNamePairText> rows{
        names(u"Alex", u""),
        names(u"Alex", u"\uAE40\uBBFC\uC9C0"),
        names(u"Alex", u"\uAE40\uBBFC\uC9C0"),
        names(u"", u"\uAE40\uBBFC\uC9C0")
    };

    return lookupStudentNamePairPeers(rows, 0).empty()
        && lookupStudentNamePairPeers(rows, 3).empty()
        && lookupStudentNamePairPeers(rows, 1) == std::vector<int>{2};
}

bool keyTrimsQtWhitespaceAndKeepsNameEqualityCaseSensitive()
{
    const std::vector<StudentNamePairText> rows{
        names(u"\u00a0Alex\u3000", u" \uAE40\uBBFC\uC9C0\t"),
        names(u"Alex", u"\uAE40\uBBFC\uC9C0"),
        names(u"alex", u"\uAE40\uBBFC\uC9C0"),
        names(u"Alex", u"\uAE40\uBBFC\uC9C0")
    };

    return studentNamePairLookupKey(rows[0])
            == u"Alex\u001f\uAE40\uBBFC\uC9C0"
        && lookupStudentNamePairPeers(rows, 0) == std::vector<int>{1, 3};
}

bool delimiterCollisionsAndSelectedRowExclusionMatchLegacyBehavior()
{
    const std::vector<StudentNamePairText> rows{
        names(u"A\u001fB", u"C"),
        names(u"A", u"B\u001fC"),
        names(u"A\u001fB", u"C"),
        names(u"A", u"B\u001fC")
    };

    return lookupStudentNamePairPeers(rows, 2) == std::vector<int>{0, 1, 3};
}

} // namespace

int main()
{
    if (!invalidSelectedRowsReturnNoPeers())
    {
        std::fprintf(stderr, "Invalid selected rows unexpectedly returned peers.\n");
        return EXIT_FAILURE;
    }
    if (!incompleteSelectedAndCandidatePairsDoNotMatch())
    {
        std::fprintf(stderr, "Incomplete name pairs were treated as duplicates.\n");
        return EXIT_FAILURE;
    }
    if (!keyTrimsQtWhitespaceAndKeepsNameEqualityCaseSensitive())
    {
        std::fprintf(stderr, "Pair key trimming or case-sensitive matching changed.\n");
        return EXIT_FAILURE;
    }
    if (!delimiterCollisionsAndSelectedRowExclusionMatchLegacyBehavior())
    {
        std::fprintf(stderr, "Separator collisions or peer order changed.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
