#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

inline constexpr std::array<std::string_view, 4> SpeakingEvaluationNames{
    "Winter",
    "Speech Contest",
    "Summer",
    "Fall"
};

inline constexpr std::array<std::string_view, 5>
    SpeakingEvaluationScoreValues{
        "A+",
        "A",
        "B+",
        "B",
        "C"
    };

enum class SpeakingEvaluationColumn
{
    Index = 0,
    EnglishName = 1,
    KoreanName = 2,
    Grammar = 3,
    Pronunciation = 4,
    Fluency = 5,
    Manner = 6,
    Content = 7,
    OverallEffort = 8,
    Comments = 9,
    Notes = 10
};

inline constexpr int SpeakingEvaluationRowCount = 25;
inline constexpr int SpeakingEvaluationColumnCount = 11;
inline constexpr int SpeakingEvaluationCommentMinLength = 100;
inline constexpr int SpeakingEvaluationCommentMaxLength = 450;
inline constexpr std::size_t
    SpeakingEvaluationMaximumEvaluationNameLength = 128;
inline constexpr std::size_t SpeakingEvaluationMaximumNotesLength = 10000;

[[nodiscard]] constexpr int toInt(
    SpeakingEvaluationColumn column
    ) noexcept
{
    return static_cast<int>(column);
}

struct SpeakingEvaluationCellChange
{
    int row = -1;
    int column = -1;
};

using SpeakingEvaluationRow = std::vector<std::string>;
using SpeakingEvaluationRows = std::vector<SpeakingEvaluationRow>;

struct SpeakingEvaluationScore
{
    std::string englishName;
    std::string koreanName;
    std::string finalGrade;
};

} // namespace classmngr::engine
