#include "classmngr/engine/speaking_analytics.h"
#include "classmngr/engine/student_name.h"

#include <cmath>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using namespace classmngr::engine;

bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSpeakingAnalyticsTests: "
              << message << '\n';
    return false;
}

SpeakingAnalyticsRow makeRow(
    std::string english,
    std::string korean,
    std::string grammar,
    std::string pronunciation,
    std::string fluency,
    std::string manner,
    std::string content,
    std::string overallEffort
    )
{
    return {
        "0",
        std::move(english),
        std::move(korean),
        std::move(grammar),
        std::move(pronunciation),
        std::move(fluency),
        std::move(manner),
        std::move(content),
        std::move(overallEffort),
        {},
        {}
    };
}

bool closeTo(double left, double right)
{
    return std::abs(left - right) < 1e-9;
}
} // namespace

int main()
{
    bool passed = true;

    passed &= expect(
        SpeakingAnalyticsService::evaluationNames()
            == std::vector<std::string>{
                "Winter", "Speech Contest", "Summer", "Fall"
            },
        "evaluation names changed"
        );
    passed &= expect(
        SpeakingAnalyticsService::gradeToNumber("A+") == 5
            && SpeakingAnalyticsService::gradeToNumber("A") == 4
            && SpeakingAnalyticsService::gradeToNumber("B+") == 3
            && SpeakingAnalyticsService::gradeToNumber("B") == 2
            && SpeakingAnalyticsService::gradeToNumber("C") == 1
            && SpeakingAnalyticsService::gradeToNumber({}) == 0
            && SpeakingAnalyticsService::numberToGrade(0) == "C"
            && SpeakingAnalyticsService::numberToGrade(9) == "A+",
        "grade conversion changed"
        );
    passed &= expect(
        SpeakingAnalyticsService::roundAverageToGrade(2.39) == 2
            && SpeakingAnalyticsService::roundAverageToGrade(2.6) == 3
            && SpeakingAnalyticsService::roundAverageToGrade(3.6) == 4
            && SpeakingAnalyticsService::roundAverageToGrade(0.0) == 0
            && closeTo(SpeakingAnalyticsService::roundTo3(3.833333), 3.833)
            && SpeakingAnalyticsService::formatAverage(3.833) == "3.8",
        "grade rounding or formatting changed"
        );
    passed &= expect(
        SpeakingAnalyticsService::strongestIndices({2.0, 4.0, 4.0})
            == std::vector<int>{1, 2}
            && SpeakingAnalyticsService::focusIndices({2.0, 2.0, 4.0})
                == std::vector<int>{0, 1},
        "strongest/focus ties changed"
        );

    const SpeakingAnalyticsRows matrix{
        makeRow(
            "Anna",
            "\xEC\x95\x88\xEB\x82\x98",
            "A+", "A+", "A+", "A+", "A+", "A+"
            ),
        makeRow(
            "Ben",
            "\xEB\xB2\xA4",
            "B", "B", "B", "B", "B", "B"
            ),
        makeRow(
            "Cara",
            "\xEC\xB9\xB4\xEB\x9D\xBC",
            "B+", "B+", "B+", "B+", "B+", "B+"
            )
    };
    const SpeakingAnalyticsSnapshot snapshot =
        SpeakingAnalyticsService::compute({matrix}, 12);
    passed &= expect(
        snapshot.hasData
            && snapshot.rosterStudentCount == 12
            && snapshot.fullyScoredCount == 3
            && closeTo(snapshot.classAverage3, 3.333)
            && snapshot.classAverageLetter == "B+"
            && snapshot.criteria.size() == 6
            && snapshot.criteria.front().students == 3
            && closeTo(snapshot.criteria.front().average3, 3.333)
            && snapshot.criteria.front().distribution.at("A+") == 1
            && snapshot.rankings.size() == 3
            && snapshot.rankings.front().englishName == "Anna"
            && snapshot.rankings.front().overallLetter == "A+"
            && snapshot.rankings.at(1).englishName == "Cara"
            && snapshot.rankings.at(2).englishName == "Ben",
        "full analytics snapshot changed"
        );
    passed &= expect(
        snapshot.strongestNames.size() == 6
            && snapshot.focusNames.size() == 6,
        "tied criterion insights changed"
        );

    const SpeakingAnalyticsRows partial{
        makeRow(
            "Dan", "\xEB\x8C\x84", "A", "B", "A", "B", "B+", {}
            ),
        makeRow(
            "Eve", "\xEC\x9D\xB4\xEB\xB8\x8C", "B", "B", "B", "B", "B", {}
            )
    };
    const SpeakingAnalyticsSnapshot partialSnapshot =
        SpeakingAnalyticsService::compute({partial}, 3);
    passed &= expect(
        partialSnapshot.fullyScoredCount == 0
            && closeTo(partialSnapshot.classAverage3, 2.5)
            && partialSnapshot.classAverageLetter == "B+"
            && partialSnapshot.criteria.at(
                   static_cast<std::size_t>(SpeakingAnalyticsCriterion::OverallEffort)
                   ).students == 0
            && !partialSnapshot.criteria.at(
                   static_cast<std::size_t>(SpeakingAnalyticsCriterion::OverallEffort)
                   ).hasData
            && partialSnapshot.strongestNames
                == std::vector<std::string>{"Grammar", "Fluency"}
            && partialSnapshot.focusNames
                == std::vector<std::string>{"Pronunciation", "Manner"},
        "partial analytics snapshot changed"
        );

    const SpeakingAnalyticsRows summer{
        makeRow(
            "avery", "\xEC\x97\x90\xEB\xB2\x84\xEB\xA6\xAC",
            "A", "A", "A", "A", "A", "A"
            )
    };
    const SpeakingAnalyticsSnapshot consolidated =
        SpeakingAnalyticsService::compute({
            SpeakingAnalyticsRows{
                makeRow(
                    "Avery", "\xEC\x97\x90\xEB\xB2\x84\xEB\xA6\xAC",
                    "B", "B", "B", "B", "B", "B"
                    )
            },
            summer
        }, 1);
    passed &= expect(
        consolidated.rankings.size() == 1
            && consolidated.rankings.front().overallLetter == "B+"
            && closeTo(consolidated.classAverage3, 3.0)
            && consolidated.fullyScoredCount == 1,
        "students were not consolidated across evaluations"
        );

    SpeakingAnalyticsRoster roster;
    roster.columns = {"English", "Korean"};
    roster.rows = {
        {"JOHN SMITH", "\xEC\x97\x90\xEB\xB2\x84\xEB\xA6\xAC(A)"},
        {"", "\xEB\x8C\x84"}
    };
    const SpeakingAnalyticsRows filterInput{
        makeRow(
            "john  smith", "\xEC\x9D\xB4\xEB\xB8\x8C",
            "B", "A", "A", "B", "B", "B"
            ),
        makeRow(
            "Avery", "\xEC\x97\x90\xEB\xB2\x84\xEB\xA6\xAC",
            "A", "A", "B", "A", "A", "A"
            ),
        makeRow(
            "Jill", "\xEC\xA7\x80\xEC\x9D\xBC",
            "A", "A", "B", "A", "A", "A"
            )
    };
    const SpeakingAnalyticsRows filtered =
        SpeakingAnalyticsService::filterMatrixByRoster(filterInput, roster);
    passed &= expect(
        filtered.size() == 2
            && filtered.at(0).at(1) == "john  smith"
            && filtered.at(1).at(1) == "Avery",
        "roster name matching changed"
        );

    const auto point = SpeakingAnalyticsService::yearToDatePoint(
        "Winter",
        snapshot
        );
    passed &= expect(
        point.has_value()
            && point->evaluationName == "Winter"
            && closeTo(point->classAverage3, 3.333)
            && point->classAverageLetter == "B+",
        "year-to-date point changed"
        );

    const SpeakingAnalyticsSnapshot noFull =
        SpeakingAnalyticsService::compute({partial}, 2);
    passed &= expect(
        !SpeakingAnalyticsService::yearToDatePoint("Summer", noFull).has_value(),
        "partial-only year-to-date point was not omitted"
        );

    SpeakingAnalyticsRoster dashboardRoster;
    dashboardRoster.columns = {"English", "Korean"};
    dashboardRoster.rows = {
        {"Alice", "\xEC\x95\x8C\xEB\xA6\xAC\xEC\x8A\xA4"},
        {"Bob", "\xEB\xB0\x95\xEB\xB3\xB4"}
    };
    const std::vector<SpeakingAnalyticsEvaluation> dashboardEvaluations{
        {
            "Winter",
            {
                makeRow(
                    "Alice", "\xEC\x95\x8C\xEB\xA6\xAC\xEC\x8A\xA4",
                    "B", "B", "B", "B", "B", "B"
                    ),
                makeRow(
                    "Carol", "\xEC\xB9\xB4\xEB\x9D\xBC",
                    "A", "A", "A", "A", "A", "A"
                    )
            }
        },
        {
            "Speech Contest",
            {
                makeRow(
                    "Bob", "\xEB\xB0\x95\xEB\xB3\xB4",
                    "A", "A", "A", "A", "A", "A"
                    )
            }
        },
        {
            "Summer",
            {
                makeRow(
                    "Alice", "\xEC\x95\x8C\xEB\xA6\xAC\xEC\x8A\xA4",
                    "A", "A", "A", "A", "A", "A"
                    ),
                makeRow(
                    "Bob", "\xEB\xB0\x95\xEB\xB3\xB4",
                    "B", "B", "B", "B", "B", "B"
                    )
            }
        },
        {
            "Fall",
            {
                makeRow(
                    "Alice", "\xEC\x95\x8C\xEB\xA6\xAC\xEC\x8A\xA4",
                    "A", "A", "A", "A", "A", {}
                    )
            }
        }
    };

    const auto buildDashboard =
        [&dashboardRoster, &dashboardEvaluations](std::string selection)
    {
        SpeakingAnalyticsDashboardInput input;
        input.selection = std::move(selection);
        input.roster = dashboardRoster;
        input.evaluations = dashboardEvaluations;
        return SpeakingAnalyticsService::buildDashboard(input);
    };
    const SpeakingAnalyticsDashboard allDashboard =
        buildDashboard("  aLl  ");
    const SpeakingAnalyticsDashboard emptyDashboard = buildDashboard({});
    passed &= expect(
        allDashboard.selectedSnapshot.rankings.size() == 2
            && allDashboard.classShapeEvaluationName == "Summer"
            && allDashboard.classShapeSnapshot.fullyScoredCount == 2
            && allDashboard.yearToDatePoints.size() == 3
            && closeTo(allDashboard.yearToDatePoints.front().classAverage3, 3.0)
            && emptyDashboard.classShapeEvaluationName
                == allDashboard.classShapeEvaluationName
            && emptyDashboard.selectedSnapshot.rankings.size()
                == allDashboard.selectedSnapshot.rankings.size(),
        "all and empty dashboard selection policy changed"
        );

    SpeakingAnalyticsRoster sparseDashboardRoster;
    sparseDashboardRoster.columns = {"English", "Korean"};
    sparseDashboardRoster.rows = {
        {"Alice", "\xEC\x95\x8C\xEB\xA6\xAC\xEC\x8A\xA4"},
        {"  ", "\t"},
        {},
        {"", "\xEB\xB0\x95\xEB\xB3\xB4"}
    };
    SpeakingAnalyticsDashboardInput sparseInput;
    sparseInput.roster = sparseDashboardRoster;
    sparseInput.evaluations = dashboardEvaluations;
    const SpeakingAnalyticsDashboard sparseDashboard =
        SpeakingAnalyticsService::buildDashboard(sparseInput);
    passed &= expect(
        sparseDashboard.selectedSnapshot.rosterStudentCount == 2,
        "dashboard roster count did not ignore blank or short roster rows"
        );

    const SpeakingAnalyticsDashboard selectedDashboard =
        buildDashboard("  Winter  ");
    passed &= expect(
        selectedDashboard.selectedSnapshot.rankings.size() == 1
            && selectedDashboard.selectedSnapshot.rankings.front().englishName
                == "Alice"
            && selectedDashboard.classShapeEvaluationName == "Winter"
            && selectedDashboard.classShapeSnapshot.rankings.size() == 1,
        "named dashboard selection policy changed"
        );

    const SpeakingAnalyticsDashboard partialDashboard =
        buildDashboard("Fall");
    passed &= expect(
        partialDashboard.selectedSnapshot.hasData
            && partialDashboard.selectedSnapshot.fullyScoredCount == 0
            && partialDashboard.classShapeEvaluationName == "Fall"
            && partialDashboard.yearToDatePoints.size() == 3,
        "partial dashboard selection policy changed"
        );

    const SpeakingAnalyticsDashboard unknownDashboard =
        buildDashboard("Unknown");
    passed &= expect(
        !unknownDashboard.selectedSnapshot.hasData
            && unknownDashboard.classShapeEvaluationName.empty()
            && !unknownDashboard.classShapeSnapshot.hasData
            && unknownDashboard.yearToDatePoints.size() == 3,
        "unknown dashboard selection policy changed"
        );

    passed &= expect(
        StudentNameService::normalizeEnglish("  j. p. kim  ")
            == "J.P.Kim",
        "portable English initial normalization changed"
        );
    passed &= expect(
        StudentNameService::normalizeEnglish("mary - jane")
            == "Mary-jane",
        "portable English hyphen normalization changed"
        );
    passed &= expect(
        StudentNameService::normalizeEnglish("A\xEB\xAF\xBC" "B")
            == "A\xEB\xAF\xBC" "B",
        "portable invalid English normalization changed"
        );
    passed &= expect(
        StudentNameService::normalizeKorean(
            " \xEA\xB9\x80\xEB\xAF\xBC (a) "
            ) == "\xEA\xB9\x80\xEB\xAF\xBC(A)",
        "portable Korean normalization changed"
        );
    passed &= expect(
        StudentNameService::baseKorean(
            "\xEA\xB9\x80\xEB\xAF\xBC(A)"
            ) == "\xEA\xB9\x80\xEB\xAF\xBC"
            && StudentNameService::koreanSuffix(
                   "\xEA\xB9\x80\xEB\xAF\xBC(a)"
                   ) == "A",
        "portable Korean suffix handling changed"
        );
    passed &= expect(
        StudentNameService::normalizeKorean("(A)") == "(A)",
        "invalid Korean suffix-only input was normalized away"
        );

    return passed ? 0 : 1;
}
