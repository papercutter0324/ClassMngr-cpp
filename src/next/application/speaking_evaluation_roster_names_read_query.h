#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/application/speaking_evaluation_roster_names_read_port.h"

#include <utility>

namespace ClassMngr::Next::Application
{

class SpeakingEvaluationRosterNamesReadQuery final
{
public:
    [[nodiscard]] static SpeakingEvaluationRosterNamesReadResult execute(
        const SpeakingEvaluationRosterNamesReadRequest& request,
        const SpeakingEvaluationRosterNamesReadPort& port
        )
    {
        if (!isCanonicalPositiveDecimalId(request.classId.value()))
        {
            return SpeakingEvaluationRosterNamesReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = false
            });
        }

        auto source = port.readSpeakingEvaluationRosterNames(request);
        if (!source)
        {
            return SpeakingEvaluationRosterNamesReadResult::failure(
                source.error());
        }

        auto snapshot = std::move(source.value());
        if (snapshot.classId != request.classId)
        {
            return SpeakingEvaluationRosterNamesReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "Roster names were returned for a different class.",
                .recoverable = false
            });
        }

        return SpeakingEvaluationRosterNamesReadResult::success(
            std::move(snapshot));
    }
};

} // namespace ClassMngr::Next::Application
