#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

enum class SpeakingAnalyticsColumn
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

enum class SpeakingAnalyticsCriterion
{
    Grammar,
    Pronunciation,
    Fluency,
    Manner,
    Content,
    OverallEffort,
    Count
};

using SpeakingAnalyticsRow = std::vector<std::string>;
using SpeakingAnalyticsRows = std::vector<SpeakingAnalyticsRow>;

struct SpeakingAnalyticsRoster
{
    std::vector<std::string> columns;
    std::vector<SpeakingAnalyticsRow> rows;
};

struct SpeakingAnalyticsCriterionSlice
{
    int order = 0;
    std::string name;
    int students = 0;
    std::map<std::string, int> distribution;
    double average3 = 0.0;
    bool hasData = false;
};

struct SpeakingAnalyticsStudentRank
{
    std::string englishName;
    std::string koreanName;
    double overall3 = 0.0;
    std::string overallLetter;
    std::vector<std::string> criterionLetters;
    bool fullyScored = false;
};

struct SpeakingAnalyticsSnapshot
{
    bool hasData = false;
    double classAverage3 = 0.0;
    std::string classAverageLetter;
    int rosterStudentCount = 0;
    int fullyScoredCount = 0;
    std::vector<SpeakingAnalyticsCriterionSlice> criteria;
    std::vector<std::string> strongestNames;
    std::vector<std::string> focusNames;
    std::vector<std::string> strongestLabels;
    std::vector<std::string> focusLabels;
    std::vector<std::string> overallLetters;
    std::vector<SpeakingAnalyticsStudentRank> rankings;
};

struct SpeakingAnalyticsYearToDatePoint
{
    std::string evaluationName;
    double classAverage3 = 0.0;
    std::string classAverageLetter;
};

class SpeakingAnalyticsService final
{
public:
    [[nodiscard]] static std::vector<std::string> evaluationNames();

    [[nodiscard]] static double roundTo3(double value);

    [[nodiscard]] static std::string formatAverage(double average);

    [[nodiscard]] static int gradeToNumber(std::string_view grade);

    [[nodiscard]] static std::string numberToGrade(int number);

    [[nodiscard]] static int roundAverageToGrade(double average);

    [[nodiscard]] static std::vector<int> strongestIndices(
        const std::vector<double>& averages3
        );

    [[nodiscard]] static std::vector<int> focusIndices(
        const std::vector<double>& averages3
        );

    [[nodiscard]] static SpeakingAnalyticsRows filterMatrixByRoster(
        const SpeakingAnalyticsRows& matrix,
        const SpeakingAnalyticsRoster& roster
        );

    [[nodiscard]] static SpeakingAnalyticsSnapshot compute(
        const std::vector<SpeakingAnalyticsRows>& matrices,
        int rosterStudentCount
        );

    [[nodiscard]] static std::optional<SpeakingAnalyticsYearToDatePoint>
        yearToDatePoint(
            std::string evaluationName,
            const SpeakingAnalyticsSnapshot& snapshot
            );
};

} // namespace classmngr::engine
