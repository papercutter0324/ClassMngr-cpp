#include "next/application/roster_custom_column_append.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using ClassMngr::Next::Application::RosterCustomColumnAppendError;
using ClassMngr::Next::Application::RosterCustomColumnAppendResult;
using ClassMngr::Next::Application::RosterCustomColumnNameRejection;
using ClassMngr::Next::Application::RosterSnapshot;
using ClassMngr::Next::Application::appendRosterCustomColumn;

namespace
{

bool asciiCaseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        char16_t lhs = left[index];
        char16_t rhs = right[index];
        if (lhs >= u'A' && lhs <= u'Z')
        {
            lhs = static_cast<char16_t>(lhs - u'A' + u'a');
        }
        if (rhs >= u'A' && rhs <= u'Z')
        {
            rhs = static_cast<char16_t>(rhs - u'A' + u'a');
        }
        if (lhs != rhs)
        {
            return false;
        }
    }
    return true;
}

RosterCustomColumnAppendResult append(
    const RosterSnapshot& roster,
    const std::u16string_view name
    )
{
    return appendRosterCustomColumn(
        roster,
        name,
        {u"English", u"Korean", u"Winter", u"Speech Contest", u"Summer", u"Fall"},
        asciiCaseInsensitiveEquals
        );
}

bool appendsNormalizedNameAndPreservesAllDataAndWidths()
{
    const RosterSnapshot original{
        .columns = {u"English", u"Korean"},
        .columnWidths = {211, 143},
        .rows = {
            {u"Amy", u"\uAE40\uBBFC\uC9C0"},
            {u"Ben", u"\uC774\uC11C\uC900", u"Existing extra cell"}
        }
    };
    const RosterSnapshot expected{
        .columns = {u"English", u"Korean", u"Parent Contact"},
        .columnWidths = {211, 143},
        .rows = {
            {u"Amy", u"\uAE40\uBBFC\uC9C0", u""},
            {u"Ben", u"\uC774\uC11C\uC900", u"Existing extra cell", u""}
        }
    };
    const auto result = append(
        original,
        u"  Parent\t\u00a0 Contact  "
        );
    return std::holds_alternative<RosterSnapshot>(result)
        && std::get<RosterSnapshot>(result) == expected
        && original.columns.size() == 2
        && original.columnWidths == std::vector<int>{211, 143};
}

bool appendsToAnEmptyRosterWithoutInventingRowsOrWidths()
{
    const RosterSnapshot original;
    const auto result = append(original, u"Review");
    return std::holds_alternative<RosterSnapshot>(result)
        && std::get<RosterSnapshot>(result)
            == RosterSnapshot{.columns = {u"Review"}};
}

bool rejectedNamesReturnTypedErrorsWithoutChangingTheInput()
{
    const RosterSnapshot original{
        .columns = {u"English", u"Fall", u"Review"},
        .columnWidths = {211, 143, 242},
        .rows = {{u"Amy", u"\uAE40\uBBFC\uC9C0", u"Note"}}
    };
    const std::vector<std::pair<
        std::u16string_view,
        RosterCustomColumnNameRejection>> rejectedNames{
        {u" \t\u00a0 ", RosterCustomColumnNameRejection::Empty},
        {u" review ", RosterCustomColumnNameRejection::Duplicate},
        {u"Korean", RosterCustomColumnNameRejection::RequiredColumn}
    };

    for (const auto& [name, expected] : rejectedNames)
    {
        const auto result = append(original, name);
        const auto* error = std::get_if<RosterCustomColumnAppendError>(&result);
        if (!error || error->rejection != expected)
        {
            return false;
        }
    }

    const RosterSnapshot noFallColumn{
        .columns = {u"English", u"Review"}
    };
    const auto aliasResult = append(noFallColumn, u"Autumn");
    const auto* aliasError =
        std::get_if<RosterCustomColumnAppendError>(&aliasResult);

    return aliasError
            && aliasError->rejection == RosterCustomColumnNameRejection::RequiredColumn
        && original.columns == std::vector<std::u16string>{u"English", u"Fall", u"Review"}
        && original.columnWidths == std::vector<int>{211, 143, 242}
        && original.rows == std::vector<std::vector<std::u16string>>{
            {u"Amy", u"\uAE40\uBBFC\uC9C0", u"Note"}
        };
}

} // namespace

int main()
{
    if (!appendsNormalizedNameAndPreservesAllDataAndWidths())
    {
        std::fprintf(stderr, "Custom-column append did not preserve and extend the snapshot.\n");
        return EXIT_FAILURE;
    }
    if (!appendsToAnEmptyRosterWithoutInventingRowsOrWidths())
    {
        std::fprintf(stderr, "Appending to an empty roster changed its empty metadata.\n");
        return EXIT_FAILURE;
    }
    if (!rejectedNamesReturnTypedErrorsWithoutChangingTheInput())
    {
        std::fprintf(stderr, "A rejected custom-column name changed or was accepted by the snapshot.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
