#pragma once

#include "next/application/speaking_evaluation_roster_names_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
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
        if (!isCanonicalPositiveId(request.classId.value()))
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

private:
    [[nodiscard]] static bool isCanonicalPositiveId(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return false;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(), value.data() + value.size(), parsed);
        return error == std::errc{}
            && end == value.data() + value.size()
            && parsed > 0
            && std::to_string(parsed) == value;
    }
};

} // namespace ClassMngr::Next::Application
