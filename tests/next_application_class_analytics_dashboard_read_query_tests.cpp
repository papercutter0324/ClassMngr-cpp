#include "next/application/class_analytics_dashboard_read_query.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

#undef assert
#define assert(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            std::cerr << "Class Analytics query assertion failed at line " \
                      << __LINE__ << std::endl; \
            std::exit(EXIT_FAILURE); \
        } \
    } while (false)

namespace
{

class TestNameSemantics final : public ClassAnalyticsNameSemanticsPort
{
public:
    [[nodiscard]] std::u16string trimmed(
        const std::u16string_view value
        ) const override
    {
        const auto isWhitespace = [](const char16_t character)
        {
            return character == u' ' || character == u'\t'
                || character == u'\r' || character == u'\n';
        };
        std::size_t first = 0;
        std::size_t last = value.size();
        while (first < last && isWhitespace(value[first])) ++first;
        while (last > first && isWhitespace(value[last - 1])) --last;
        return std::u16string(value.substr(first, last - first));
    }

    [[nodiscard]] std::u16string normalizedEnglishIdentity(
        const std::u16string_view value
        ) const override
    {
        const std::u16string source = trimmed(value);
        std::u16string normalized;
        normalized.reserve(source.size());
        for (std::size_t index = 0; index < source.size(); ++index)
        {
            char16_t character = source[index];
            if (character == u' ')
            {
                if (index > 0 && index + 1 < source.size()
                    && (source[index - 1] == u'-' || source[index + 1] == u'-'
                        || source[index - 1] == u'.' || source[index + 1] == u'.'))
                {
                    continue;
                }
                if (!normalized.empty() && normalized.back() == u' ')
                    continue;
            }
            if (character >= u'A' && character <= u'Z')
                character = static_cast<char16_t>(character + (u'a' - u'A'));
            normalized.push_back(character);
        }
        return normalized;
    }

    [[nodiscard]] std::u16string baseKoreanName(
        const std::u16string_view value
        ) const override
    {
        std::u16string source = trimmed(value);
        if (source.size() >= 3
            && source[source.size() - 3] == u'('
            && source[source.size() - 2] >= u'A'
            && source[source.size() - 2] <= u'Z'
            && source.back() == u')')
        {
            source.resize(source.size() - 3);
        }
        return source;
    }

    [[nodiscard]] bool equalsCaseInsensitive(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        if (left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            const char16_t a = left[index] >= u'A' && left[index] <= u'Z'
                ? static_cast<char16_t>(left[index] + (u'a' - u'A'))
                : left[index];
            const char16_t b = right[index] >= u'A' && right[index] <= u'Z'
                ? static_cast<char16_t>(right[index] + (u'a' - u'A'))
                : right[index];
            if (a != b) return false;
        }
        return true;
    }

    [[nodiscard]] int compareEnglishNames(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        std::u16string leftFolded(left);
        std::u16string rightFolded(right);
        std::transform(leftFolded.begin(), leftFolded.end(), leftFolded.begin(),
            [](char16_t value)
            {
                return value >= u'A' && value <= u'Z'
                    ? static_cast<char16_t>(value + (u'a' - u'A')) : value;
            });
        std::transform(rightFolded.begin(), rightFolded.end(), rightFolded.begin(),
            [](char16_t value)
            {
                return value >= u'A' && value <= u'Z'
                    ? static_cast<char16_t>(value + (u'a' - u'A')) : value;
            });
        if (leftFolded < rightFolded) return -1;
        if (leftFolded > rightFolded) return 1;
        return 0;
    }
};

class FakeReadPort final : public ClassAnalyticsDashboardReadPort
{
public:
    ClassAnalyticsRosterNames roster;
    std::array<ClassAnalyticsEvaluationRows, 4> evaluations;
    mutable std::vector<std::u16string> calls;
    mutable std::vector<std::string> rosterClassIds;
    mutable std::vector<std::string> evaluationBatchClassIds;
    mutable int evaluationBatchReadCount = 0;
    bool failEvaluationBatch = false;

