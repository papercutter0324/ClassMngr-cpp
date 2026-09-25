#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ClassMngr::Next::Domain
{
enum class SpeakingEvaluationCriterion : std::size_t
{
    Grammar,
    Pronunciation,
    Fluency,
    Manner,
    Content,
    OverallEffort,
    Count
};

inline constexpr std::size_t SpeakingEvaluationCriterionCount =
    static_cast<std::size_t>(SpeakingEvaluationCriterion::Count);

enum class SpeakingEvaluationGrade : std::uint8_t
{
    C = 1,
    B = 2,
    BPlus = 3,
    A = 4,
    APlus = 5
};

using SpeakingEvaluationComponentScores = std::array<
    std::optional<SpeakingEvaluationGrade>,
    SpeakingEvaluationCriterionCount
    >;

[[nodiscard]] constexpr std::optional<SpeakingEvaluationGrade>
speakingEvaluationGradeFromLabel(std::string_view label)
{
    if (label == "C")
    {
        return SpeakingEvaluationGrade::C;
    }
    if (label == "B")
    {
        return SpeakingEvaluationGrade::B;
    }
    if (label == "B+")
    {
        return SpeakingEvaluationGrade::BPlus;
    }
    if (label == "A")
    {
        return SpeakingEvaluationGrade::A;
    }
    if (label == "A+")
    {
        return SpeakingEvaluationGrade::APlus;
    }
    return std::nullopt;
}

[[nodiscard]] constexpr std::string_view speakingEvaluationGradeLabel(
    SpeakingEvaluationGrade grade
    )
{
    switch (grade)
    {
    case SpeakingEvaluationGrade::C:
        return "C";
    case SpeakingEvaluationGrade::B:
        return "B";
    case SpeakingEvaluationGrade::BPlus:
        return "B+";
    case SpeakingEvaluationGrade::A:
        return "A";
    case SpeakingEvaluationGrade::APlus:
        return "A+";
    }
    return {};
}

[[nodiscard]] constexpr std::optional<SpeakingEvaluationGrade>
speakingEvaluationGradeFromValue(int value)
{
    switch (value)
    {
    case 1:
        return SpeakingEvaluationGrade::C;
    case 2:
        return SpeakingEvaluationGrade::B;
    case 3:
        return SpeakingEvaluationGrade::BPlus;
    case 4:
        return SpeakingEvaluationGrade::A;
    case 5:
        return SpeakingEvaluationGrade::APlus;
    }
    return std::nullopt;
}

[[nodiscard]] constexpr std::optional<SpeakingEvaluationGrade>
calculateOverallSpeakingEvaluationGrade(
    const SpeakingEvaluationComponentScores& scores
    )
{
    int sum = 0;
    for (const auto score : scores)
    {
        if (!score)
        {
            return std::nullopt;
        }

        const int value = static_cast<int>(*score);
        if (!speakingEvaluationGradeFromValue(value))
        {
            return std::nullopt;
        }
        sum += value;
    }

    const double average =
        static_cast<double>(sum) / SpeakingEvaluationCriterionCount;
    int rounded = static_cast<int>(average);
    if (average - rounded >= 0.4)
    {
        ++rounded;
    }

    rounded = std::clamp(rounded, 1, 5);
    return speakingEvaluationGradeFromValue(rounded);
}
}
