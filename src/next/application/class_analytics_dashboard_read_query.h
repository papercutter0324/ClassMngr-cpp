#pragma once

#include "next/application/class_analytics_dashboard_read_port.h"
#include "next/domain/speaking_evaluation_grade.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassAnalyticsDashboardReadRequest final
{
    Domain::ClassId classId;
    std::u16string evaluationSelection;
};

enum class ClassAnalyticsCriterion : std::size_t
{
    Grammar,
    Pronunciation,
    Fluency,
    Manner,
    Content,
    OverallEffort,
    Count
};

inline constexpr std::size_t kClassAnalyticsCriterionCount =
    static_cast<std::size_t>(ClassAnalyticsCriterion::Count);

inline constexpr std::array<std::u16string_view,
                             kClassAnalyticsCriterionCount>
    kClassAnalyticsCriterionNames{
        u"Grammar", u"Pronunciation", u"Fluency", u"Manner",
        u"Content", u"Overall Effort"
    };

struct ClassAnalyticsCriterionSlice final
{
    std::u16string name;
    int students = 0;
    std::array<int, 5> distribution{};
    double average3 = 0.0;
    bool hasData = false;
};

struct ClassAnalyticsStudentRank final
{
    std::u16string englishName;
    std::u16string koreanName;
    double overall3 = 0.0;
    std::u16string overallLetter;
    std::array<std::u16string, kClassAnalyticsCriterionCount>
        criterionLetters;
    bool fullyScored = false;
};

struct ClassAnalyticsSnapshot final
{
    bool hasData = false;
    double classAverage3 = 0.0;
    std::u16string classAverageLetter;
    std::size_t rosterStudentCount = 0;
    std::size_t fullyScoredCount = 0;
    std::vector<ClassAnalyticsCriterionSlice> criteria;
    std::vector<std::u16string> strongestNames;
    std::vector<std::u16string> focusNames;
    std::vector<std::u16string> strongestLabels;
    std::vector<std::u16string> focusLabels;
    std::vector<std::u16string> overallLetters;
    std::vector<ClassAnalyticsStudentRank> rankings;
};

struct ClassAnalyticsYearToDatePoint final
{
    std::u16string evaluationName;
    double classAverage3 = 0.0;
    std::u16string classAverageLetter;
};

struct ClassAnalyticsDashboard final
{
    ClassAnalyticsSnapshot selectedSnapshot;
    std::u16string classShapeEvaluationName;
    ClassAnalyticsSnapshot classShapeSnapshot;
    std::vector<ClassAnalyticsYearToDatePoint> yearToDatePoints;
};

using ClassAnalyticsDashboardResult = Domain::Result<ClassAnalyticsDashboard>;

