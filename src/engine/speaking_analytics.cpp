#include "classmngr/engine/speaking_analytics.h"

#include "classmngr/engine/speaking_evaluation.h"
#include "classmngr/engine/student_name.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <string_view>
#include <utility>

namespace classmngr::engine
{
namespace
{
constexpr std::array<SpeakingAnalyticsColumn, 6> kCriterionColumns{
    SpeakingAnalyticsColumn::Grammar,
    SpeakingAnalyticsColumn::Pronunciation,
    SpeakingAnalyticsColumn::Fluency,
    SpeakingAnalyticsColumn::Manner,
    SpeakingAnalyticsColumn::Content,
    SpeakingAnalyticsColumn::OverallEffort
};

constexpr std::array<std::string_view, 6> kCriterionNames{
    "Grammar",
    "Pronunciation",
    "Fluency",
    "Manner",
    "Content",
    "Overall Effort"
};

constexpr std::size_t criterionCount =
    static_cast<std::size_t>(SpeakingAnalyticsCriterion::Count);

std::string trimAsciiWhitespace(std::string_view value)
{
    const auto isWhitespace = [](char character)
    {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    };

    std::size_t first = 0;
    while (first < value.size() && isWhitespace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isWhitespace(value[last - 1]))
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

const std::string& valueAt(
    const SpeakingAnalyticsRow& row,
    int column
    )
{
    static const std::string empty;
    if (column < 0 || static_cast<std::size_t>(column) >= row.size())
    {
        return empty;
    }
    return row[static_cast<std::size_t>(column)];
}

std::string asciiCaseFold(std::string_view value)
{
    std::string result(value);
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        }
        );
    return result;
}

std::string studentKey(
    std::string_view englishName,
    std::string_view koreanName
    )
{
    const std::string normalizedEnglish =
        StudentNameService::normalizeEnglish(englishName);
    if (!normalizedEnglish.empty())
    {
        return "en:" + asciiCaseFold(normalizedEnglish);
    }

    const std::string normalizedKorean =
        trimAsciiWhitespace(StudentNameService::baseKorean(koreanName));
    if (normalizedKorean.empty())
    {
        return {};
    }

    return "ko:" + normalizedKorean;
}

std::string criterionLabel(
    const SpeakingAnalyticsCriterionSlice& slice
    )
{
    if (!slice.hasData)
    {
        return slice.name;
    }

    return slice.name
        + " ("
        + SpeakingAnalyticsService::numberToGrade(
            SpeakingAnalyticsService::roundAverageToGrade(slice.average3)
            )
        + ")";
}

bool namesSortBefore(
    const SpeakingAnalyticsStudentRank& left,
    const SpeakingAnalyticsStudentRank& right
    )
{
    const std::string leftNormalized =
        asciiCaseFold(StudentNameService::normalizeEnglish(left.englishName));
    const std::string rightNormalized =
        asciiCaseFold(StudentNameService::normalizeEnglish(right.englishName));
    if (leftNormalized != rightNormalized)
    {
        return leftNormalized < rightNormalized;
    }
    return left.englishName < right.englishName;
}
} // namespace

std::vector<std::string> SpeakingAnalyticsService::evaluationNames()
{
    std::vector<std::string> names;
    names.reserve(SpeakingEvaluationNames.size());
    for (const std::string_view name : SpeakingEvaluationNames)
    {
        names.emplace_back(name);
    }
    return names;
}

double SpeakingAnalyticsService::roundTo3(double value)
{
    return std::round(value * 1000.0) / 1000.0;
}

std::string SpeakingAnalyticsService::formatAverage(double average)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(1) << roundTo3(average);
    return output.str();
}

int SpeakingAnalyticsService::gradeToNumber(std::string_view grade)
{
    for (std::size_t index = 0; index < SpeakingEvaluationScoreValues.size(); ++index)
    {
        if (grade == SpeakingEvaluationScoreValues[index])
        {
            return static_cast<int>(
                SpeakingEvaluationScoreValues.size() - index
                );
        }
    }
    return 0;
}

std::string SpeakingAnalyticsService::numberToGrade(int number)
{
    number = std::clamp(
        number,
        1,
        static_cast<int>(SpeakingEvaluationScoreValues.size())
        );
    return std::string(
        SpeakingEvaluationScoreValues[
            SpeakingEvaluationScoreValues.size() - static_cast<std::size_t>(number)
            ]
        );
}

int SpeakingAnalyticsService::roundAverageToGrade(double average)
{
    if (average <= 0.0)
    {
        return 0;
    }
    if (average >= 5.0)
    {
        return 5;
    }

    const int whole = static_cast<int>(std::floor(average));
    return whole + (average - whole >= 0.4 ? 1 : 0);
}

