#include "next/application/roster_row_availability.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using ClassMngr::Next::Application::firstEmptyRosterRow;
using ClassMngr::Next::Application::rosterRowHasData;

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
    return firstEmptyRosterRow({}) == 0
        && firstEmptyRosterRow({{}}) == 0
        && firstEmptyRosterRow({{u""}}) == 0;
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

    return firstEmptyRosterRow(leadingBlankRows) == 0
        && firstEmptyRosterRow(interiorBlankRows) == 2;
}

bool fullRowsReturnSizeAsNoRowSentinel()
{
    const std::vector<std::vector<std::u16string>> rows{
        {u"one"},
        {u"two"},
        {u"three"}
    };
    return firstEmptyRosterRow(rows) == rows.size();
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
    return EXIT_SUCCESS;
}
