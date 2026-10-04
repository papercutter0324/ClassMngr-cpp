#pragma once

#include "next/application/speaking_evaluation_roster_names_read_snapshot.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationRosterNamesReadRequest final
{
    Domain::ClassId classId;
};

using SpeakingEvaluationRosterNamesReadResult =
    Domain::Result<SpeakingEvaluationRosterNamesReadSnapshot>;

class SpeakingEvaluationRosterNamesReadPort
{
public:
    virtual ~SpeakingEvaluationRosterNamesReadPort() = default;

    [[nodiscard]] virtual SpeakingEvaluationRosterNamesReadResult
    readSpeakingEvaluationRosterNames(
        const SpeakingEvaluationRosterNamesReadRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
