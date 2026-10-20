#pragma once

#include "next/application/class_analytics_average_thousandths.h"
#include "next/application/class_analytics_dashboard_read_port.h"
#include "next/application/class_analytics_criterion_extrema_policy.h"
#include "next/application/class_analytics_criterion_student_count.h"
#include "next/application/class_analytics_numeric_policy.h"
#include "next/application/class_analytics_ranking_order_policy.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/speaking_evaluation_grade.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class ClassAnalyticsEvaluationSelection final
{
public:
    enum class Kind
    {
        All,
        ExactName
    };

    ClassAnalyticsEvaluationSelection() noexcept = default;

    [[nodiscard]] static ClassAnalyticsEvaluationSelection all()
        noexcept
    {
        return {};
    }

    [[nodiscard]] static ClassAnalyticsEvaluationSelection exactName(
        std::u16string name
        )
    {
        return {Kind::ExactName, std::move(name)};
    }

    [[nodiscard]] static ClassAnalyticsEvaluationSelection fromLegacyText(
        const std::u16string_view text,
        const ClassAnalyticsNameSemanticsPort& names
        )
    {
        const std::u16string trimmed = names.trimmed(text);
        if (trimmed.empty()
            || names.equalsCaseInsensitive(trimmed, u"All"))
        {
            return all();
        }
        // Keep the original text; execute() trims only its comparison copy.
        return exactName(std::u16string(text));
    }

    [[nodiscard]] Kind kind() const noexcept
    {
        return m_kind;
    }

    [[nodiscard]] bool isAll() const noexcept
    {
        return m_kind == Kind::All;
    }

    [[nodiscard]] const std::u16string& name() const noexcept
    {
        return m_exactName;
    }

private:
    ClassAnalyticsEvaluationSelection(
        const Kind kind,
        std::u16string name
        )
        : m_kind(kind)
        , m_exactName(std::move(name))
    {
    }

    Kind m_kind = Kind::All;
    std::u16string m_exactName;
};

struct ClassAnalyticsDashboardReadRequest final
{
    Domain::ClassId classId;
    ClassAnalyticsEvaluationSelection evaluationSelection;
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
    ClassAnalyticsCriterionStudentCount students;
    std::array<int, 5> distribution{};
    ClassAnalyticsAverageThousandths average3;
    bool hasData = false;
};

struct ClassAnalyticsStudentRank final
{
    std::u16string englishName;
    std::u16string koreanName;
    ClassAnalyticsAverageThousandths overall3;
    Domain::SpeakingEvaluationGrade overallLetter =
        Domain::SpeakingEvaluationGrade::C;
    std::array<std::optional<Domain::SpeakingEvaluationGrade>,
               kClassAnalyticsCriterionCount> criterionLetters{};
    bool fullyScored = false;
};

struct ClassAnalyticsSnapshot final
{
    bool hasData = false;
    ClassAnalyticsAverageThousandths classAverage3;
    std::optional<Domain::SpeakingEvaluationGrade> classAverageLetter;
    std::size_t rosterStudentCount = 0;
    std::size_t fullyScoredCount = 0;
    std::vector<ClassAnalyticsCriterionSlice> criteria;
    std::vector<std::u16string> strongestNames;
    std::vector<std::u16string> focusNames;
    std::vector<std::u16string> strongestLabels;
    std::vector<std::u16string> focusLabels;
    std::vector<Domain::SpeakingEvaluationGrade> overallLetters;
    std::vector<ClassAnalyticsStudentRank> rankings;
};

struct ClassAnalyticsYearToDatePoint final
{
    std::u16string evaluationName;
    ClassAnalyticsAverageThousandths classAverage3;
    Domain::SpeakingEvaluationGrade classAverageLetter =
        Domain::SpeakingEvaluationGrade::C;
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
    return ClassAnalyticsNumericPolicy::roundTo3(value);
}

[[nodiscard]] inline int roundAverageToGrade(const double average) noexcept
{
    return ClassAnalyticsNumericPolicy::roundAverageToGrade(average);
}

[[nodiscard]] inline int roundAverageToGrade(
    const ClassAnalyticsAverageThousandths average
    ) noexcept
{
    return ClassAnalyticsNumericPolicy::roundAverageToGrade(average);
}

