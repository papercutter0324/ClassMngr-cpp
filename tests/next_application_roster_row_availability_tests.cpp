#include "next/application/roster_row_availability.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <vector>

using ClassMngr::Next::Application::firstEmptyRosterRow;
using ClassMngr::Next::Application::RosterFirstEmptyRowIndex;
using ClassMngr::Next::Application::rosterRowHasData;

static_assert(std::is_constructible_v<RosterFirstEmptyRowIndex, std::size_t>);
static_assert(!std::is_convertible_v<std::size_t, RosterFirstEmptyRowIndex>);
static_assert(!std::is_convertible_v<RosterFirstEmptyRowIndex, std::size_t>);

namespace
{

bool emptyAndWhitespaceOnlyRowsHaveNoData()
{
    return !rosterRowHasData({})
        && !rosterRowHasData({u"", u" \t\u00a0\u3000 "})
        && rosterRowHasData({u"", u" \t", u"data"});
}

bool emptyCollectionAndEmptyRowUseTheFirstAvailableIndex()
{
    return firstEmptyRosterRow({}) == RosterFirstEmptyRowIndex{}
        && firstEmptyRosterRow({{}}) == RosterFirstEmptyRowIndex{0}
        && firstEmptyRosterRow({{u""}}) == RosterFirstEmptyRowIndex{0};
}

bool firstEmptyRowIndexHasExplicitUnsignedValueSemantics()
{
    const RosterFirstEmptyRowIndex defaultIndex;
    const RosterFirstEmptyRowIndex valueIndex{7};
    return defaultIndex.rawValue() == 0
        && valueIndex.rawValue() == 7
        && valueIndex == RosterFirstEmptyRowIndex{7}
        && valueIndex != defaultIndex;
}

bool firstEmptyRowUsesLowestIndexIncludingLeadingBlank()
{
    const std::vector<std::vector<std::u16string>> leadingBlankRows{
        {u" \u00a0\u3000 "},
        {u"filled"}
    };
    const std::vector<std::vector<std::u16string>> interiorBlankRows{
        {u"first"},
        {u"second"},
        {u" \t\u00a0 "},
        {u"later"}
    };

    return firstEmptyRosterRow(leadingBlankRows).rawValue() == 0
        && firstEmptyRosterRow(interiorBlankRows).rawValue() == 2;
}

bool fullRowsReturnSizeAsNoRowSentinel()
{
    const std::vector<std::vector<std::u16string>> rows{
        {u"one"},
        {u"two"},
        {u"three"}
    };
    return firstEmptyRosterRow(rows).rawValue() == rows.size();
}

bool modeledFullRosterReturnsModeledRowCountSentinel()
{
    std::vector<std::vector<std::u16string>> rows(
        25,
        {u"occupied"}
        );
    return firstEmptyRosterRow(rows).rawValue() == 25;
}

} // namespace

int main()
{
    if (!emptyAndWhitespaceOnlyRowsHaveNoData())
    {
        std::fprintf(stderr, "Row occupancy did not trim Qt whitespace.\n");
        return EXIT_FAILURE;
    }
    if (!emptyCollectionAndEmptyRowUseTheFirstAvailableIndex())
    {
        std::fprintf(stderr, "An empty roster or row did not select index zero.\n");
        return EXIT_FAILURE;
    }
    if (!firstEmptyRowIndexHasExplicitUnsignedValueSemantics())
    {
        std::fprintf(stderr, "First-empty row index lost its explicit value semantics.\n");
        return EXIT_FAILURE;
    }
    if (!firstEmptyRowUsesLowestIndexIncludingLeadingBlank())
    {
        std::fprintf(stderr, "The lowest empty row was not selected.\n");
        return EXIT_FAILURE;
    }
    if (!fullRowsReturnSizeAsNoRowSentinel())
    {
        std::fprintf(stderr, "A full roster did not return its size sentinel.\n");
        return EXIT_FAILURE;
    }
    if (!modeledFullRosterReturnsModeledRowCountSentinel())
    {
        std::fprintf(stderr, "A full modeled roster did not return the 25-row sentinel.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
