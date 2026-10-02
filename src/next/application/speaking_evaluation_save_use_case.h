#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/application/speaking_evaluation_validation.h"

#include <charconv>
#include <string>
#include <system_error>
#include <vector>

namespace ClassMngr::Next::Application
{

class SpeakingEvaluationSavePort
{
public:
    virtual ~SpeakingEvaluationSavePort() = default;

    [[nodiscard]] virtual Domain::Result<void> saveEvaluation(
        const SpeakingEvaluationSaveRequest& request
        ) const = 0;
};

class SpeakingEvaluationSaveUseCase final
{
public:
    [[nodiscard]] static Domain::Result<void> execute(
        const SpeakingEvaluationSaveRequest& request,
        const SpeakingEvaluationSavePort& port
        )
    {
        if (!isCanonicalPositiveClassId(request.classId.value()))
        {
            return failure("Class ID must be a canonical positive integer.");
        }

        if (!hasCompleteMatrix(request.evaluation.rows))
        {
            return failure(
                "Speaking evaluation data must contain exactly 25 rows and 11 cells per row."
                );
        }

        for (const SpeakingEvaluationCellChange& change
             : request.evaluation.changedCells)
        {
            if (change.row < 0
                || change.row >= SpeakingEvaluationRowCount
                || change.column < 0
                || change.column >= SpeakingEvaluationColumnCount)
            {
                return failure(
                    "Changed-cell coordinates must identify a cell in the speaking evaluation matrix."
                    );
            }
        }

        const SpeakingEvaluationValidationResult validation =
            validateAndNormalizeSpeakingEvaluation(request);
        if (validation.hasErrors())
        {
            return failure("Speaking evaluation content is invalid.");
        }

        return port.saveEvaluation(validation.normalizedRequest);
    }

private:
    [[nodiscard]] static Domain::Result<void> failure(std::string message)
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = std::move(message),
            .recoverable = true
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

    [[nodiscard]] static bool hasCompleteMatrix(
        const std::vector<std::vector<std::u16string>>& rows
        )
    {
        if (rows.size() != static_cast<std::size_t>(SpeakingEvaluationRowCount))
        {
            return false;
        }

        for (const std::vector<std::u16string>& row : rows)
        {
            if (row.size()
                != static_cast<std::size_t>(SpeakingEvaluationColumnCount))
            {
                return false;
            }
        }

        return true;
    }
};

} // namespace ClassMngr::Next::Application