    [[nodiscard]] Domain::Result<ClassAnalyticsRosterNames> readRosterNames(
        const Domain::ClassId& classId
        ) const override
    {
        rosterClassIds.push_back(classId.value());
        calls.push_back(u"roster");
        return Domain::Result<ClassAnalyticsRosterNames>::success(roster);
    }

    [[nodiscard]] Domain::Result<ClassAnalyticsEvaluationBatch>
    readEvaluationBatch(
        const Domain::ClassId& classId
        ) const override
    {
        evaluationBatchClassIds.push_back(classId.value());
        ++evaluationBatchReadCount;
        calls.push_back(u"evaluation batch");
        if (failEvaluationBatch)
        {
            return Domain::Result<ClassAnalyticsEvaluationBatch>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "evaluation batch read failed",
                .recoverable = true
            });
        }
        return Domain::Result<ClassAnalyticsEvaluationBatch>::success(
            evaluations);
    }
};

[[nodiscard]] Domain::ClassId classId()
{
    return *Domain::ClassId::fromString("7");
}

[[nodiscard]] ClassAnalyticsEvaluationRow row(
    std::u16string english,
    std::u16string korean,
    const std::u16string_view grade,
    const bool fullyScored = true
    )
{
    ClassAnalyticsEvaluationRow result;
    result.englishName = std::move(english);
    result.koreanName = std::move(korean);
    for (std::size_t index = 0; index < result.scores.size(); ++index)
    {
        if (fullyScored || index == 0)
            result.scores[index] = std::u16string(grade);
    }
    return result;
}

[[nodiscard]] ClassAnalyticsDashboardResult run(
    FakeReadPort& port,
    const TestNameSemantics& names
    )
{
    return ClassAnalyticsDashboardReadQuery::execute(
        { .classId = classId() }, port, names);
}

[[nodiscard]] ClassAnalyticsDashboardResult run(
    FakeReadPort& port,
    const TestNameSemantics& names,
    const ClassAnalyticsEvaluationSelection& selection
    )
{
    return ClassAnalyticsDashboardReadQuery::execute(
        { .classId = classId(), .evaluationSelection = selection }, port, names);
}

[[nodiscard]] ClassAnalyticsDashboardResult runLegacyText(
    FakeReadPort& port,
    const TestNameSemantics& names,
    const std::u16string_view selection
    )
{
    return run(port, names,
        ClassAnalyticsEvaluationSelection::fromLegacyText(selection, names));
}

void appendScoresForAverage(
    ClassAnalyticsEvaluationRows& rows,
    const std::u16string_view englishName,
    const int scoredCellCount,
    const int higherGradeCellCount
    )
{
    int scoreCell = 0;
    while (scoreCell < scoredCellCount)
    {
        ClassAnalyticsEvaluationRow current;
        current.englishName = englishName;
        for (std::size_t criterion = 0;
             criterion < current.scores.size()
                 && scoreCell < scoredCellCount;
             ++criterion, ++scoreCell)
        {
            current.scores[criterion] = scoreCell < higherGradeCellCount
                ? u"B+" : u"B";
        }
        rows.push_back(std::move(current));
    }
}

void averageThousandthsValueSemantics()
{
    using Average = ClassAnalyticsAverageThousandths;
    static_assert(std::is_constructible_v<Average, int>);
    static_assert(!std::is_convertible_v<int, Average>);

    const Average zero{};
    assert(zero.thousandths() == 0);
    assert(zero.toDouble() == 0.0);

    const Average value(2399);
    assert(value.thousandths() == 2399);
    assert(value.toDouble() == 2.399);
    assert(value == Average(2399));
    assert(value != Average(2400));

    // Construction preserves values outside the analytics display range.
    assert(Average(-125).thousandths() == -125);
    assert(Average(6000).thousandths() == 6000);
}

