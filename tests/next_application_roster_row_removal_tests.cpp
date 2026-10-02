#include "next/application/roster_row_removal.h"

#include <cstdio>
#include <cstdlib>
#include <variant>

using ClassMngr::Next::Application::RosterRowRemovalError;
using ClassMngr::Next::Application::RosterRowRemovalErrorCode;
using ClassMngr::Next::Application::RosterRowRemovalResult;
using ClassMngr::Next::Application::RosterSnapshot;
using ClassMngr::Next::Application::removeRosterRow;

namespace
{

RosterSnapshot sampleRoster()
{
    return {
        .columns = {u"English", u"Korean", u"Review"},
        .columnWidths = {210, 160, 240},
        .rows = {
            {u"Amy", u"\uAE40\uBBFC\uC9C0", u"Score A"},
            {u"Ben", u"\uC774\uC11C\uC900", u"Score B"},
            {u"Cal", u"\uBC15\uC11C\uC900", u"Review \U0001F4DA"},
            {u"Dee", u"\uCD5C\uC218\uC5F0", u"Score D"}
        }
    };
}

bool isError(
    const RosterRowRemovalResult& result,
    const RosterRowRemovalErrorCode expected
    )
{
    const auto* error = std::get_if<RosterRowRemovalError>(&result);
    return error && error->code == expected;
}

bool compactsCompleteRowsAndPreservesSnapshotMetadata()
{
    const RosterSnapshot original = sampleRoster();
    const auto result = removeRosterRow(original, 1);

    RosterSnapshot expected = original;
    expected.rows = {
        original.rows[0],
        original.rows[2],
        original.rows[3],
        {u"", u"", u""}
    };

    return std::holds_alternative<RosterSnapshot>(result)
        && std::get<RosterSnapshot>(result) == expected
        && std::get<RosterSnapshot>(result).rows.size() == original.rows.size()
        && std::get<RosterSnapshot>(result).columns == original.columns
        && std::get<RosterSnapshot>(result).columnWidths == original.columnWidths;
}

bool clearsTheLastSlotWhenRemovingTheLastPopulatedRow()
{
    const RosterSnapshot original = sampleRoster();
    const auto result = removeRosterRow(original, 3);

    RosterSnapshot expected = original;
    expected.rows.back() = {u"", u"", u""};
    return std::holds_alternative<RosterSnapshot>(result)
        && std::get<RosterSnapshot>(result) == expected;
}

bool rejectsInvalidIndexes()
{
    const RosterSnapshot roster = sampleRoster();
    return isError(
               removeRosterRow(roster, -1),
               RosterRowRemovalErrorCode::InvalidRowIndex
               )
        && isError(
               removeRosterRow(roster, 4),
               RosterRowRemovalErrorCode::InvalidRowIndex
               )
        && isError(
               removeRosterRow(RosterSnapshot{}, 0),
               RosterRowRemovalErrorCode::InvalidRowIndex
               );
}

bool rejectsEmptyAndQStringTrimmedWhitespaceOnlyRows()
{
    const RosterSnapshot roster{
        .columns = {u"English", u"Korean", u"Review"},
        .columnWidths = {210, 160, 240},
        .rows = {
            {u"", u"", u""},
            {u" \t\n\v\f\r", u"\u0085\u00A0\u1680", u"\u2000\u2001\u2002"
             u"\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A"
             u"\u2028\u2029\u202F\u205F\u3000"},
            {u"\u2003 Student \u3000", u"\uAE40\uBBFC\uC9C0", u"Review"}
        }
    };

    return isError(
               removeRosterRow(roster, 0),
               RosterRowRemovalErrorCode::RowHasNoData
               )
        && isError(
               removeRosterRow(roster, 1),
               RosterRowRemovalErrorCode::RowHasNoData
               )
        && std::holds_alternative<RosterSnapshot>(removeRosterRow(roster, 2));
}

} // namespace

int main()
{
    if (!compactsCompleteRowsAndPreservesSnapshotMetadata())
    {
        std::fprintf(stderr, "Row compaction or snapshot metadata was incorrect.\n");
        return EXIT_FAILURE;
    }
    if (!clearsTheLastSlotWhenRemovingTheLastPopulatedRow())
    {
        std::fprintf(stderr, "Removing the last row did not clear its slot.\n");
        return EXIT_FAILURE;
    }
    if (!rejectsInvalidIndexes())
    {
        std::fprintf(stderr, "An invalid removal index was accepted.\n");
        return EXIT_FAILURE;
    }
    if (!rejectsEmptyAndQStringTrimmedWhitespaceOnlyRows())
    {
        std::fprintf(stderr, "Empty or whitespace-only row eligibility was incorrect.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