namespace ClassAnalyticsDashboardReadDetail
{

[[nodiscard]] inline bool isCanonicalPositiveClassId(
    const std::string& value
    ) noexcept
{
    if (value.empty() || value.front() < '1' || value.front() > '9')
    {
        return false;
    }

    std::uint64_t parsed = 0;
    for (const char character : value)
    {
        if (character < '0' || character > '9')
        {
            return false;
        }
        const std::uint64_t digit =
            static_cast<std::uint64_t>(character - '0');
        if (parsed > (static_cast<std::uint64_t>(
                          std::numeric_limits<int>::max()) - digit) / 10)
        {
            return false;
        }
        parsed = parsed * 10 + digit;
    }
    return parsed > 0;
}

[[nodiscard]] inline Domain::OperationError invalidClassId()
{
    return {
        .code = Domain::ErrorCode::InvalidInput,
        .message = "Class ID must be a canonical positive integer.",
        .recoverable = false
    };
}

[[nodiscard]] inline std::u16string prependKey(
    const std::u16string_view prefix,
    const std::u16string_view value
    )
{
    std::u16string key(prefix);
    key.append(value);
    return key;
}

[[nodiscard]] inline int gradeValue(const std::u16string_view grade) noexcept
{
    if (grade == u"C") return 1;
    if (grade == u"B") return 2;
    if (grade == u"B+") return 3;
    if (grade == u"A") return 4;
    if (grade == u"A+") return 5;
    return 0;
}

[[nodiscard]] inline std::u16string_view gradeLabel(
    const int value
    ) noexcept
{
    switch (value)
    {
    case 5: return u"A+";
    case 4: return u"A";
    case 3: return u"B+";
    case 2: return u"B";
    default: return u"C";
    }
}

[[nodiscard]] inline double roundTo3(const double value) noexcept
{
    return std::floor(value * 1000.0 + 0.5) / 1000.0;
}

[[nodiscard]] inline int roundAverageToGrade(const double average) noexcept
{
    if (average <= 0.0) return 0;
    if (average >= 5.0) return 5;
    const int whole = static_cast<int>(std::floor(average));
    return whole + (average - static_cast<double>(whole) >= 0.4 ? 1 : 0);
}

[[nodiscard]] inline std::u16string numberToGrade(const int value)
{
    const std::u16string_view label = gradeLabel(std::clamp(value, 1, 5));
    return std::u16string(label);
}

[[nodiscard]] inline std::u16string studentKey(
    const ClassAnalyticsEvaluationRow& row,
    const ClassAnalyticsNameSemanticsPort& names
    )
{
    const std::u16string english =
        names.normalizedEnglishIdentity(row.englishName);
    if (!english.empty())
    {
        return prependKey(u"en:", english);
    }

    const std::u16string korean = names.trimmed(
        names.baseKoreanName(row.koreanName)
        );
    return korean.empty() ? std::u16string{} : prependKey(u"ko:", korean);
}

[[nodiscard]] inline std::u16string criterionLabel(
    const ClassAnalyticsCriterionSlice& criterion
    )
{
    if (!criterion.hasData)
    {
        return criterion.name;
    }

    std::u16string label = criterion.name;
    label += u" (";
    label += numberToGrade(roundAverageToGrade(criterion.average3));
    label += u")";
    return label;
}

[[nodiscard]] inline std::vector<ClassAnalyticsEvaluationRow> filterByRoster(
    const ClassAnalyticsEvaluationRows& matrix,
    const ClassAnalyticsRosterNames& roster,
    const ClassAnalyticsNameSemanticsPort& names
    )
{
    if (roster.rowCount == 0
        || (!roster.hasEnglishColumn && !roster.hasKoreanColumn))
    {
        return matrix;
    }

    std::vector<std::u16string> englishNames;
    if (roster.hasEnglishColumn)
    {
        for (const std::u16string& rawName : roster.englishNames)
        {
            const std::u16string name = names.trimmed(rawName);
            if (!name.empty())
                englishNames.push_back(names.normalizedEnglishIdentity(name));
        }
    }

    std::vector<std::u16string> koreanNames;
    if (roster.hasKoreanColumn)
    {
        for (const std::u16string& rawName : roster.koreanNames)
        {
            if (!names.trimmed(rawName).empty())
                koreanNames.push_back(names.baseKoreanName(rawName));
        }
    }

    // The legacy roster matcher leaves evaluations unfiltered if the roster
    // has name columns but every matching cell is blank.
    if (englishNames.empty() && koreanNames.empty())
    {
        return matrix;
    }

    std::vector<ClassAnalyticsEvaluationRow> filtered;
    filtered.reserve(matrix.size());
    for (const ClassAnalyticsEvaluationRow& row : matrix)
    {
        const std::u16string english =
            names.normalizedEnglishIdentity(row.englishName);
        const std::u16string korean = names.baseKoreanName(row.koreanName);
        const bool englishMatches = !english.empty()
            && std::ranges::find(englishNames, english) != englishNames.end();
        const bool koreanMatches = !korean.empty()
            && std::ranges::find(koreanNames, korean) != koreanNames.end();
        if (englishMatches || koreanMatches)
        {
            filtered.push_back(row);
        }
    }
    return filtered;
}

[[nodiscard]] inline ClassAnalyticsSnapshot compute(
    const ClassAnalyticsEvaluationRows& rows,
    const std::size_t rosterCount,
    const ClassAnalyticsNameSemanticsPort& names
    )
{
    struct StudentAccumulator final
    {
        std::u16string englishName;
        std::u16string koreanName;
        std::array<double, kClassAnalyticsCriterionCount> sums{};
        std::array<int, kClassAnalyticsCriterionCount> counts{};
    };

    std::unordered_map<std::u16string, std::size_t> indexByKey;
    std::vector<StudentAccumulator> students;
    students.reserve(rows.size());
    for (const ClassAnalyticsEvaluationRow& row : rows)
    {
        const std::u16string english = names.trimmed(row.englishName);
        const std::u16string korean = names.trimmed(row.koreanName);
        const ClassAnalyticsEvaluationRow identityRow{
            .englishName = english,
            .koreanName = korean
        };
        const std::u16string key = studentKey(identityRow, names);
        if (key.empty()) continue;

        auto [found, inserted] = indexByKey.emplace(key, students.size());
        if (inserted)
        {
            students.push_back({ english, korean, {}, {} });
        }
        StudentAccumulator& student = students[found->second];
        if (student.englishName.empty()) student.englishName = english;
        if (student.koreanName.empty()) student.koreanName = korean;

        for (std::size_t criterion = 0;
             criterion < kClassAnalyticsCriterionCount;
             ++criterion)
        {
            const std::u16string score = names.trimmed(row.scores[criterion]);
            const int value = gradeValue(score);
            if (value <= 0) continue;
            student.sums[criterion] += value;
            ++student.counts[criterion];
        }
    }

    ClassAnalyticsSnapshot snapshot;
    snapshot.rosterStudentCount = rosterCount;
    std::array<double, kClassAnalyticsCriterionCount> criterionSums{};
    std::array<int, kClassAnalyticsCriterionCount> criterionCounts{};
    std::array<int, kClassAnalyticsCriterionCount> criterionStudentCounts{};
    std::array<std::array<int, 5>, kClassAnalyticsCriterionCount> distributions{};
    double classSum = 0.0;
    int classCount = 0;

    for (const StudentAccumulator& student : students)
    {
        double scoreSum = 0.0;
        int scoreCount = 0;
        bool fullyScored = true;
        std::array<std::u16string, kClassAnalyticsCriterionCount> letters;

        for (std::size_t criterion = 0;
             criterion < kClassAnalyticsCriterionCount;
             ++criterion)
        {
            if (student.counts[criterion] == 0)
            {
                fullyScored = false;
                continue;
            }

            const double average = roundTo3(
                student.sums[criterion] / student.counts[criterion]);
            const int letterValue = roundAverageToGrade(average);
            letters[criterion] = numberToGrade(letterValue);
            criterionSums[criterion] += student.sums[criterion];
            criterionCounts[criterion] += student.counts[criterion];
            ++criterionStudentCounts[criterion];
            ++distributions[criterion][static_cast<std::size_t>(letterValue - 1)];
            scoreSum += student.sums[criterion];
            scoreCount += student.counts[criterion];
        }

        if (scoreCount == 0) continue;
        ClassAnalyticsStudentRank rank;
        rank.englishName = student.englishName;
        rank.koreanName = student.koreanName;
        rank.overall3 = roundTo3(scoreSum / scoreCount);
        rank.overallLetter = numberToGrade(
            roundAverageToGrade(rank.overall3));
        rank.criterionLetters = std::move(letters);
        rank.fullyScored = fullyScored;
        snapshot.rankings.push_back(rank);
        snapshot.overallLetters.push_back(rank.overallLetter);
        classSum += rank.overall3;
        ++classCount;
        if (fullyScored) ++snapshot.fullyScoredCount;
    }

    snapshot.hasData = classCount > 0;
    if (!snapshot.hasData) return snapshot;

    snapshot.classAverage3 = roundTo3(classSum / classCount);
    snapshot.classAverageLetter = numberToGrade(
        roundAverageToGrade(snapshot.classAverage3));

    std::array<double, kClassAnalyticsCriterionCount> criterionAverages{};
    for (std::size_t criterion = 0;
         criterion < kClassAnalyticsCriterionCount;
         ++criterion)
    {
        ClassAnalyticsCriterionSlice slice;
        slice.name = std::u16string(kClassAnalyticsCriterionNames[criterion]);
        slice.students = criterionStudentCounts[criterion];
        slice.distribution = distributions[criterion];
        slice.hasData = criterionCounts[criterion] > 0;
        if (slice.hasData)
        {
            slice.average3 = roundTo3(
                criterionSums[criterion] / criterionCounts[criterion]);
            criterionAverages[criterion] = slice.average3;
        }
        else
        {
            criterionAverages[criterion] = -1.0;
        }
        snapshot.criteria.push_back(std::move(slice));
    }

    double strongest = -1.0;
    double focus = 6.0;
    for (const double average : criterionAverages)
    {
        if (average < 0.0) continue;
        strongest = std::max(strongest, average);
        focus = std::min(focus, average);
    }
    for (const ClassAnalyticsCriterionSlice& criterion : snapshot.criteria)
    {
        if (!criterion.hasData) continue;
        if (std::abs(criterion.average3 - strongest) < 1e-9)
        {
            snapshot.strongestNames.push_back(criterion.name);
            snapshot.strongestLabels.push_back(criterionLabel(criterion));
        }
        if (std::abs(criterion.average3 - focus) < 1e-9)
        {
            snapshot.focusNames.push_back(criterion.name);
            snapshot.focusLabels.push_back(criterionLabel(criterion));
        }
    }

    std::sort(
        snapshot.rankings.begin(), snapshot.rankings.end(),
        [&names](const ClassAnalyticsStudentRank& left,
                 const ClassAnalyticsStudentRank& right)
        {
            if (left.overall3 != right.overall3)
                return left.overall3 > right.overall3;
            return names.compareEnglishNames(left.englishName,
                                             right.englishName) < 0;
        });
    return snapshot;
}

[[nodiscard]] inline bool isAllSelection(
    const std::u16string_view selection,
    const ClassAnalyticsNameSemanticsPort& names
    )
{
    return names.equalsCaseInsensitive(selection, u"All");
}

[[nodiscard]] inline std::optional<ClassAnalyticsYearToDatePoint>
yearToDatePoint(
    const std::u16string_view evaluationName,
    const ClassAnalyticsSnapshot& snapshot
    )
{
    if (snapshot.fullyScoredCount == 0) return std::nullopt;
    double sum = 0.0;
    std::size_t count = 0;
    for (const ClassAnalyticsStudentRank& rank : snapshot.rankings)
    {
        if (!rank.fullyScored) continue;
        sum += rank.overall3;
        ++count;
    }
    if (count == 0) return std::nullopt;
    const double average = roundTo3(sum / static_cast<double>(count));
    return ClassAnalyticsYearToDatePoint{
        .evaluationName = std::u16string(evaluationName),
        .classAverage3 = average,
        .classAverageLetter = numberToGrade(roundAverageToGrade(average))
    };
}

} // namespace ClassAnalyticsDashboardReadDetail