void evaluationSelectionDefaultsAndLegacyTextConversion()
{
    const TestNameSemantics names;
    const ClassAnalyticsDashboardReadRequest defaultRequest{
        .classId = classId()
    };
    assert(defaultRequest.evaluationSelection.isAll());
    assert(defaultRequest.evaluationSelection.kind()
        == ClassAnalyticsEvaluationSelection::Kind::All);

    const auto blank = ClassAnalyticsEvaluationSelection::fromLegacyText(
        u" \t\r\n ", names);
    assert(blank.isAll());
    const auto legacyAll = ClassAnalyticsEvaluationSelection::fromLegacyText(
        u" aLl ", names);
    assert(legacyAll.isAll());

    std::u16string mutableName = u"Winter";
    const auto owningExactName =
        ClassAnalyticsEvaluationSelection::exactName(mutableName);
    mutableName.clear();
    assert(!owningExactName.isAll());
    assert(owningExactName.kind()
        == ClassAnalyticsEvaluationSelection::Kind::ExactName);
    assert(owningExactName.name() == u"Winter");

    const auto exactAll = ClassAnalyticsEvaluationSelection::exactName(u"All");
    assert(!exactAll.isAll());
    assert(exactAll.kind() == ClassAnalyticsEvaluationSelection::Kind::ExactName);
    assert(exactAll.name() == u"All");

    const auto legacyPaddedName =
        ClassAnalyticsEvaluationSelection::fromLegacyText(u" Winter ", names);
    assert(!legacyPaddedName.isAll());
    assert(legacyPaddedName.name() == u" Winter ");
}

void canonicalOrderLatestClassShapeAndYtd()
{
    FakeReadPort port;
    port.evaluations[0].push_back(row(u"Taylor", u"", u"B"));
    port.evaluations[1].push_back(row(u"Taylor", u"", u"B+"));
    // A later partial cohort cannot replace the latest fully scored shape.
    port.evaluations[3].push_back(row(u"Taylor", u"", u"A", false));
    const TestNameSemantics names;

    const auto result = run(port, names);
    assert(result);
    assert((port.calls == std::vector<std::u16string>{
        u"roster", u"evaluation batch" }));
    assert(port.evaluationBatchReadCount == 1);
    assert(result.value().selectedSnapshot.hasData);
    assert(result.value().selectedSnapshot.rankings.size() == 1);
    // All aggregates every current score, including Fall's partial score.
    assert(result.value().selectedSnapshot.rankings.front().overall3
        .thousandths() == 2615);
    assert(result.value().classShapeEvaluationName == u"Speech Contest");
    assert(result.value().classShapeSnapshot.fullyScoredCount == 1);
    assert(result.value().yearToDatePoints.size() == 2);
    assert(result.value().yearToDatePoints[0].evaluationName == u"Winter");
    assert(result.value().yearToDatePoints[1].evaluationName == u"Speech Contest");

    const auto legacyAll = runLegacyText(port, names, u" aLl ");
    assert(legacyAll);
    assert(legacyAll.value().selectedSnapshot.rankings.size() == 1);
    assert(legacyAll.value().selectedSnapshot.rankings.front().overall3
        == result.value().selectedSnapshot.rankings.front().overall3);
    assert(legacyAll.value().classShapeEvaluationName
        == result.value().classShapeEvaluationName);
    assert(legacyAll.value().yearToDatePoints.size()
        == result.value().yearToDatePoints.size());
}

