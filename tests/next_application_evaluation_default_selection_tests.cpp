#include "next/application/evaluation_default_selection.h"
#include "next/application/roster_evaluation_column_policy.h"

#include <array>
#include <cstdlib>
#include <string_view>

using ClassMngr::Next::Application::EvaluationDefaultPolicy;
using ClassMngr::Next::Application::EvaluationPeriod;
using ClassMngr::Next::Application::EvaluationRows;
using ClassMngr::Next::Application::kStoredEvaluationNames;
using ClassMngr::Next::Application::evaluationRowsHaveContent;
using ClassMngr::Next::Application::normalizeStoredEvaluationName;
using ClassMngr::Next::Application::isRosterEvaluationColumnName;
using ClassMngr::Next::Application::selectDefaultEvaluationPeriod;
using ClassMngr::Next::Application::storedEvaluationName;

namespace
{

struct CycleCase
{
    EvaluationPeriod current;
    EvaluationPeriod previous;
};

constexpr std::array<CycleCase, 4> CycleCases{{
    {EvaluationPeriod::Winter, EvaluationPeriod::Fall},
    {EvaluationPeriod::Spring, EvaluationPeriod::Winter},
    {EvaluationPeriod::Summer, EvaluationPeriod::Spring},
    {EvaluationPeriod::Fall, EvaluationPeriod::Summer}
}};

constexpr std::array<EvaluationPeriod, 4> OrderedPeriods{{
    EvaluationPeriod::Winter,
    EvaluationPeriod::Spring,
    EvaluationPeriod::Summer,
    EvaluationPeriod::Fall
}};

constexpr char16_t asciiLowercase(const char16_t value) noexcept
{
    return value >= u'A' && value <= u'Z'
        ? static_cast<char16_t>(value + (u'a' - u'A'))
        : value;
}

constexpr bool asciiCaseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    ) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (asciiLowercase(left[index]) != asciiLowercase(right[index]))
        {
            return false;
        }
    }

    return true;
}

} // namespace

int main()
{
    constexpr std::array<std::u16string_view, 4> ExpectedStoredNames{{
        u"Winter",
        u"Speech Contest",
        u"Summer",
        u"Fall"
    }};
    if (kStoredEvaluationNames != ExpectedStoredNames)
    {
        return EXIT_FAILURE;
    }

    for (std::size_t index = 0; index < OrderedPeriods.size(); ++index)
    {
        const std::u16string_view expectedName = ExpectedStoredNames[index];
        if (storedEvaluationName(OrderedPeriods[index]) != expectedName
            || normalizeStoredEvaluationName(expectedName) != expectedName)
        {
            return EXIT_FAILURE;
        }
    }

    constexpr auto InvalidPeriod = static_cast<EvaluationPeriod>(255);
    if (!storedEvaluationName(InvalidPeriod).empty()
        || normalizeStoredEvaluationName(u"") != u"Winter"
        || normalizeStoredEvaluationName(u"Unrecognized") != u"Winter")
    {
        return EXIT_FAILURE;
    }

    constexpr std::array<std::u16string_view, 4> LowercaseStoredNames{{
        u"winter",
        u"speech contest",
        u"summer",
        u"fall"
    }};
    for (std::size_t index = 0; index < ExpectedStoredNames.size(); ++index)
    {
        if (!isRosterEvaluationColumnName(
                ExpectedStoredNames[index],
                asciiCaseInsensitiveEquals
                )
            || !isRosterEvaluationColumnName(
                   LowercaseStoredNames[index],
                   asciiCaseInsensitiveEquals
                   ))
        {
            return EXIT_FAILURE;
        }
    }

    if (!isRosterEvaluationColumnName(u"Autumn", asciiCaseInsensitiveEquals)
        || !isRosterEvaluationColumnName(u"aUtUmN", asciiCaseInsensitiveEquals)
        || isRosterEvaluationColumnName(u"Midterm", asciiCaseInsensitiveEquals)
        || isRosterEvaluationColumnName(u" Winter", asciiCaseInsensitiveEquals)
        || isRosterEvaluationColumnName(u"Fall ", asciiCaseInsensitiveEquals)
        || isRosterEvaluationColumnName(u" Autumn ", asciiCaseInsensitiveEquals))
    {
        return EXIT_FAILURE;
    }

    if (evaluationRowsHaveContent({}))
    {
        return EXIT_FAILURE;
    }

    const EvaluationRows emptyRows{
        {},
        {u"", u""},
        {u" \t\n\v\f\r", u"\u00A0\u1680\u2000\u2001\u2002"
         u"\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A"
         u"\u0085\u2028\u2029\u202F\u205F\u3000"}
    };
    if (evaluationRowsHaveContent(emptyRows))
    {
        return EXIT_FAILURE;
    }

    const EvaluationRows populatedRows{{u"\u3000\uAE40\uBBFC\uC9C0\u00A0"}};
    if (!evaluationRowsHaveContent(populatedRows))
    {
        return EXIT_FAILURE;
    }

    if (selectDefaultEvaluationPeriod(
            EvaluationDefaultPolicy::CurrentOrPreviousTerm,
            EvaluationPeriod::Fall,
            emptyRows
            ) != EvaluationPeriod::Summer
        || selectDefaultEvaluationPeriod(
               EvaluationDefaultPolicy::CurrentOrPreviousTerm,
               EvaluationPeriod::Fall,
               populatedRows
               ) != EvaluationPeriod::Fall
        || selectDefaultEvaluationPeriod(
               EvaluationDefaultPolicy::All,
               EvaluationPeriod::Fall,
               populatedRows
               ).has_value())
    {
        return EXIT_FAILURE;
    }

    for (const CycleCase& cycleCase : CycleCases)
    {
        if (selectDefaultEvaluationPeriod(
                EvaluationDefaultPolicy::CurrentOrPreviousTerm,
                cycleCase.current,
                true
                ) != cycleCase.current)
        {
            return EXIT_FAILURE;
        }

        if (selectDefaultEvaluationPeriod(
                EvaluationDefaultPolicy::CurrentOrPreviousTerm,
                cycleCase.current,
                false
                ) != cycleCase.previous)
        {
            return EXIT_FAILURE;
        }

        if (selectDefaultEvaluationPeriod(
                EvaluationDefaultPolicy::All,
                cycleCase.current,
                true
                ).has_value()
            || selectDefaultEvaluationPeriod(
                   EvaluationDefaultPolicy::All,
                   cycleCase.current,
                   false
                   ).has_value())
        {
            return EXIT_FAILURE;
        }
    }

    if (selectDefaultEvaluationPeriod(
            EvaluationDefaultPolicy::CurrentOrPreviousTerm,
            InvalidPeriod,
            true
            ).has_value()
        || selectDefaultEvaluationPeriod(
               EvaluationDefaultPolicy::CurrentOrPreviousTerm,
               InvalidPeriod,
               false
               ).has_value())
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
