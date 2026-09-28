#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <string>
#include <system_error>
#include <vector>

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationReadQuery final
{
    Domain::ClassId classId;
    std::u16string evaluationName;

    friend bool operator==(
        const SpeakingEvaluationReadQuery&,
        const SpeakingEvaluationReadQuery&
        ) = default;
};

struct SpeakingEvaluationReadSnapshot final
{
    Domain::ClassId classId;
    std::u16string evaluationName;
    std::vector<std::vector<std::u16string>> rows;

    friend bool operator==(
        const SpeakingEvaluationReadSnapshot&,
        const SpeakingEvaluationReadSnapshot&
        ) = default;
};

using SpeakingEvaluationReadResult =
    Domain::Result<SpeakingEvaluationReadSnapshot>;

class SpeakingEvaluationReadPort
{
public:
    virtual ~SpeakingEvaluationReadPort() = default;

    [[nodiscard]] virtual SpeakingEvaluationReadResult readEvaluation(
        const SpeakingEvaluationReadQuery& query
        ) const = 0;
};

class SpeakingEvaluationQuery final
{
public:
    [[nodiscard]] static SpeakingEvaluationReadResult execute(
        const SpeakingEvaluationReadQuery& query,
        const SpeakingEvaluationReadPort& port
        )
    {
        if (!isCanonicalPositiveClassId(query.classId.value()))
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."
                );
        }

        if (query.evaluationName.empty())
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Evaluation name must not be empty."
                );
        }

        SpeakingEvaluationReadResult source = port.readEvaluation(query);
        if (!source)
        {
            return SpeakingEvaluationReadResult::failure(source.error());
        }

        if (source.value().classId != query.classId
            || source.value().evaluationName != query.evaluationName)
        {
            return failure(
                Domain::ErrorCode::Validation,
                "A speaking evaluation read returned a different query identity."
                );
        }

        return source;
    }

private:
    [[nodiscard]] static SpeakingEvaluationReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return SpeakingEvaluationReadResult::failure({
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