std::vector<int> SpeakingAnalyticsService::strongestIndices(
    const std::vector<double>& averages3
    )
{
    if (averages3.empty())
    {
        return {};
    }

    const double best = *std::max_element(
        averages3.cbegin(),
        averages3.cend()
        );
    std::vector<int> result;
    for (std::size_t index = 0; index < averages3.size(); ++index)
    {
        if (std::abs(averages3[index] - best) < 1e-9)
        {
            result.push_back(static_cast<int>(index));
        }
    }
    return result;
}

std::vector<int> SpeakingAnalyticsService::focusIndices(
    const std::vector<double>& averages3
    )
{
    if (averages3.empty())
    {
        return {};
    }

    const double worst = *std::min_element(
        averages3.cbegin(),
        averages3.cend()
        );
    std::vector<int> result;
    for (std::size_t index = 0; index < averages3.size(); ++index)
    {
        if (std::abs(averages3[index] - worst) < 1e-9)
        {
            result.push_back(static_cast<int>(index));
        }
    }
    return result;
}

SpeakingAnalyticsSnapshot SpeakingAnalyticsService::compute(
    const std::vector<SpeakingAnalyticsRows>& matrices,
    int rosterStudentCount
    )
{
    struct StudentAccumulator
    {
        std::string englishName;
        std::string koreanName;
        std::array<double, criterionCount> sums{};
        std::array<int, criterionCount> counts{};
    };

    std::map<std::string, std::size_t> accumulatorByStudent;
    std::vector<StudentAccumulator> students;

    for (const SpeakingAnalyticsRows& matrix : matrices)
    {
        for (const SpeakingAnalyticsRow& row : matrix)
        {
            const std::string englishName = trimAsciiWhitespace(valueAt(
                row,
                static_cast<int>(SpeakingAnalyticsColumn::EnglishName)
                ));
            const std::string koreanName = trimAsciiWhitespace(valueAt(
                row,
                static_cast<int>(SpeakingAnalyticsColumn::KoreanName)
                ));
            const std::string key = studentKey(englishName, koreanName);
            if (key.empty())
            {
                continue;
            }

            const auto [iterator, inserted] = accumulatorByStudent.emplace(
                key,
                students.size()
                );
            if (inserted)
            {
                students.push_back({englishName, koreanName, {}, {}});
            }

            StudentAccumulator& student = students[iterator->second];
            if (student.englishName.empty())
            {
                student.englishName = englishName;
            }
            if (student.koreanName.empty())
            {
                student.koreanName = koreanName;
            }

            for (std::size_t criterion = 0;
                 criterion < criterionCount;
                 ++criterion)
            {
                const std::string grade = trimAsciiWhitespace(valueAt(
                    row,
                    static_cast<int>(kCriterionColumns[criterion])
                    ));
                const int value = gradeToNumber(grade);
                if (value <= 0)
                {
                    continue;
                }

                student.sums[criterion] += value;
                ++student.counts[criterion];
            }
        }
    }

    SpeakingAnalyticsSnapshot snapshot;
    snapshot.rosterStudentCount = std::max(0, rosterStudentCount);

    std::array<double, criterionCount> criterionSums{};
    std::array<int, criterionCount> criterionCounts{};
    std::array<int, criterionCount> criterionStudentCounts{};
    std::array<std::map<std::string, int>, criterionCount> distributions;
    std::vector<double> criterionAverages;
    criterionAverages.reserve(criterionCount);

    double classSum = 0.0;
    int classCount = 0;

    for (const StudentAccumulator& student : students)
    {
        double scoreSum = 0.0;
        int scoreCount = 0;
        bool fullyScored = true;
        std::vector<std::string> letters;
        letters.reserve(criterionCount);

        for (std::size_t criterion = 0;
             criterion < criterionCount;
             ++criterion)
        {
            if (student.counts[criterion] == 0)
            {
                fullyScored = false;
                letters.emplace_back();
                continue;
            }

            const double average = roundTo3(
                student.sums[criterion] / student.counts[criterion]
                );
            const std::string letter = numberToGrade(
                roundAverageToGrade(average)
                );
            letters.push_back(letter);

            criterionSums[criterion] += student.sums[criterion];
            criterionCounts[criterion] += student.counts[criterion];
            ++criterionStudentCounts[criterion];
            ++distributions[criterion][letter];
            scoreSum += student.sums[criterion];
            scoreCount += student.counts[criterion];
        }

        if (scoreCount == 0)
        {
            continue;
        }

        SpeakingAnalyticsStudentRank rank;
        rank.englishName = student.englishName;
        rank.koreanName = student.koreanName;
        rank.overall3 = roundTo3(scoreSum / scoreCount);
        rank.overallLetter = numberToGrade(
            roundAverageToGrade(rank.overall3)
            );
        rank.criterionLetters = std::move(letters);
        rank.fullyScored = fullyScored;
        snapshot.rankings.push_back(rank);
        snapshot.overallLetters.push_back(rank.overallLetter);

        classSum += rank.overall3;
        ++classCount;
        if (fullyScored)
        {
            ++snapshot.fullyScoredCount;
        }
    }

    snapshot.hasData = classCount > 0;
    if (!snapshot.hasData)
    {
        return snapshot;
    }

    snapshot.classAverage3 = roundTo3(classSum / classCount);
    snapshot.classAverageLetter = numberToGrade(
        roundAverageToGrade(snapshot.classAverage3)
        );

    for (std::size_t criterion = 0;
         criterion < criterionCount;
         ++criterion)
    {
        SpeakingAnalyticsCriterionSlice slice;
        slice.order = static_cast<int>(criterion);
        slice.name = std::string(kCriterionNames[criterion]);
        slice.students = criterionStudentCounts[criterion];
        slice.distribution = std::move(distributions[criterion]);
        slice.hasData = criterionCounts[criterion] > 0;
        if (slice.hasData)
        {
            slice.average3 = roundTo3(
                criterionSums[criterion] / criterionCounts[criterion]
                );
            criterionAverages.push_back(slice.average3);
        }
        else
        {
            criterionAverages.push_back(-1.0);
        }
        snapshot.criteria.push_back(std::move(slice));
    }

    double strongest = -1.0;
    double focus = 6.0;
    for (const double average : criterionAverages)
    {
        if (average < 0.0)
        {
            continue;
        }
        strongest = std::max(strongest, average);
        focus = std::min(focus, average);
    }
    for (const SpeakingAnalyticsCriterionSlice& slice : snapshot.criteria)
    {
        if (!slice.hasData)
        {
            continue;
        }
        if (std::abs(slice.average3 - strongest) < 1e-9)
        {
            snapshot.strongestNames.push_back(slice.name);
            snapshot.strongestLabels.push_back(criterionLabel(slice));
        }
        if (std::abs(slice.average3 - focus) < 1e-9)
        {
            snapshot.focusNames.push_back(slice.name);
            snapshot.focusLabels.push_back(criterionLabel(slice));
        }
    }

    std::sort(
        snapshot.rankings.begin(),
        snapshot.rankings.end(),
        [](const SpeakingAnalyticsStudentRank& left,
           const SpeakingAnalyticsStudentRank& right)
        {
            if (std::abs(left.overall3 - right.overall3) >= 1e-9)
            {
                return left.overall3 > right.overall3;
            }
            return namesSortBefore(left, right);
        }
        );

    return snapshot;
}

