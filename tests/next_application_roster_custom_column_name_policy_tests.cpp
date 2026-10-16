#include "next/application/roster_custom_column_name_policy.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

using ClassMngr::Next::Application::RosterCustomColumnNameRejection;
using ClassMngr::Next::Application::RosterMaximumColumnNameLength;
using ClassMngr::Next::Application::admitRosterCustomColumnName;
using ClassMngr::Next::Application::normalizeRosterCustomColumnName;

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

bool normalizesWhitespaceAndAutumnAlias()
{
    return normalizeRosterCustomColumnName(
               u" \t aUtUmN\u00a0\u2003 ",
               asciiCaseInsensitiveEquals
               ) == u"Fall"
        && normalizeRosterCustomColumnName(
               u"  Review\t\u00a0 Note  ",
               asciiCaseInsensitiveEquals
               ) == u"Review Note"
        && normalizeRosterCustomColumnName(
               u"Review\ufeffNote",
               asciiCaseInsensitiveEquals
               ) == u"Review\ufeffNote";
}

bool rejectsEmptyBeforeAnyConflicts()
{
    const auto result = admitRosterCustomColumnName(
        u" \t\u00a0\u2003 ",
        {u""},
        {u""},
        asciiCaseInsensitiveEquals
        );
    return !result.accepted()
        && result.normalizedName.empty()
        && result.rejection == RosterCustomColumnNameRejection::Empty;
}

bool rejectsDuplicateBeforeRequiredColumn()
{
    const auto result = admitRosterCustomColumnName(
        u"AUTUMN",
        {u" fall "},
        {u"Fall"},
        asciiCaseInsensitiveEquals
        );
    return !result.accepted()
        && result.normalizedName == u"Fall"
        && result.rejection == RosterCustomColumnNameRejection::Duplicate;
}

bool rejectsRequiredAliasAfterDuplicateCheck()
{
    const auto result = admitRosterCustomColumnName(
        u" autumn ",
        {},
        {u"Fall"},
        asciiCaseInsensitiveEquals
        );
    return !result.accepted()
        && result.normalizedName == u"Fall"
        && result.rejection == RosterCustomColumnNameRejection::RequiredColumn;
}

bool acceptsUniqueNormalizedName()
{
    const auto result = admitRosterCustomColumnName(
        u"  Parent\t\u00a0 Contact  ",
        {u"English", u"Review"},
        {u"English", u"Fall"},
        asciiCaseInsensitiveEquals
        );
    return result.accepted()
        && result.normalizedName == u"Parent Contact";
}

std::u16string repeated(const std::u16string_view value, const std::size_t count)
{
    std::u16string result;
    for (std::size_t index = 0; index < count; ++index)
    {
        result.append(value);
    }
    return result;
}

bool admitsExactlyTheSaveableNormalizedLength()
{
    const std::u16string exactLength(RosterMaximumColumnNameLength, u'x');
    const auto result = admitRosterCustomColumnName(
        u"  " + exactLength + u"  ",
        {},
        {},
        asciiCaseInsensitiveEquals
        );
    return result.accepted()
        && result.normalizedName == exactLength;
}

bool rejectsNamesBeyondTheSaveableNormalizedLength()
{
    const std::u16string tooLong(RosterMaximumColumnNameLength + 1, u'x');
    const auto direct = admitRosterCustomColumnName(
        tooLong,
        {},
        {},
        asciiCaseInsensitiveEquals
        );
    const auto normalized = admitRosterCustomColumnName(
        std::u16string(RosterMaximumColumnNameLength, u'x') + u"\t  x",
        {},
        {},
        asciiCaseInsensitiveEquals
        );
    const auto duplicate = admitRosterCustomColumnName(
        tooLong,
        {tooLong},
        {},
        asciiCaseInsensitiveEquals
        );
    const auto required = admitRosterCustomColumnName(
        tooLong,
        {},
        {tooLong},
        asciiCaseInsensitiveEquals
        );

    return !direct.accepted()
        && direct.normalizedName == tooLong
        && direct.rejection == RosterCustomColumnNameRejection::TooLong
        && !normalized.accepted()
        && normalized.normalizedName
            == std::u16string(RosterMaximumColumnNameLength, u'x') + u" x"
        && normalized.rejection == RosterCustomColumnNameRejection::TooLong
        && duplicate.rejection == RosterCustomColumnNameRejection::Duplicate
        && required.rejection == RosterCustomColumnNameRejection::RequiredColumn;
}

bool countsSupplementaryCharactersAsUtf16CodeUnits()
{
    const std::u16string exactLength = repeated(u"\U0001F600", 32);
    const std::u16string tooLong = exactLength + u"x";
    const auto accepted = admitRosterCustomColumnName(
        exactLength,
        {},
        {},
        asciiCaseInsensitiveEquals
        );
    const auto rejected = admitRosterCustomColumnName(
        tooLong,
        {},
        {},
        asciiCaseInsensitiveEquals
        );
    return exactLength.size() == RosterMaximumColumnNameLength
        && accepted.accepted()
        && rejected.rejection == RosterCustomColumnNameRejection::TooLong;
}

} // namespace

int main()
{
    if (!normalizesWhitespaceAndAutumnAlias())
    {
        std::fprintf(stderr, "Whitespace or Autumn alias normalization failed.\n");
        return EXIT_FAILURE;
    }
    if (!rejectsEmptyBeforeAnyConflicts())
    {
        std::fprintf(stderr, "An empty name did not take precedence.\n");
        return EXIT_FAILURE;
    }
    if (!rejectsDuplicateBeforeRequiredColumn())
    {
        std::fprintf(stderr, "Duplicate rejection did not precede required rejection.\n");
        return EXIT_FAILURE;
    }
    if (!rejectsRequiredAliasAfterDuplicateCheck())
    {
        std::fprintf(stderr, "A required alias was accepted.\n");
        return EXIT_FAILURE;
    }
    if (!acceptsUniqueNormalizedName())
    {
        std::fprintf(stderr, "A unique normalized name was not accepted.\n");
        return EXIT_FAILURE;
    }
    if (!admitsExactlyTheSaveableNormalizedLength())
    {
        std::fprintf(stderr, "A normalized 64-unit name was not admitted.\n");
        return EXIT_FAILURE;
    }
    if (!rejectsNamesBeyondTheSaveableNormalizedLength())
    {
        std::fprintf(stderr, "A normalized overlength name was not rejected correctly.\n");
        return EXIT_FAILURE;
    }
    if (!countsSupplementaryCharactersAsUtf16CodeUnits())
    {
        std::fprintf(stderr, "Supplementary characters did not count as UTF-16 units.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
