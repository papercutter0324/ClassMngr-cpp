#include "next/application/speaking_evaluation_roster_score_import_use_case.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class FakeSpeakingEvaluationReadPort final : public SpeakingEvaluationReadPort
{
public:
    [[nodiscard]] SpeakingEvaluationReadResult readEvaluation(
        const SpeakingEvaluationReadQuery& query
        ) const override
    {
        requests.push_back(query);
        return result;
    }

    mutable std::vector<SpeakingEvaluationReadQuery> requests;
    SpeakingEvaluationReadResult result =
        SpeakingEvaluationReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The fake speaking-evaluation port was not configured.",
            .recoverable = false
        });
};

SpeakingEvaluationReadQuery query()
{
    return {
        .classId = *Domain::ClassId::fromString("42"),
        .evaluationName = u"Winter"
    };
}

SpeakingEvaluationReadSnapshot snapshot(
    const SpeakingEvaluationReadQuery& request,
    std::vector<std::vector<std::u16string>> rows
    )
{
    return {
        .classId = request.classId,
        .evaluationName = request.evaluationName,
        .rows = std::move(rows)
    };
}

std::vector<std::u16string> scoreRow(
    std::u16string english,
    std::u16string korean,
    const std::array<std::u16string, 6>& grades
    )
{
    std::vector<std::u16string> row(11);
    row[1] = std::move(english);
    row[2] = std::move(korean);
    for (std::size_t index = 0; index < grades.size(); ++index)
    {
        row[3 + index] = grades[index];
    }
    return row;
}

bool expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
    }
    return condition;
}

bool convertsTrimmedNamesAndRoundsGradesInSourceOrder()
{
    const SpeakingEvaluationReadQuery requested = query();
    FakeSpeakingEvaluationReadPort port;
    port.result = SpeakingEvaluationReadResult::success(snapshot(
        requested,
        {
            scoreRow(
                u"\u3000Alex\u00a0",
                u"\u00a0\uAE40\uBBFC\uC9C0\u3000",
                {u"B+", u"B+", u"B", u"B", u"B", u"B"}
                ),
            scoreRow(
                u"Riley",
                u"\uAE40\uD558\uB298",
                {u"B+", u"B+", u"B+", u"B", u"B", u"B"}
                ),
            scoreRow(
                u" Alex ",
                u" \uAE40\uBBFC\uC9C0 ",
                {u"A+", u"A+", u"A+", u"A+", u"A+", u"A+"}
                )
        }
        ));

    const auto result = SpeakingEvaluationRosterScoreImportUseCase::execute(
        requested,
        port
        );

    if (!expect(result.hasValue(), "A successful read must produce scores.")
        || !expect(port.requests.size() == 1,
                   "The use case must perform exactly one typed read.")
        || !expect(port.requests.front() == requested,
                   "The use case must preserve the typed query.")
        || !expect(result.value().size() == 3,
                   "Every complete source row must remain in source order.")
        || !expect(result.value()[0].englishName == u"Alex",
                   "English names must use QString-compatible trimming.")
        || !expect(result.value()[0].koreanName == u"\uAE40\uBBFC\uC9C0",
                   "Korean names must use QString-compatible trimming.")
        || !expect(result.value()[0].finalGrade == u"B",
                   "A six-component average below 0.4 must round down.")
        || !expect(result.value()[1].englishName == u"Riley",
                   "Score records must preserve source row ordering.")
        || !expect(result.value()[1].finalGrade == u"B+",
                   "A six-component average at 0.5 must round up.")
        || !expect(result.value()[2].englishName == u"Alex",
                   "Duplicate imported names must remain represented in order.")
        || !expect(result.value()[2].finalGrade == u"A+",
                   "The later duplicate must retain its derived grade.")
        )
    {
        return false;
    }

    return true;
}