std::optional<SpeakingAnalyticsYearToDatePoint>
SpeakingAnalyticsService::yearToDatePoint(
    std::string evaluationName,
    const SpeakingAnalyticsSnapshot& snapshot
    )
{
    if (snapshot.fullyScoredCount <= 0)
    {
        return std::nullopt;
    }

    double sum = 0.0;
    int count = 0;
    for (const SpeakingAnalyticsStudentRank& rank : snapshot.rankings)
    {
        if (!rank.fullyScored)
        {
            continue;
        }

        sum += rank.overall3;
        ++count;
    }

    if (count == 0)
    {
        return std::nullopt;
    }

    SpeakingAnalyticsYearToDatePoint point;
    point.evaluationName = std::move(evaluationName);
    point.classAverage3 = roundTo3(sum / count);
    point.classAverageLetter = numberToGrade(
        roundAverageToGrade(point.classAverage3)
        );
    return point;
}

SpeakingAnalyticsRows SpeakingAnalyticsService::filterMatrixByRoster(
    const SpeakingAnalyticsRows& matrix,
    const SpeakingAnalyticsRoster& roster
    )
{
    const auto columnIndex = [&roster](std::string_view name)
    {
        const auto iterator = std::find(
            roster.columns.cbegin(),
            roster.columns.cend(),
            name
            );
        return iterator == roster.columns.cend()
            ? -1
            : static_cast<int>(std::distance(roster.columns.cbegin(), iterator));
    };

    const int englishColumn = columnIndex("English");
    const int koreanColumn = columnIndex("Korean");
    if (englishColumn < 0 && koreanColumn < 0)
    {
        return matrix;
    }

    const auto namesInColumn = [&roster](int column)
    {
        std::vector<std::string> names;
        if (column < 0)
        {
            return names;
        }

        for (const SpeakingAnalyticsRow& row : roster.rows)
        {
            const std::string name = trimAsciiWhitespace(valueAt(row, column));
            if (!name.empty())
            {
                names.push_back(name);
            }
        }
        return names;
    };

    const std::vector<std::string> englishNames = namesInColumn(englishColumn);
    const std::vector<std::string> koreanNames = namesInColumn(koreanColumn);
    if (englishNames.empty() && koreanNames.empty())
    {
        return matrix;
    }

    const auto matchesEnglish = [&englishNames](std::string_view name)
    {
        const std::string normalized = asciiCaseFold(
            StudentNameService::normalizeEnglish(name)
            );
        return !normalized.empty()
            && std::ranges::any_of(
                englishNames,
                [&normalized](const std::string& rosterName)
                {
                    return asciiCaseFold(
                        StudentNameService::normalizeEnglish(rosterName)
                        ) == normalized;
                }
                );
    };
    const auto matchesKorean = [&koreanNames](std::string_view name)
    {
        const std::string normalized = StudentNameService::baseKorean(name);
        return !normalized.empty()
            && std::ranges::any_of(
                koreanNames,
                [&normalized](const std::string& rosterName)
                {
                    return StudentNameService::baseKorean(rosterName)
                        == normalized;
                }
                );
    };

    SpeakingAnalyticsRows filtered;
    filtered.reserve(matrix.size());
    for (const SpeakingAnalyticsRow& row : matrix)
    {
        if (matchesEnglish(valueAt(
                row,
                static_cast<int>(SpeakingAnalyticsColumn::EnglishName)
                ))
            || matchesKorean(valueAt(
                row,
                static_cast<int>(SpeakingAnalyticsColumn::KoreanName)
                )))
        {
            filtered.push_back(row);
        }
    }
    return filtered;
}