void strongestAndFocusCriteriaPreserveOrderAndLabels()
{
    FakeReadPort port;
    ClassAnalyticsEvaluationRow student = row(u"Taylor", u"", u"B");
    student.scores[0] = u"A"; // Grammar: strongest tie.
    student.scores[1] = u"B"; // Pronunciation: focus tie.
    student.scores[2] = u"A"; // Fluency: strongest tie.
    student.scores[3].clear(); // Manner: no data.
    student.scores[4] = u"B"; // Content: focus tie.
    student.scores[5].clear(); // Overall Effort: no data.
    port.evaluations[0].push_back(std::move(student));
    const TestNameSemantics names;

    const auto result = run(port, names,
        ClassAnalyticsEvaluationSelection::exactName(u"Winter"));
    assert(result);
    const auto& selected = result.value().selectedSnapshot;
    assert((selected.strongestNames == std::vector<std::u16string>{
        u"Grammar", u"Fluency" }));
    assert((selected.focusNames == std::vector<std::u16string>{
        u"Pronunciation", u"Content" }));
    assert((selected.strongestLabels == std::vector<std::u16string>{
        u"Grammar (A)", u"Fluency (A)" }));
    assert((selected.focusLabels == std::vector<std::u16string>{
        u"Pronunciation (B)", u"Content (B)" }));
    assert(!selected.criteria[3].hasData);
    assert(!selected.criteria[5].hasData);
}

void currentRosterUsesNormalizedEnglishAndKoreanIdentities()
{
    FakeReadPort port;
    port.roster.rowCount = 2;
    port.roster.hasEnglishColumn = true;
    port.roster.hasKoreanColumn = true;
    port.roster.englishNames = { u"Mary - Jane", u"" };
    port.roster.koreanNames = { u"", u"김민수(B)" };
    port.evaluations[0] = {
        row(u"Mary-Jane", u"", u"B"),
        row(u"", u"김민수(A)", u"A"),
        row(u"Former", u"박지훈", u"A+")
    };
    const TestNameSemantics names;

    const auto result = run(port, names,
        ClassAnalyticsEvaluationSelection::exactName(u"Winter"));
    assert(result);
    const auto& selected = result.value().selectedSnapshot;
    assert(selected.rankings.size() == 2);
    assert(selected.rankings[0].koreanName == u"김민수(A)");
    assert(selected.rankings[1].englishName == u"Mary-Jane");
    assert(result.value().yearToDatePoints.size() == 1);
    // Historical YTD is global and still includes the Former row.
    assert(result.value().yearToDatePoints.front().classAverage3
        .thousandths() > 3000);
}

void duplicateRowsConsolidateAndRankingTiesUseNamePort()
{
    FakeReadPort port;
    port.evaluations[0] = {
        row(u"J. P. Kim", u"", u"B"),
        row(u"J.P.KIM", u"", u"A"),
        row(u"Bee", u"", u"B+")
    };
    const TestNameSemantics names;

    const auto result = run(port, names,
        ClassAnalyticsEvaluationSelection::exactName(u"Winter"));
    assert(result);
    const auto& selected = result.value().selectedSnapshot;
    assert(selected.rankings.size() == 2);
    assert(selected.rankings[0].englishName == u"Bee");
    assert(selected.rankings[1].englishName == u"J. P. Kim");
    assert(selected.rankings[0].overall3.thousandths() == 3000);
    assert(selected.rankings[1].overall3.thousandths() == 3000);
}

void adjacentRoundedScoresRankBeforeNamePort()
{
    FakeReadPort port;
    // 112 rows provide 672 score cells per student. One extra point therefore
    // rounds to an adjacent thousandth: 3.001 must beat 3.000 even though Amy
    // sorts before Zed by name.
    for (int repeat = 0; repeat < 112; ++repeat)
    {
        port.evaluations[0].push_back(row(u"Amy", u"", u"B+"));
        ClassAnalyticsEvaluationRow higher = row(u"Zed", u"", u"B+");
        if (repeat == 0) higher.scores[0] = u"A";
        port.evaluations[0].push_back(std::move(higher));
    }
    const TestNameSemantics names;

    const auto result = run(port, names,
        ClassAnalyticsEvaluationSelection::exactName(u"Winter"));
    assert(result);
    const auto& rankings = result.value().selectedSnapshot.rankings;
    assert(rankings.size() == 2);
    assert(rankings[0].englishName == u"Zed");
    assert(rankings[0].overall3.thousandths() == 3001);
    assert(rankings[0].overallLetter == u"B+");
    assert(rankings[1].englishName == u"Amy");
    assert(rankings[1].overall3.thousandths() == 3000);
    assert(rankings[1].overallLetter == u"B+");
}