class ClassAnalyticsDashboardReadQuery final
{
public:
    [[nodiscard]] static ClassAnalyticsDashboardResult execute(
        const ClassAnalyticsDashboardReadRequest& query,
        const ClassAnalyticsDashboardReadPort& readPort,
        const ClassAnalyticsNameSemanticsPort& names
        )
    {
        using namespace ClassAnalyticsDashboardReadDetail;

        if (!isCanonicalPositiveClassId(query.classId.value()))
        {
            return ClassAnalyticsDashboardResult::failure(invalidClassId());
        }

        const auto roster = readPort.readRosterNames(query.classId);
        if (!roster)
        {
            return ClassAnalyticsDashboardResult::failure(roster.error());
        }

        struct EvaluationView final
        {
            ClassAnalyticsEvaluation evaluation;
            ClassAnalyticsEvaluationRows rawRows;
            ClassAnalyticsEvaluationRows filteredRows;
            ClassAnalyticsSnapshot filteredSnapshot;
            ClassAnalyticsSnapshot historicalSnapshot;
        };
        std::array<EvaluationView, kClassAnalyticsEvaluationNames.size()>
            evaluations;

        for (std::size_t index = 0;
             index < kClassAnalyticsEvaluationNames.size();
             ++index)
        {
            const auto evaluation = static_cast<ClassAnalyticsEvaluation>(index);
            const auto loaded = readPort.readEvaluation(query.classId, evaluation);
            if (!loaded)
            {
                return ClassAnalyticsDashboardResult::failure(loaded.error());
            }

            EvaluationView& view = evaluations[index];
            view.evaluation = evaluation;
            view.rawRows = loaded.value();
            view.filteredRows = filterByRoster(
                view.rawRows, roster.value(), names);
            view.filteredSnapshot = compute(
                view.filteredRows, roster.value().rowCount, names);
            view.historicalSnapshot = compute(
                view.rawRows, roster.value().rowCount, names);
        }

        const std::u16string selection = names.trimmed(query.evaluationSelection);
        const bool allEvaluations = selection.empty()
            || isAllSelection(selection, names);
        ClassAnalyticsDashboard dashboard;
        if (allEvaluations)
        {
            ClassAnalyticsEvaluationRows currentRows;
            for (const EvaluationView& view : evaluations)
            {
                if (view.rawRows.empty()) continue;
                currentRows.insert(currentRows.end(),
                                   view.filteredRows.begin(),
                                   view.filteredRows.end());
            }
            dashboard.selectedSnapshot = compute(
                currentRows, roster.value().rowCount, names);

            for (auto view = evaluations.rbegin();
                 view != evaluations.rend();
                 ++view)
            {
                if (view->filteredSnapshot.fullyScoredCount == 0) continue;
                dashboard.classShapeEvaluationName = std::u16string(
                    classAnalyticsEvaluationName(view->evaluation));
                dashboard.classShapeSnapshot = view->filteredSnapshot;
                break;
            }
        }
        else
        {
            for (const EvaluationView& view : evaluations)
            {
                if (selection != classAnalyticsEvaluationName(view.evaluation))
                    continue;
                dashboard.selectedSnapshot = view.filteredSnapshot;
                dashboard.classShapeEvaluationName = std::u16string(
                    classAnalyticsEvaluationName(view.evaluation));
                dashboard.classShapeSnapshot = view.filteredSnapshot;
                break;
            }
        }

        for (const EvaluationView& view : evaluations)
        {
            if (auto point = yearToDatePoint(
                    classAnalyticsEvaluationName(view.evaluation),
                    view.historicalSnapshot))
            {
                dashboard.yearToDatePoints.push_back(std::move(*point));
            }
        }

        return ClassAnalyticsDashboardResult::success(std::move(dashboard));
    }
};

} // namespace ClassMngr::Next::Application