SpeakingAnalyticsDashboard SpeakingAnalyticsService::buildDashboard(
    const SpeakingAnalyticsDashboardInput& input
    )
{
    const std::string selection = trimAsciiWhitespace(input.selection);
    const bool allEvaluations = selection.empty()
        || asciiCaseFold(selection) == "all";
    const int rosterCount = rosterStudentCount(input.roster);

    struct EvaluationView
    {
        const SpeakingAnalyticsEvaluation* evaluation = nullptr;
        SpeakingAnalyticsSnapshot filteredSnapshot;
        SpeakingAnalyticsSnapshot historicalSnapshot;
    };

    std::vector<EvaluationView> evaluations;
    evaluations.reserve(input.evaluations.size());
    std::vector<SpeakingAnalyticsRows> filteredMatrices;
    filteredMatrices.reserve(input.evaluations.size());

    for (const SpeakingAnalyticsEvaluation& evaluation : input.evaluations)
    {
        const SpeakingAnalyticsRows filtered = input.roster.rows.empty()
            ? evaluation.rows
            : filterMatrixByRoster(evaluation.rows, input.roster);

        EvaluationView view;
        view.evaluation = &evaluation;
        view.filteredSnapshot = compute({filtered}, rosterCount);
        view.historicalSnapshot = compute({evaluation.rows}, rosterCount);
        evaluations.push_back(std::move(view));

        if (!evaluation.rows.empty())
        {
            filteredMatrices.push_back(filtered);
        }
    }

    SpeakingAnalyticsDashboard dashboard;
    if (allEvaluations)
    {
        dashboard.selectedSnapshot = compute(filteredMatrices, rosterCount);
    }
    else
    {
        for (const EvaluationView& view : evaluations)
        {
            if (view.evaluation->name != selection)
            {
                continue;
            }

            dashboard.selectedSnapshot = view.filteredSnapshot;
            dashboard.classShapeEvaluationName = view.evaluation->name;
            dashboard.classShapeSnapshot = view.filteredSnapshot;
            break;
        }
    }

    if (allEvaluations)
    {
        for (auto iterator = evaluations.crbegin();
             iterator != evaluations.crend();
             ++iterator)
        {
            if (iterator->filteredSnapshot.fullyScoredCount <= 0)
            {
                continue;
            }

            dashboard.classShapeEvaluationName =
                iterator->evaluation->name;
            dashboard.classShapeSnapshot = iterator->filteredSnapshot;
            break;
        }
    }

    dashboard.yearToDatePoints.reserve(evaluations.size());
    for (const EvaluationView& view : evaluations)
    {
        if (const auto point = yearToDatePoint(
                view.evaluation->name,
                view.historicalSnapshot
                ))
        {
            dashboard.yearToDatePoints.push_back(*point);
        }
    }

    return dashboard;
}

} // namespace classmngr::engine
