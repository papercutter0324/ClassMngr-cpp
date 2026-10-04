#pragma once

#include "next/application/speaking_evaluation_query.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationReadBatchQuery final
{
    Domain::ClassId classId;
    std::vector<std::u16string> evaluationNames;

    friend bool operator==(
        const SpeakingEvaluationReadBatchQuery&,
        const SpeakingEvaluationReadBatchQuery&
        ) = default;
};

struct SpeakingEvaluationReadBatchSnapshot final
{
    std::vector<SpeakingEvaluationReadSnapshot> evaluations;

    friend bool operator==(
        const SpeakingEvaluationReadBatchSnapshot&,
        const SpeakingEvaluationReadBatchSnapshot&
        ) = default;
};

using SpeakingEvaluationReadBatchResult =
    Domain::Result<SpeakingEvaluationReadBatchSnapshot>;

class SpeakingEvaluationReadBatchPort
{
public:
    virtual ~SpeakingEvaluationReadBatchPort() = default;

    [[nodiscard]] virtual SpeakingEvaluationReadBatchResult readEvaluations(
        const SpeakingEvaluationReadBatchQuery& query
        ) const = 0;
};

class SpeakingEvaluationReadBatchQueryHandler final
{
public:
    [[nodiscard]] static Domain::Result<void> validate(
        const SpeakingEvaluationReadBatchQuery& query
        )
    {
        if (!isCanonicalPositiveClassId(query.classId.value()))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        for (const std::u16string& name : query.evaluationNames)
        {
            if (name.empty())
            {
                return Domain::Result<void>::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "Evaluation names must not be empty.",
                    .recoverable = true
                });
            }
        }

        return Domain::Result<void>::success();
    }

    [[nodiscard]] static SpeakingEvaluationReadBatchResult execute(
        const SpeakingEvaluationReadBatchQuery& query,
        const SpeakingEvaluationReadBatchPort& port
        )
    {
        const Domain::Result<void> valid = validate(query);
        if (!valid)
        {
            return SpeakingEvaluationReadBatchResult::failure(valid.error());
        }

        if (query.evaluationNames.empty())
        {
            return SpeakingEvaluationReadBatchResult::success({});
        }

        SpeakingEvaluationReadBatchResult source = port.readEvaluations(query);
        if (!source)
        {
            return SpeakingEvaluationReadBatchResult::failure(source.error());
        }

        if (source.value().evaluations.size() != query.evaluationNames.size())
        {
            return failure(
                Domain::ErrorCode::Validation,
                "A speaking evaluation batch returned an unexpected result count."
                );
        }

        for (std::size_t index = 0; index < query.evaluationNames.size(); ++index)
        {
            const SpeakingEvaluationReadSnapshot& evaluation =
                source.value().evaluations[index];
            if (evaluation.classId != query.classId
                || evaluation.evaluationName != query.evaluationNames[index])
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "A speaking evaluation batch returned a different query "
                    "identity or order."
                    );
            }
        }

        return source;
    }

private:
    [[nodiscard]] static SpeakingEvaluationReadBatchResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return SpeakingEvaluationReadBatchResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::InvalidInput
        });
    }

    [[nodiscard]] static bool isCanonicalPositiveClassId(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return false;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        return error == std::errc{}
            && end == value.data() + value.size()
            && parsed > 0
            && std::to_string(parsed) == value;
    }
};

} // namespace ClassMngr::Next::Application
