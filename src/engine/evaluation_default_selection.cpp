#include "classmngr/engine/evaluation_default_selection.h"

#include <string_view>

namespace classmngr::engine
{
namespace
{
bool isAsciiWhitespace(char value) noexcept
{
    switch (value)
    {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\f':
    case '\v':
        return true;
    default:
        return false;
    }
}

std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (first < value.size() && isAsciiWhitespace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isAsciiWhitespace(value[last - 1]))
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

bool equalsAsciiInsensitive(
    std::string_view left,
    std::string_view right
    ) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        const char leftCharacter = left[index];
        const char rightCharacter = right[index];
        const char normalizedLeft =
            leftCharacter >= 'A' && leftCharacter <= 'Z'
                ? static_cast<char>(leftCharacter - 'A' + 'a')
                : leftCharacter;
        const char normalizedRight =
            rightCharacter >= 'A' && rightCharacter <= 'Z'
                ? static_cast<char>(rightCharacter - 'A' + 'a')
                : rightCharacter;
        if (normalizedLeft != normalizedRight)
        {
            return false;
        }
    }

    return true;
}

AcademicTerm previousTerm(AcademicTerm term) noexcept
{
    switch (term)
    {
    case AcademicTerm::Winter:
        return AcademicTerm::Fall;
    case AcademicTerm::Spring:
        return AcademicTerm::Winter;
    case AcademicTerm::Summer:
        return AcademicTerm::Spring;
    case AcademicTerm::Fall:
        return AcademicTerm::Summer;
    }

    return AcademicTerm::Winter;
}
} // namespace

std::string EvaluationDefaultSelection::evaluationNameForTerm(
    AcademicTerm term
    )
{
    switch (term)
    {
    case AcademicTerm::Winter:
        return "Winter";
    case AcademicTerm::Spring:
        return "Speech Contest";
    case AcademicTerm::Summer:
        return "Summer";
    case AcademicTerm::Fall:
        return "Fall";
    }

    return {};
}

bool EvaluationDefaultSelection::isPopulated(
    const SpeakingEvaluationRows& rows
    )
{
    for (const SpeakingEvaluationRow& row : rows)
    {
        for (const std::string& value : row)
        {
            const std::string trimmed = trimAsciiWhitespace(value);
            if (!trimmed.empty())
            {
                return true;
            }
        }
    }

    return false;
}

AcademicTerm EvaluationDefaultSelection::termForEvaluation(
    AcademicTerm currentTerm,
    bool currentEvaluationIsPopulated
    ) noexcept
{
    return currentEvaluationIsPopulated
        ? currentTerm
        : previousTerm(currentTerm);
}

SchoolLevel EvaluationDefaultSelection::schoolLevelForGrade(
    std::string_view grade
    ) noexcept
{
    const std::string normalized = trimAsciiWhitespace(grade);
    if (
        equalsAsciiInsensitive(normalized, "M1")
        || equalsAsciiInsensitive(normalized, "M2")
        || equalsAsciiInsensitive(normalized, "M3")
        )
    {
        return SchoolLevel::Middle;
    }

    return SchoolLevel::Elementary;
}

} // namespace classmngr::engine
