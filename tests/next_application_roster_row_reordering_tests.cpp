#include "next/application/roster_row_reordering.h"

#include <cstdlib>
#include <variant>

using ClassMngr::Next::Application::RosterRowReorderingError;
using ClassMngr::Next::Application::RosterRowReorderingErrorCode;
using ClassMngr::Next::Application::RosterRowReorderingResult;
using ClassMngr::Next::Application::RosterSnapshot;
using ClassMngr::Next::Application::reorderRosterRows;

namespace
{

RosterSnapshot sampleRoster()
{
    return {
        .columns = {u"English", u"Korean", u"Review"},
        .columnWidths = {210, 160, 240},
        .rows = {
            {u"  Amy\u00A0", u"\uAE40\uBBFC\uC9C0", u" \tReview\u3000"},
            {u"Ben", u"\uC774\uC11C\uC900", u"Score B"},
            {u"Cal", u"\uBC15\uC11C\uC900", u"Review \U0001F4DA"},
            {u"Dee", u"\uCD5C\uC218\uC5F0", u"Tail"}
        }
    };
}

bool isError(
    const RosterRowReorderingResult& result,
    const RosterRowReorderingErrorCode expected
    )
{
    const auto* error = std::get_if<RosterRowReorderingError>(&result);
    return error && error->code == expected;
}

bool movesCompleteRowsInBothDirections()
{
    const RosterSnapshot original = sampleRoster();

    const auto forward = reorderRosterRows(original, 0, 2);
    RosterSnapshot expectedForward = original;
    expectedForward.rows = {
        original.rows[1],
        original.rows[2],
        original.rows[0],
        original.rows[3]
    };
    if (
        !std::holds_alternative<RosterSnapshot>(forward)
        || std::get<RosterSnapshot>(forward) != expectedForward
        || std::get<RosterSnapshot>(forward).rows.size() != original.rows.size()
        )
    {
        return false;
    }

    const auto backward = reorderRosterRows(original, 3, 1);
    RosterSnapshot expectedBackward = original;
    expectedBackward.rows = {
        original.rows[0],
        original.rows[3],
        original.rows[1],
        original.rows[2]
    };
    return std::holds_alternative<RosterSnapshot>(backward)
        && std::get<RosterSnapshot>(backward) == expectedBackward
        && std::get<RosterSnapshot>(backward).rows.size() == original.rows.size();
}

bool rejectsInvalidAndIdenticalIndexes()
{
    const RosterSnapshot roster = sampleRoster();
    return isError(
               reorderRosterRows(roster, -1, 1),
               RosterRowReorderingErrorCode::InvalidSourceIndex
               )
        && isError(
               reorderRosterRows(roster, 4, 1),
               RosterRowReorderingErrorCode::InvalidSourceIndex
               )
        && isError(
               reorderRosterRows(roster, 1, -1),
               RosterRowReorderingErrorCode::InvalidDestinationIndex
               )
        && isError(
               reorderRosterRows(roster, 1, 4),
               RosterRowReorderingErrorCode::InvalidDestinationIndex
               )
        && isError(
               reorderRosterRows(roster, 2, 2),
               RosterRowReorderingErrorCode::SameRow
               );
}

bool rejectsEmptyAndUnicodeWhitespaceOnlyRows()
{
    const RosterSnapshot emptySource{
        .columns = {u"English", u"Review"},
        .columnWidths = {210, 240},
        .rows = {{}, {u"Target", u"Keep"}}
    };
    if (!isError(
            reorderRosterRows(emptySource, 0, 1),
            RosterRowReorderingErrorCode::SourceRowHasNoData
            ))
    {
        return false;
    }

    const RosterSnapshot whitespaceOnlySource{
        .columns = {u"English", u"Korean", u"Review"},
        .columnWidths = {210, 160, 240},
        .rows = {
            {u" \t\n\v\f\r", u"\u0085\u00A0\u1680", u"\u2000\u2001\u2002"
             u"\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A",
             u"\u2028\u2029\u202F\u205F\u3000"},
            {u"Target", u"\uAE40\uBBFC\uC9C0", u"Keep"}
        }
    };
    return isError(
        reorderRosterRows(whitespaceOnlySource, 0, 1),
        RosterRowReorderingErrorCode::SourceRowHasNoData
        );
}

bool preservesNonBlankWhitespaceAndEveryCell()
{
    RosterSnapshot original = sampleRoster();
    original.rows[0] = {
        u"\u3000Name\u00A0",
        u"\uAE40\uBBFC\uC9C0",
        u"\tReview\u2028"
    };

    const auto result = reorderRosterRows(original, 0, 3);
    RosterSnapshot expected = original;
    expected.rows = {
        original.rows[1],
        original.rows[2],
        original.rows[3],
        original.rows[0]
    };
    return std::holds_alternative<RosterSnapshot>(result)
        && std::get<RosterSnapshot>(result) == expected;
}

} // namespace

int main()
{
    return movesCompleteRowsInBothDirections()
            && rejectsInvalidAndIdenticalIndexes()
            && rejectsEmptyAndUnicodeWhitespaceOnlyRows()
            && preservesNonBlankWhitespaceAndEveryCell()
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