[[nodiscard]] inline Domain::SpeakingEvaluationGrade numberToGrade(
    const int value
    )
{
    return *Domain::speakingEvaluationGradeFromValue(std::clamp(value, 1, 5));
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
    label += gradeLabel(std::clamp(
        roundAverageToGrade(criterion.average3), 1, 5));
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
    std::int64_t classSumThousandths = 0;
    int classCount = 0;

    for (const StudentAccumulator& student : students)
    {
        double scoreSum = 0.0;
        int scoreCount = 0;
        bool fullyScored = true;
        std::array<std::optional<Domain::SpeakingEvaluationGrade>,
                   kClassAnalyticsCriterionCount> letters{};

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
        rank.overall3 = ClassAnalyticsAverageThousandths{
            ClassAnalyticsNumericPolicy::roundToThousandths(
                scoreSum / scoreCount)
        };
        rank.overallLetter = numberToGrade(
            roundAverageToGrade(rank.overall3));
        rank.criterionLetters = std::move(letters);
        rank.fullyScored = fullyScored;
        snapshot.rankings.push_back(rank);
        snapshot.overallLetters.push_back(rank.overallLetter);
        classSumThousandths += rank.overall3.thousandths();
        ++classCount;
        if (fullyScored) ++snapshot.fullyScoredCount;
    }

    snapshot.hasData = classCount > 0;
    if (!snapshot.hasData) return snapshot;

    snapshot.classAverage3 = ClassAnalyticsAverageThousandths{
        ClassAnalyticsNumericPolicy::roundMeanOfThousandths(
            classSumThousandths, static_cast<std::size_t>(classCount))
    };
    snapshot.classAverageLetter = numberToGrade(
        roundAverageToGrade(snapshot.classAverage3));

    for (std::size_t criterion = 0;
         criterion < kClassAnalyticsCriterionCount;
         ++criterion)
    {
        ClassAnalyticsCriterionSlice slice;
        slice.name = std::u16string(kClassAnalyticsCriterionNames[criterion]);
        slice.students = ClassAnalyticsCriterionStudentCount{
            criterionStudentCounts[criterion]
        };
        slice.distribution = distributions[criterion];
        slice.hasData = criterionCounts[criterion] > 0;
        if (slice.hasData)
        {
            slice.average3 = ClassAnalyticsAverageThousandths{
                ClassAnalyticsNumericPolicy::roundToThousandths(
                    criterionSums[criterion] / criterionCounts[criterion])
            };
        }
        snapshot.criteria.push_back(std::move(slice));
    }

    const auto extrema = selectClassAnalyticsCriterionExtrema(
        snapshot.criteria);
    for (const std::size_t index : extrema.strongestIndices)
    {
        const ClassAnalyticsCriterionSlice& criterion =
            snapshot.criteria[index];
        snapshot.strongestNames.push_back(criterion.name);
        snapshot.strongestLabels.push_back(criterionLabel(criterion));
    }
    for (const std::size_t index : extrema.focusIndices)
    {
        const ClassAnalyticsCriterionSlice& criterion =
            snapshot.criteria[index];
        snapshot.focusNames.push_back(criterion.name);
        snapshot.focusLabels.push_back(criterionLabel(criterion));
    }

    ClassAnalyticsRankingOrderPolicy::sortByScoreThenName(
        snapshot.rankings.begin(), snapshot.rankings.end(),
        [](const ClassAnalyticsStudentRank& rank)
        {
            return rank.overall3.thousandths();
        },
        [&names](const ClassAnalyticsStudentRank& left,
                 const ClassAnalyticsStudentRank& right)
        {
            return names.compareEnglishNames(left.englishName,
                                             right.englishName);
        });
    return snapshot;
}

[[nodiscard]] inline std::optional<ClassAnalyticsYearToDatePoint>
yearToDatePoint(
    const std::u16string_view evaluationName,
    const ClassAnalyticsSnapshot& snapshot
    )
{
    if (snapshot.fullyScoredCount == 0) return std::nullopt;
    std::int64_t sumThousandths = 0;
    std::size_t count = 0;
    for (const ClassAnalyticsStudentRank& rank : snapshot.rankings)
    {
        if (!rank.fullyScored) continue;
        sumThousandths += rank.overall3.thousandths();
        ++count;
    }
    if (count == 0) return std::nullopt;
    const ClassAnalyticsAverageThousandths average{
        ClassAnalyticsNumericPolicy::roundMeanOfThousandths(
            sumThousandths, count)
    };
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

        if (!isCanonicalPositiveDecimalId(query.classId.value()))
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

        auto loadedEvaluations =
            readPort.readEvaluationBatch(query.classId);
        if (!loadedEvaluations)
        {
            return ClassAnalyticsDashboardResult::failure(
                loadedEvaluations.error());
        }

        for (std::size_t index = 0;
             index < kClassAnalyticsEvaluationNames.size();
             ++index)
        {
            const auto evaluation = static_cast<ClassAnalyticsEvaluation>(index);
            EvaluationView& view = evaluations[index];
            view.evaluation = evaluation;
            view.rawRows = std::move(loadedEvaluations.value()[index]);
            view.filteredRows = filterByRoster(
                view.rawRows, roster.value(), names);
            view.filteredSnapshot = compute(
                view.filteredRows, roster.value().rowCount, names);
            view.historicalSnapshot = compute(
                view.rawRows, roster.value().rowCount, names);
        }

        const ClassAnalyticsEvaluationSelection& selection =
            query.evaluationSelection;
        ClassAnalyticsDashboard dashboard;
        if (selection.isAll())
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
            const std::u16string trimmedSelection =
                names.trimmed(selection.name());
            for (const EvaluationView& view : evaluations)
            {
                if (trimmedSelection
                    != classAnalyticsEvaluationName(view.evaluation))
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
