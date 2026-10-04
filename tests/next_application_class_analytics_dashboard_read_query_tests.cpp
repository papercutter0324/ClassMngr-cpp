#include "next/application/class_analytics_dashboard_read_query.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
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
        std::size_t first = 0;
        std::size_t last = value.size();
        while (first < last && value[first] == u' ') ++first;
        while (last > first && value[last - 1] == u' ') --last;
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
    mutable int evaluationBatchReadCount = 0;
    bool failEvaluationBatch = false;

    [[nodiscard]] Domain::Result<ClassAnalyticsRosterNames> readRosterNames(
        const Domain::ClassId& classId
        ) const override
    {
        (void)classId;
        calls.push_back(u"roster");
        return Domain::Result<ClassAnalyticsRosterNames>::success(roster);
    }

    [[nodiscard]] Domain::Result<ClassAnalyticsEvaluationBatch>
    readEvaluationBatch(
        const Domain::ClassId& classId
        ) const override
    {
        (void)classId;
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
    const TestNameSemantics& names,
    const std::u16string_view selection = {}
    )
{
    return ClassAnalyticsDashboardReadQuery::execute(
        { .classId = classId(), .evaluationSelection = std::u16string(selection) },
        port, names);
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
    assert(result.value().selectedSnapshot.rankings.front().overall3 == 2.615);
    assert(result.value().classShapeEvaluationName == u"Speech Contest");
    assert(result.value().classShapeSnapshot.fullyScoredCount == 1);
    assert(result.value().yearToDatePoints.size() == 2);
    assert(result.value().yearToDatePoints[0].evaluationName == u"Winter");
    assert(result.value().yearToDatePoints[1].evaluationName == u"Speech Contest");
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

    const auto result = run(port, names, u"Winter");
    assert(result);
    const auto& selected = result.value().selectedSnapshot;
    assert(selected.rankings.size() == 2);
    assert(selected.rankings[0].koreanName == u"김민수(A)");
    assert(selected.rankings[1].englishName == u"Mary-Jane");
    assert(result.value().yearToDatePoints.size() == 1);
    // Historical YTD is global and still includes the Former row.
    assert(result.value().yearToDatePoints.front().classAverage3 > 3.0);
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

    const auto result = run(port, names, u"Winter");
    assert(result);
    const auto& selected = result.value().selectedSnapshot;
    assert(selected.rankings.size() == 2);
    assert(selected.rankings[0].englishName == u"Bee");
    assert(selected.rankings[1].englishName == u"J. P. Kim");
    assert(selected.rankings[0].overall3 == 3.0);
    assert(selected.rankings[1].overall3 == 3.0);
}

void emptyRosterLeavesEvaluationRowsUnfilteredAndPartialScoresDoNotMakeYtd()
{
    FakeReadPort port;
    port.roster.rowCount = 0;
    port.evaluations[3].push_back(row(u"Former", u"", u"A", false));
    const TestNameSemantics names;

    const auto result = run(port, names, u"Fall");
    assert(result);
    assert(result.value().selectedSnapshot.rankings.size() == 1);
    assert(!result.value().selectedSnapshot.rankings.front().fullyScored);
    assert(result.value().yearToDatePoints.empty());
}

void missingAndUnknownSelectionKeepGlobalYtdWhileUnknownSelectionIsEmpty()
{
    FakeReadPort port;
    // Missing Winter/Speech Contest/Summer rows are successful empty inputs.
    port.evaluations[3].push_back(row(u"Current", u"", u"A"));
    const TestNameSemantics names;

    const auto result = run(port, names, u"Unknown selection");
    assert(result);
    assert(!result.value().selectedSnapshot.hasData);
    assert(!result.value().classShapeSnapshot.hasData);
    assert(result.value().classShapeEvaluationName.empty());
    assert(result.value().yearToDatePoints.size() == 1);
    assert(result.value().yearToDatePoints.front().evaluationName == u"Fall");
}

void selectedEvaluationStillReadsEveryYtdInputInOneBatch()
{
    FakeReadPort port;
    port.evaluations[0].push_back(row(u"Current", u"", u"B"));
    port.evaluations[3].push_back(row(u"Current", u"", u"A"));
    const TestNameSemantics names;

    const auto result = run(port, names, u"Winter");
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
    FakeReadPort port;
    const TestNameSemantics names;
    const auto invalidId = *Domain::ClassId::fromString("007");
    const auto result = ClassAnalyticsDashboardReadQuery::execute(
        { .classId = invalidId }, port, names);
    assert(!result);
    assert(result.error().code == Domain::ErrorCode::InvalidInput);
    assert(port.calls.empty());
}

} // namespace

int main()
{
    canonicalOrderLatestClassShapeAndYtd();
    currentRosterUsesNormalizedEnglishAndKoreanIdentities();
    duplicateRowsConsolidateAndRankingTiesUseNamePort();
    emptyRosterLeavesEvaluationRowsUnfilteredAndPartialScoresDoNotMakeYtd();
    missingAndUnknownSelectionKeepGlobalYtdWhileUnknownSelectionIsEmpty();
    selectedEvaluationStillReadsEveryYtdInputInOneBatch();
    readFailureReturnsNoPartialDashboard();
    invalidClassIdDoesNotRead();
    return 0;
}
