#pragma once

#include "next/application/speaking_evaluation_batch_read_query.h"
#include "next/application/speaking_evaluation_roster_score_import_use_case.h"

#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

using SpeakingEvaluationRosterScoreImportBatchItems =
    std::vector<SpeakingEvaluationRosterScoreImportResult>;
using SpeakingEvaluationRosterScoreImportBatchResult =
    Domain::Result<SpeakingEvaluationRosterScoreImportBatchItems>;

class SpeakingEvaluationRosterScoreImportBatchUseCase final
{
public:
    [[nodiscard]] static SpeakingEvaluationRosterScoreImportBatchResult execute(
        const SpeakingEvaluationReadBatchQuery& query,
        const SpeakingEvaluationReadBatchPort& batchPort,
        const SpeakingEvaluationReadPort& singlePort
        )
    {
        const Domain::Result<void> valid =
            SpeakingEvaluationReadBatchQueryHandler::validate(query);
        if (!valid)
        {
            return SpeakingEvaluationRosterScoreImportBatchResult::failure(
                valid.error()
                );
        }

        SpeakingEvaluationRosterScoreImportBatchItems imports;
        imports.reserve(query.evaluationNames.size());
        if (query.evaluationNames.empty())
        {
            return SpeakingEvaluationRosterScoreImportBatchResult::success(
                std::move(imports)
                );
        }

        const SpeakingEvaluationReadBatchResult batch =
            SpeakingEvaluationReadBatchQueryHandler::execute(
                query,
                batchPort
                );
        if (batch)
        {
            for (const SpeakingEvaluationReadSnapshot& evaluation :
                 batch.value().evaluations)
            {
                imports.push_back(
                    SpeakingEvaluationRosterScoreImportResult::success(
                        SpeakingEvaluationRosterScoreImportUseCase::
                            scoresFromSnapshot(evaluation)
                        )
                    );
            }
            return SpeakingEvaluationRosterScoreImportBatchResult::success(
                std::move(imports)
                );
        }

        // A failed or malformed batch falls back per requested evaluation so
        // one single-read error cannot suppress later successful evaluations.
        for (const std::u16string& evaluationName : query.evaluationNames)
        {
            imports.push_back(
                SpeakingEvaluationRosterScoreImportUseCase::execute(
                    {
                        .classId = query.classId,
                        .evaluationName = evaluationName
                    },
                    singlePort
                    )
                );
        }

        return SpeakingEvaluationRosterScoreImportBatchResult::success(
            std::move(imports)
            );
    }
};

} // namespace ClassMngr::Next::Application
