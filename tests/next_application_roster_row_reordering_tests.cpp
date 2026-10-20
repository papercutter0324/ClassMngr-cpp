#include "next/application/roster_row_reordering.h"

#include <cstdlib>
#include <type_traits>
#include <variant>

using ClassMngr::Next::Application::RosterRowReorderingError;
using ClassMngr::Next::Application::RosterRowReorderingErrorCode;
using ClassMngr::Next::Application::RosterRowReorderingDestinationIndex;
using ClassMngr::Next::Application::RosterRowReorderingResult;
using ClassMngr::Next::Application::RosterRowReorderingSourceIndex;
using ClassMngr::Next::Application::RosterSnapshot;
using ClassMngr::Next::Application::reorderRosterRows;

static_assert(!std::is_convertible_v<int, RosterRowReorderingSourceIndex>);
static_assert(!std::is_convertible_v<RosterRowReorderingSourceIndex, int>);
static_assert(!std::is_convertible_v<int, RosterRowReorderingDestinationIndex>);
static_assert(!std::is_convertible_v<RosterRowReorderingDestinationIndex, int>);
static_assert(!std::is_convertible_v<
    RosterRowReorderingSourceIndex,
    RosterRowReorderingDestinationIndex
    >);
static_assert(!std::is_convertible_v<
    RosterRowReorderingDestinationIndex,
    RosterRowReorderingSourceIndex
    >);

constexpr RosterRowReorderingSourceIndex DefaultSourceIndex{};
constexpr RosterRowReorderingSourceIndex ZeroSourceIndex{0};
constexpr RosterRowReorderingSourceIndex NonzeroSourceIndex{7};
constexpr RosterRowReorderingDestinationIndex DefaultDestinationIndex{};
constexpr RosterRowReorderingDestinationIndex ZeroDestinationIndex{0};
constexpr RosterRowReorderingDestinationIndex NonzeroDestinationIndex{9};
static_assert(DefaultSourceIndex.rawValue() == 0);
static_assert(DefaultSourceIndex == ZeroSourceIndex);
static_assert(NonzeroSourceIndex.rawValue() == 7);
static_assert(DefaultDestinationIndex.rawValue() == 0);
static_assert(DefaultDestinationIndex == ZeroDestinationIndex);
static_assert(NonzeroDestinationIndex.rawValue() == 9);

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

    const auto forward = reorderRosterRows(
        original,
        RosterRowReorderingSourceIndex{0},
        RosterRowReorderingDestinationIndex{2}
        );
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

    const auto backward = reorderRosterRows(
        original,
        RosterRowReorderingSourceIndex{3},
        RosterRowReorderingDestinationIndex{1}
        );
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
               reorderRosterRows(
                   roster,
                   RosterRowReorderingSourceIndex{-1},
                   RosterRowReorderingDestinationIndex{1}
                   ),
               RosterRowReorderingErrorCode::InvalidSourceIndex
               )
        && isError(
               reorderRosterRows(
                   roster,
                   RosterRowReorderingSourceIndex{4},
                   RosterRowReorderingDestinationIndex{1}
                   ),
               RosterRowReorderingErrorCode::InvalidSourceIndex
               )
        && isError(
               reorderRosterRows(
                   roster,
                   RosterRowReorderingSourceIndex{1},
                   RosterRowReorderingDestinationIndex{-1}
                   ),
               RosterRowReorderingErrorCode::InvalidDestinationIndex
               )
        && isError(
               reorderRosterRows(
                   roster,
                   RosterRowReorderingSourceIndex{1},
                   RosterRowReorderingDestinationIndex{4}
                   ),
               RosterRowReorderingErrorCode::InvalidDestinationIndex
               )
        && isError(
               reorderRosterRows(
                   roster,
                   RosterRowReorderingSourceIndex{2},
                   RosterRowReorderingDestinationIndex{2}
                   ),
               RosterRowReorderingErrorCode::SameRow
               )
        && isError(
               reorderRosterRows(
                   roster,
                   RosterRowReorderingSourceIndex{-1},
                   RosterRowReorderingDestinationIndex{4}
                   ),
               RosterRowReorderingErrorCode::InvalidSourceIndex
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
            reorderRosterRows(
                emptySource,
                RosterRowReorderingSourceIndex{0},
                RosterRowReorderingDestinationIndex{1}
                ),
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
        reorderRosterRows(
            whitespaceOnlySource,
            RosterRowReorderingSourceIndex{0},
            RosterRowReorderingDestinationIndex{1}
            ),
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

    const auto result = reorderRosterRows(
        original,
        RosterRowReorderingSourceIndex{0},
        RosterRowReorderingDestinationIndex{3}
        );
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