void emptyRosterLeavesEvaluationRowsUnfilteredAndPartialScoresDoNotMakeYtd()
{
    FakeReadPort port;
    port.roster.rowCount = 0;
    port.evaluations[3].push_back(row(u"Former", u"", u"A", false));
    const TestNameSemantics names;

    const auto result = run(port, names,
        ClassAnalyticsEvaluationSelection::exactName(u"Fall"));
    assert(result);
    assert(result.value().selectedSnapshot.rankings.size() == 1);
    assert(!result.value().selectedSnapshot.rankings.front().fullyScored);
    assert(result.value().yearToDatePoints.empty());
}

void missingAndUnknownSelectionKeepGlobalYtdWhileUnknownSelectionIsEmpty()
{
    FakeReadPort port;
    port.evaluations[0].push_back(row(u"Current", u"", u"B"));
    port.evaluations[3].push_back(row(u"Current", u"", u"A"));
    const TestNameSemantics names;

    for (const std::u16string_view unknown : {
             u"winter", u"Unknown selection", u"All" })
    {
        const auto result = run(port, names,
            ClassAnalyticsEvaluationSelection::exactName(
                std::u16string(unknown)));
        assert(result);
        assert(!result.value().selectedSnapshot.hasData);
        assert(result.value().selectedSnapshot.rankings.empty());
        assert(!result.value().classShapeSnapshot.hasData);
        assert(result.value().classShapeSnapshot.rankings.empty());
        assert(result.value().classShapeEvaluationName.empty());
        assert(result.value().yearToDatePoints.size() == 2);
        assert(result.value().yearToDatePoints[0].evaluationName == u"Winter");
        assert(result.value().yearToDatePoints[1].evaluationName == u"Fall");
    }
}

void typedAndPaddedExactNamesSelectAfterTrimmedComparison()
{
    FakeReadPort port;
    port.evaluations[0].push_back(row(u"Current", u"", u"B"));
    port.evaluations[3].push_back(row(u"Current", u"", u"A"));
    const TestNameSemantics names;

    const auto exactWinter =
        ClassAnalyticsEvaluationSelection::exactName(u"Winter");
    const auto exactResult = run(port, names, exactWinter);
    assert(exactResult);
    assert(exactResult.value().selectedSnapshot.classAverageLetter == u"B");
    assert(exactResult.value().classShapeEvaluationName == u"Winter");
    assert(exactResult.value().classShapeSnapshot.hasData);
    assert(exactResult.value().yearToDatePoints.size() == 2);

    const auto paddedWinter =
        ClassAnalyticsEvaluationSelection::exactName(u"  Winter  ");
    assert(paddedWinter.name() == u"  Winter  ");
    const auto paddedResult = run(port, names, paddedWinter);
    assert(paddedResult);
    assert(paddedResult.value().selectedSnapshot.classAverageLetter == u"B");
    assert(paddedResult.value().classShapeEvaluationName == u"Winter");
    assert(paddedResult.value().yearToDatePoints.size() == 2);
}

void selectedEvaluationStillReadsEveryYtdInputInOneBatch()
{
    FakeReadPort port;
    port.evaluations[0].push_back(row(u"Current", u"", u"B"));
    port.evaluations[3].push_back(row(u"Current", u"", u"A"));
    const TestNameSemantics names;

    const auto result = run(port, names,
        ClassAnalyticsEvaluationSelection::exactName(u"Winter"));
    assert(result);
    assert(port.evaluationBatchReadCount == 1);
    assert((port.calls == std::vector<std::u16string>{
        u"roster", u"evaluation batch" }));
    assert(result.value().selectedSnapshot.classAverageLetter == u"B");
    assert(result.value().yearToDatePoints.size() == 2);
    assert(result.value().yearToDatePoints[0].evaluationName == u"Winter");
    assert(result.value().yearToDatePoints[1].evaluationName == u"Fall");
}

