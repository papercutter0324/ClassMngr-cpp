#include "next/application/speaking_evaluation_roster_score_import_use_case.h"
#include "next/application/speaking_evaluation_roster_score_import_batch_use_case.h"

#include <array>
#include <cstdlib>
#include <functional>
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
        if (responder)
        {
            return responder(query);
        }
        return result;
    }

    mutable std::vector<SpeakingEvaluationReadQuery> requests;
    std::function<SpeakingEvaluationReadResult(
        const SpeakingEvaluationReadQuery&)> responder;
    SpeakingEvaluationReadResult result =
        SpeakingEvaluationReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The fake speaking-evaluation port was not configured.",
            .recoverable = false
        });
};

class FakeSpeakingEvaluationReadBatchPort final
    : public SpeakingEvaluationReadBatchPort
{
public:
    [[nodiscard]] SpeakingEvaluationReadBatchResult readEvaluations(
        const SpeakingEvaluationReadBatchQuery& query
        ) const override
    {
        requests.push_back(query);
        return result;
    }

    mutable std::vector<SpeakingEvaluationReadBatchQuery> requests;
    SpeakingEvaluationReadBatchResult result =
        SpeakingEvaluationReadBatchResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The fake speaking evaluation batch was not configured.",
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

SpeakingEvaluationReadBatchQuery batchQuery(
    std::vector<std::u16string> names
    )
{
    return {
        .classId = *Domain::ClassId::fromString("42"),
        .evaluationNames = std::move(names)
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

bool importsOrderedBatchResultsIncludingEmptyEvaluations()
{
    const SpeakingEvaluationReadBatchQuery requested = batchQuery({
        u"Winter", u"Speech Contest", u"Summer"
    });
    FakeSpeakingEvaluationReadBatchPort batchPort;
    batchPort.result = SpeakingEvaluationReadBatchResult::success({
        .evaluations = {
            snapshot(
                { .classId = requested.classId, .evaluationName = u"Winter" },
                { scoreRow(
                    u" Winter Student ",
                    u" \uAE40\uACA8\uC6B8 ",
                    {u"A+", u"A+", u"A+", u"A+", u"A+", u"A+"}
                    ) }
                ),
            snapshot(
                { .classId = requested.classId, .evaluationName = u"Speech Contest" },
                {}
                ),
            snapshot(
                { .classId = requested.classId, .evaluationName = u"Summer" },
                {}
                )
        }
    });
    FakeSpeakingEvaluationReadPort singlePort;

    const auto result = SpeakingEvaluationRosterScoreImportBatchUseCase::execute(
        requested,
        batchPort,
        singlePort
        );

    return expect(result.hasValue(), "A complete batch must import successfully.")
        && expect(batchPort.requests.size() == 1,
                  "All requested evaluations must use one batch read.")
        && expect(batchPort.requests.front() == requested,
                  "The batch must preserve the requested fixed order.")
        && expect(singlePort.requests.empty(),
                  "Successful batch reads must not use the single-read fallback.")
        && expect(result.value().size() == 3,
                  "Each requested evaluation must retain its own result slot.")
        && expect(result.value()[0].hasValue()
                      && result.value()[0].value().size() == 1,
                  "The first ordered evaluation must keep its score rows.")
        && expect(result.value()[0].value()[0].englishName == u"Winter Student",
                  "The existing score parser must be reused for batch rows.")
        && expect(result.value()[1].hasValue()
                      && result.value()[1].value().empty(),
                  "A missing evaluation must remain a successful empty result.")
        && expect(result.value()[2].hasValue()
                      && result.value()[2].value().empty(),
                  "An evaluation without data rows must remain empty.");
}

bool emptyBatchSkipsBatchAndSinglePorts()
{
    FakeSpeakingEvaluationReadBatchPort batchPort;
    FakeSpeakingEvaluationReadPort singlePort;

    const auto result = SpeakingEvaluationRosterScoreImportBatchUseCase::execute(
        batchQuery({}),
        batchPort,
        singlePort
        );

    return expect(result.hasValue(), "An empty request is a successful no-op.")
        && expect(result.value().empty(), "An empty request has no result items.")
        && expect(batchPort.requests.empty(),
                  "An empty request must not invoke the batch adapter.")
        && expect(singlePort.requests.empty(),
                  "An empty request must not invoke single-read fallback.");
}

bool batchQueryValidatesRequestAndReturnedOrder()
{
    FakeSpeakingEvaluationReadBatchPort batchPort;
    const auto invalidRequest = SpeakingEvaluationReadBatchQueryHandler::execute(
        batchQuery({u"Winter", {}}),
        batchPort
        );
    if (!expect(!invalidRequest,
                "Empty evaluation names must fail batch validation.")
        || !expect(invalidRequest.error().code == Domain::ErrorCode::InvalidInput,
                   "Invalid batch names must report InvalidInput.")
        || !expect(batchPort.requests.empty(),
                   "Invalid requests must not reach the adapter."))
    {
        return false;
    }

    const SpeakingEvaluationReadBatchQuery requested = batchQuery({
        u"Winter", u"Summer"
    });
    batchPort.result = SpeakingEvaluationReadBatchResult::success({
        .evaluations = {
            snapshot(
                { .classId = requested.classId, .evaluationName = u"Summer" },
                {}
                ),
            snapshot(
                { .classId = requested.classId, .evaluationName = u"Winter" },
                {}
                )
        }
    });
    const auto misordered = SpeakingEvaluationReadBatchQueryHandler::execute(
        requested,
        batchPort
        );
    return expect(!misordered,
                  "Misordered batch responses must be rejected.")
        && expect(misordered.error().code == Domain::ErrorCode::Validation,
                  "A response order mismatch must be a validation failure.");
}

bool batchFailureFallsBackIndependentlyInOriginalOrder()
{
    const SpeakingEvaluationReadBatchQuery requested = batchQuery({
        u"Winter", u"Speech Contest", u"Summer"
    });
    FakeSpeakingEvaluationReadBatchPort batchPort;
    batchPort.result = SpeakingEvaluationReadBatchResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "batch unavailable",
        .recoverable = true
    });
    FakeSpeakingEvaluationReadPort singlePort;
    singlePort.responder = [](const SpeakingEvaluationReadQuery& query)
    {
        if (query.evaluationName == u"Winter")
        {
            return SpeakingEvaluationReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Winter unavailable",
                .recoverable = true
            });
        }

        return SpeakingEvaluationReadResult::success(snapshot(
            query,
            { scoreRow(
                query.evaluationName == u"Speech Contest"
                    ? u"Speech Student"
                    : u"Summer Student",
                u"\uAE40\uD559\uC0DD",
                {u"B+", u"B+", u"B+", u"B+", u"B+", u"B+"}
                ) }
            ));
    };

    const auto result = SpeakingEvaluationRosterScoreImportBatchUseCase::execute(
        requested,
        batchPort,
        singlePort
        );

    return expect(result.hasValue(),
                  "Single-read failures must remain isolated result items.")
        && expect(batchPort.requests.size() == 1,
                  "The failed batch must be attempted once.")
        && expect(singlePort.requests.size() == 3,
                  "Fallback must attempt every requested evaluation.")
        && expect(singlePort.requests[0].evaluationName == u"Winter"
                      && singlePort.requests[1].evaluationName == u"Speech Contest"
                      && singlePort.requests[2].evaluationName == u"Summer",
                  "Fallback must preserve the original destination order.")
        && expect(result.value().size() == 3,
                  "Fallback must produce one result for every requested item.")
        && expect(!result.value()[0],
                  "A failed first evaluation must remain failed on its own.")
        && expect(result.value()[1]
                      && result.value()[1].value()[0].englishName == u"Speech Student",
                  "A later evaluation must still import after an earlier failure.")
        && expect(result.value()[2]
                      && result.value()[2].value()[0].englishName == u"Summer Student",
                  "Fallback must continue after successful siblings.");
}

} // namespace

int main()
{
    return convertsTrimmedNamesAndRoundsGradesInSourceOrder()
            && skipsMalformedRowsAndReturnsNotApplicableForInvalidGrades()
            && propagatesReadFailuresAndRejectsIdentityMismatch()
            && preservesAnEmptySuccessfulRead()
            && importsOrderedBatchResultsIncludingEmptyEvaluations()
            && emptyBatchSkipsBatchAndSinglePorts()
            && batchQueryValidatesRequestAndReturnedOrder()
            && batchFailureFallsBackIndependentlyInOriginalOrder()
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