bool skipsMalformedRowsAndReturnsNotApplicableForInvalidGrades()
{
    const SpeakingEvaluationReadQuery requested = query();
    FakeSpeakingEvaluationReadPort port;
    auto shortRow = scoreRow(
        u"Short",
        u"\uC9E7\uC740\uD589",
        {u"A+", u"A+", u"A+", u"A+", u"A+", u"A+"}
        );
    shortRow.resize(10);

    auto missingKorean = scoreRow(
        u"No Korean",
        u"",
        {u"A", u"A", u"A", u"A", u"A", u"A"}
        );
    auto missingEnglish = scoreRow(
        u"",
        u"\uC601\uC5B4 \uC774\uB984 \uC5C6\uC74C",
        {u"A", u"A", u"A", u"A", u"A", u"A"}
        );
    auto unknownGrade = scoreRow(
        u"Unknown",
        u"\uC54C\uC9C0\uBABB\uD568",
        {u"A", u"A", u"?", u"A", u"A", u"A"}
        );
    auto incompleteGrade = scoreRow(
        u"Incomplete",
        u"\uBD80\uC871",
        {u"B+", u"B+", u"B+", u"B+", u"B+", u" \u00a0 "}
        );
    auto malformedGrade = scoreRow(
        u"Malformed",
        u"\uC798\uBABB\uB41C \uB4F1\uAE09",
        {u"A++", u"A", u"A", u"A", u"A", u"A"}
        );

    port.result = SpeakingEvaluationReadResult::success(snapshot(
        requested,
        {
            std::move(shortRow),
            std::move(missingKorean),
            std::move(missingEnglish),
            std::move(unknownGrade),
            std::move(incompleteGrade),
            std::move(malformedGrade)
        }
        ));

    const auto result = SpeakingEvaluationRosterScoreImportUseCase::execute(
        requested,
        port
        );

    return expect(result.hasValue(), "A valid snapshot must succeed.")
        && expect(result.value().size() == 3,
                  "Short rows and incomplete names must be skipped.")
        && expect(result.value()[0].englishName == u"Unknown",
                  "The first accepted row must keep its source position.")
        && expect(result.value()[0].finalGrade == u"N/A",
                  "Unknown grades must import as N/A.")
        && expect(result.value()[1].englishName == u"Incomplete",
                  "The next accepted row must keep its source position.")
        && expect(result.value()[1].finalGrade == u"N/A",
                  "Incomplete grades must import as N/A.")
        && expect(result.value()[2].englishName == u"Malformed",
                  "Malformed grades must keep the associated name pair.")
        && expect(result.value()[2].finalGrade == u"N/A",
                  "Malformed grades must import as N/A.");
}

bool propagatesReadFailuresAndRejectsIdentityMismatch()
{
    const SpeakingEvaluationReadQuery requested = query();
    FakeSpeakingEvaluationReadPort port;
    const Domain::OperationError readError{
        .code = Domain::ErrorCode::Technical,
        .message = "evaluation read failed",
        .recoverable = true
    };
    port.result = SpeakingEvaluationReadResult::failure(readError);

    auto result = SpeakingEvaluationRosterScoreImportUseCase::execute(
        requested,
        port
        );
    if (!expect(!result, "Read failures must fail the import use case.")
        || !expect(result.error() == readError,
                   "Read failures must preserve the original error.")
        )
    {
        return false;
    }

    port.result = SpeakingEvaluationReadResult::success({
        .classId = *Domain::ClassId::fromString("43"),
        .evaluationName = requested.evaluationName,
        .rows = {}
    });
    result = SpeakingEvaluationRosterScoreImportUseCase::execute(
        requested,
        port
        );
    return expect(!result, "A mismatched read identity must fail.")
        && expect(result.error().code == Domain::ErrorCode::Validation,
                  "A mismatched read identity must be a validation error.");
}

bool preservesAnEmptySuccessfulRead()
{
    const SpeakingEvaluationReadQuery requested = query();
    FakeSpeakingEvaluationReadPort port;
    port.result = SpeakingEvaluationReadResult::success(
        snapshot(requested, {})
        );

    const auto result = SpeakingEvaluationRosterScoreImportUseCase::execute(
        requested,
        port
        );
    return expect(result.hasValue(), "An empty successful read must succeed.")
        && expect(result.value().empty(),
                  "An empty successful read must produce no scores.");
}

} // namespace

int main()
{
    return convertsTrimmedNamesAndRoundsGradesInSourceOrder()
            && skipsMalformedRowsAndReturnsNotApplicableForInvalidGrades()
            && propagatesReadFailuresAndRejectsIdentityMismatch()
            && preservesAnEmptySuccessfulRead()
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