void readFailureReturnsNoPartialDashboard()
{
    FakeReadPort port;
    port.evaluations[0].push_back(row(u"Current", u"", u"A"));
    port.failEvaluationBatch = true;
    const TestNameSemantics names;

    const auto result = run(port, names);
    assert(!result);
    assert(result.error().code == Domain::ErrorCode::Technical);
    assert((port.calls == std::vector<std::u16string>{
        u"roster", u"evaluation batch" }));
    assert(port.evaluationBatchReadCount == 1);
}

void invalidClassIdDoesNotRead()
{
    const TestNameSemantics names;
    const std::vector<std::string> invalidIds = {
        "0", "007", "-1", "17x", "2147483648"
    };
    for (const std::string& rawId : invalidIds)
    {
        FakeReadPort port;
        const auto result = ClassAnalyticsDashboardReadQuery::execute(
            { .classId = *Domain::ClassId::fromString(rawId) }, port, names);
        assert(!result);
        assert(result.error().code == Domain::ErrorCode::InvalidInput);
        assert(result.error().message
            == "Class ID must be a canonical positive integer.");
        assert(!result.error().recoverable);
        assert(port.calls.empty());
        assert(port.rosterClassIds.empty());
        assert(port.evaluationBatchClassIds.empty());
    }
}

void maximumClassIdForwardsUnchangedToBothReads()
{
    FakeReadPort port;
    const TestNameSemantics names;
    const auto result = ClassAnalyticsDashboardReadQuery::execute(
        { .classId = *Domain::ClassId::fromString("2147483647") }, port, names);
    assert(result);
    assert((port.calls == std::vector<std::u16string>{
        u"roster", u"evaluation batch" }));
    assert((port.rosterClassIds == std::vector<std::string>{ "2147483647" }));
    assert((port.evaluationBatchClassIds
        == std::vector<std::string>{ "2147483647" }));
    assert(port.evaluationBatchReadCount == 1);
}

void numericPolicyBoundaries()
{
    using ClassAnalyticsDashboardReadDetail::roundAverageToGrade;
    using ClassAnalyticsDashboardReadDetail::roundTo3;

    assert(roundTo3(0.0) == 0.0);
    assert(roundTo3(5.0) == 5.0);
    assert(roundTo3(2.3995) == 2.4);
    assert(roundTo3(1.2345) == 1.235);
    assert(roundAverageToGrade(2.399) == 2);
    assert(roundAverageToGrade(2.400) == 3);
    assert(roundAverageToGrade(2.3995) == 3);
    assert(roundAverageToGrade(-1.0) == 0);
    assert(roundAverageToGrade(6.0) == 5);
    assert(ClassAnalyticsDashboardReadDetail::numberToGrade(
        roundAverageToGrade(2.400)) == u"B+");
}

void partialScoringPreservesWeightedAveragesAndYtdInputs()
{
    FakeReadPort port;
    ClassAnalyticsEvaluationRow alexFirst;
    alexFirst.englishName = u"Alex";
    alexFirst.scores[0] = u"A";
    alexFirst.scores[1] = u"B";
    ClassAnalyticsEvaluationRow alexSecond;
    alexSecond.englishName = u"Alex";
    alexSecond.scores[0] = u"C";
    port.evaluations[0] = {
        std::move(alexFirst),
        std::move(alexSecond),
        row(u"Blair", u"", u"B")
    };
    const TestNameSemantics names;

    const auto result = run(port, names,
        ClassAnalyticsEvaluationSelection::exactName(u"Winter"));
    assert(result);
    const auto& selected = result.value().selectedSnapshot;
    assert(selected.rankings.size() == 2);
    assert(selected.rankings[0].englishName == u"Alex");
    assert(selected.rankings[0].overall3.thousandths() == 2333);
    assert(!selected.rankings[0].fullyScored);
    assert(selected.rankings[1].englishName == u"Blair");
    assert(selected.rankings[1].overall3.thousandths() == 2000);
    assert(selected.rankings[1].fullyScored);

    // Criterion scores are weighted by their scored cell counts; class
    // average keeps the existing equal weighting of each student's overall.
    assert(selected.criteria[0].average3.thousandths() == 2333);
    assert(selected.criteria[1].average3.thousandths() == 2000);
    assert(selected.classAverage3.thousandths() == 2167);
    assert(selected.classAverageLetter == u"B");

    assert(result.value().yearToDatePoints.size() == 1);
    assert(result.value().yearToDatePoints[0].evaluationName == u"Winter");
    assert(result.value().yearToDatePoints[0].classAverage3
        .thousandths() == 2000);
    assert(result.value().yearToDatePoints[0].classAverageLetter == u"B");
}

