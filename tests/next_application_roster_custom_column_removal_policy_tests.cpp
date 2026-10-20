#include "next/application/roster_custom_column_removal_policy.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <type_traits>

using ClassMngr::Next::Application::RosterCustomColumnRemovalIndex;
using ClassMngr::Next::Application::RosterCustomColumnRemovalRejection;
using ClassMngr::Next::Application::RosterSnapshot;
using ClassMngr::Next::Application::canRemoveRosterCustomColumn;

static_assert(!std::is_convertible_v<int, RosterCustomColumnRemovalIndex>);
static_assert(!std::is_convertible_v<RosterCustomColumnRemovalIndex, int>);

constexpr RosterCustomColumnRemovalIndex DefaultRemovalIndex;
constexpr RosterCustomColumnRemovalIndex ExplicitRemovalIndex{7};
static_assert(DefaultRemovalIndex.rawValue() == -1);
static_assert(DefaultRemovalIndex == RosterCustomColumnRemovalIndex{-1});
static_assert(ExplicitRemovalIndex.rawValue() == 7);
static_assert(ExplicitRemovalIndex == RosterCustomColumnRemovalIndex{7});

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

bool rejectsInvalidIndexes()
{
    const RosterSnapshot roster{
        .columns = {u"English", u"Fall", u"Notes"}
    };
    const auto negative = canRemoveRosterCustomColumn(
        roster,
        RosterCustomColumnRemovalIndex{-1},
        {u"English", u"Fall"},
        asciiCaseInsensitiveEquals
        );
    const auto pastEnd = canRemoveRosterCustomColumn(
        roster,
        RosterCustomColumnRemovalIndex{3},
        {u"English", u"Fall"},
        asciiCaseInsensitiveEquals
        );
    return negative.rejection
            == RosterCustomColumnRemovalRejection::InvalidColumnIndex
        && pastEnd.rejection
            == RosterCustomColumnRemovalRejection::InvalidColumnIndex;
}

bool rejectsRequiredColumnsAndTheirAliases()
{
    const RosterSnapshot roster{
        .columns = {u"eNgLiSh", u"aUtUmN", u"Review"}
    };
    const auto required = canRemoveRosterCustomColumn(
        roster,
        RosterCustomColumnRemovalIndex{0},
        {u"English", u"Fall"},
        asciiCaseInsensitiveEquals
        );
    const auto alias = canRemoveRosterCustomColumn(
        roster,
        RosterCustomColumnRemovalIndex{1},
        {u"English", u"Fall"},
        asciiCaseInsensitiveEquals
        );
    return required.rejection
            == RosterCustomColumnRemovalRejection::RequiredColumn
        && alias.rejection
            == RosterCustomColumnRemovalRejection::RequiredColumn;
}

bool acceptsCustomColumn()
{
    const RosterSnapshot roster{
        .columns = {u"English", u"Fall", u"Review"}
    };
    return canRemoveRosterCustomColumn(
               roster,
               RosterCustomColumnRemovalIndex{2},
               {u"English", u"Korean", u"Winter", u"Speech Contest", u"Summer", u"Fall"},
               asciiCaseInsensitiveEquals
               )
        .accepted();
}

} // namespace

int main()
{
    if (!rejectsInvalidIndexes())
    {
        std::fprintf(stderr, "An invalid custom-column removal index was accepted.\n");
        return EXIT_FAILURE;
    }
    if (!rejectsRequiredColumnsAndTheirAliases())
    {
        std::fprintf(stderr, "A required column or its alias was removable.\n");
        return EXIT_FAILURE;
    }
    if (!acceptsCustomColumn())
    {
        std::fprintf(stderr, "A custom roster column was not removable.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
