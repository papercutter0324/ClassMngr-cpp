#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include "next/application/speaking_evaluation_validation.h"

#include <string>
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
        if (!isCanonicalPositiveDecimalId(request.classId.value()))
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
            if (change.row.rawValue() < 0
                || change.row.rawValue() >= SpeakingEvaluationRowCount
                || change.column.rawValue() < 0
                || change.column.rawValue() >= SpeakingEvaluationColumnCount)
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
            return failure(
                "Speaking evaluation content is invalid.",
                Domain::ErrorCode::Validation
                );
        }

        return port.saveEvaluation(validation.normalizedRequest);
    }

private:
    [[nodiscard]] static Domain::Result<void> failure(
        std::string message,
        const Domain::ErrorCode code = Domain::ErrorCode::InvalidInput
        )
    {
        return Domain::Result<void>::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = true
        });
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