void queryGradeBoundaryRoundsPositiveHalfUpAtThousandths()
{
    FakeReadPort port;
    // 399 B+ and 601 B cells average exactly 2.399; 799 B+ and 1201 B
    // cells average exactly 2.3995. Both sets span six-column rows.
    appendScoresForAverage(port.evaluations[0], u"Below", 1000, 399);
    appendScoresForAverage(port.evaluations[0], u"HalfUp", 2000, 799);
    const TestNameSemantics names;

    const auto result = run(port, names,
        ClassAnalyticsEvaluationSelection::exactName(u"Winter"));
    assert(result);
    const auto& selected = result.value().selectedSnapshot;
    assert(selected.rankings.size() == 2);
    assert(selected.rankings[0].englishName == u"HalfUp");
    assert(selected.rankings[0].overall3.thousandths() == 2400);
    assert(selected.rankings[0].overallLetter == u"B+");
    assert(selected.rankings[1].englishName == u"Below");
    assert(selected.rankings[1].overall3.thousandths() == 2399);
    assert(selected.rankings[1].overallLetter == u"B");
    assert(selected.classAverage3.thousandths() == 2400);
    assert(selected.classAverageLetter == u"B+");
}

void roundedClassAverageAtGradeBoundaryAdvances()
{
    FakeReadPort port;
    port.evaluations[0] = {
        row(u"C", u"", u"C"),
        row(u"B one", u"", u"B"),
        row(u"B two", u"", u"B"),
        row(u"B plus", u"", u"B+"),
        row(u"A", u"", u"A")
    };
    const TestNameSemantics names;

    const auto result = run(port, names,
        ClassAnalyticsEvaluationSelection::exactName(u"Winter"));
    assert(result);
    assert(result.value().selectedSnapshot.classAverage3.thousandths() == 2400);
    assert(result.value().selectedSnapshot.classAverageLetter == u"B+");
}

} // namespace

int main()
{
    averageThousandthsValueSemantics();
    evaluationSelectionDefaultsAndLegacyTextConversion();
    numericPolicyBoundaries();
    partialScoringPreservesWeightedAveragesAndYtdInputs();
    queryGradeBoundaryRoundsPositiveHalfUpAtThousandths();
    roundedClassAverageAtGradeBoundaryAdvances();
    canonicalOrderLatestClassShapeAndYtd();
    strongestAndFocusCriteriaPreserveOrderAndLabels();
    currentRosterUsesNormalizedEnglishAndKoreanIdentities();
    duplicateRowsConsolidateAndRankingTiesUseNamePort();
    adjacentRoundedScoresRankBeforeNamePort();
    emptyRosterLeavesEvaluationRowsUnfilteredAndPartialScoresDoNotMakeYtd();
    missingAndUnknownSelectionKeepGlobalYtdWhileUnknownSelectionIsEmpty();
    typedAndPaddedExactNamesSelectAfterTrimmedComparison();
    selectedEvaluationStillReadsEveryYtdInputInOneBatch();
    readFailureReturnsNoPartialDashboard();
    invalidClassIdDoesNotRead();
    maximumClassIdForwardsUnchangedToBothReads();
    return 0;
}
