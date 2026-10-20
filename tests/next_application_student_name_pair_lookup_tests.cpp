#include "next/application/student_name_pair_lookup.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using ClassMngr::Next::Application::StudentNamePairPeerRowIndex;
using ClassMngr::Next::Application::StudentNamePairSelectedRowIndex;
using ClassMngr::Next::Application::StudentNamePairText;
using ClassMngr::Next::Application::lookupStudentNamePairPeers;
using ClassMngr::Next::Application::studentNamePairLookupKey;

static_assert(!std::is_same_v<
    StudentNamePairSelectedRowIndex,
    StudentNamePairPeerRowIndex
    >);
static_assert(!std::is_convertible_v<int, StudentNamePairSelectedRowIndex>);
static_assert(!std::is_convertible_v<StudentNamePairSelectedRowIndex, int>);
static_assert(!std::is_convertible_v<int, StudentNamePairPeerRowIndex>);
static_assert(!std::is_convertible_v<StudentNamePairPeerRowIndex, int>);

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

StudentNamePairSelectedRowIndex selectedRow(int value)
{
    return StudentNamePairSelectedRowIndex{value};
}

StudentNamePairPeerRowIndex peerRow(int value)
{
    return StudentNamePairPeerRowIndex{value};
}

bool rowIndexTypesKeepDistinctRawValues()
{
    const StudentNamePairSelectedRowIndex defaultSelectedRow;
    const StudentNamePairPeerRowIndex defaultPeerRow;

    return defaultSelectedRow.rawValue() == -1
        && defaultPeerRow.rawValue() == -1
        && selectedRow(7).rawValue() == 7
        && peerRow(8).rawValue() == 8
        && selectedRow(-9).rawValue() == -9
        && peerRow(-10).rawValue() == -10
        && selectedRow(7) == selectedRow(7)
        && selectedRow(7) != selectedRow(8)
        && peerRow(8) == peerRow(8)
        && peerRow(8) != peerRow(9);
}

bool invalidSelectedRowsReturnNoPeers()
{
    const std::vector<StudentNamePairText> rows{
        names(u"Alex", u"\uAE40\uBBFC\uC9C0"),
        names(u"Alex", u"\uAE40\uBBFC\uC9C0")
    };

    return lookupStudentNamePairPeers({}, selectedRow(0)).empty()
        && lookupStudentNamePairPeers(rows, selectedRow(-1)).empty()
        && lookupStudentNamePairPeers(rows, selectedRow(2)).empty();
}

bool incompleteSelectedAndCandidatePairsDoNotMatch()
{
    const std::vector<StudentNamePairText> rows{
        names(u"Alex", u""),
        names(u"Alex", u"\uAE40\uBBFC\uC9C0"),
        names(u"Alex", u"\uAE40\uBBFC\uC9C0"),
        names(u"", u"\uAE40\uBBFC\uC9C0")
    };

    return lookupStudentNamePairPeers(rows, selectedRow(0)).empty()
        && lookupStudentNamePairPeers(rows, selectedRow(3)).empty()
        && lookupStudentNamePairPeers(rows, selectedRow(1))
            == std::vector<StudentNamePairPeerRowIndex>{peerRow(2)};
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
        && lookupStudentNamePairPeers(rows, selectedRow(0))
            == std::vector<StudentNamePairPeerRowIndex>{
                peerRow(1),
                peerRow(3)
            };
}

bool delimiterCollisionsAndSelectedRowExclusionMatchLegacyBehavior()
{
    const std::vector<StudentNamePairText> rows{
        names(u"A\u001fB", u"C"),
        names(u"A", u"B\u001fC"),
        names(u"A\u001fB", u"C"),
        names(u"A", u"B\u001fC")
    };

    return lookupStudentNamePairPeers(rows, selectedRow(2))
        == std::vector<StudentNamePairPeerRowIndex>{
            peerRow(0),
            peerRow(1),
            peerRow(3)
        };
}

} // namespace

int main()
{
    if (!rowIndexTypesKeepDistinctRawValues())
    {
        std::fprintf(stderr, "Name-pair row index types lost their raw values.\n");
        return EXIT_FAILURE;
    }
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
