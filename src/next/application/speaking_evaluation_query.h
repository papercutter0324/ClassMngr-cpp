#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/application/qt_compatible_text.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>
#include <utility>
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
        if (!isCanonicalPositiveDecimalId(query.classId.value()))
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."
                );
        }

        if (trimQtWhitespace(query.evaluationName).empty())
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
};

} // namespace ClassMngr::Next::Application
