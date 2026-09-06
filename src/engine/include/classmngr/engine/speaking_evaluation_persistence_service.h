#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/speaking_evaluation.h"

#include <string_view>
#include <vector>

namespace classmngr::engine
{

class SqliteDatabase;

class SpeakingEvaluationPersistenceService final
{
public:
    explicit SpeakingEvaluationPersistenceService(
        SqliteDatabase& database
        );

    [[nodiscard]] Status save(
        int classId,
        std::string_view evaluationName,
        const SpeakingEvaluationRows& rows,
        const std::vector<SpeakingEvaluationCellChange>& dirtyCells = {}
        );

    [[nodiscard]] Result<SpeakingEvaluationRows> load(
        int classId,
        std::string_view evaluationName
        );

    [[nodiscard]] Result<std::vector<SpeakingEvaluationScore>>
    buildRosterScoreImport(
        int classId,
        std::string_view evaluationName
        );

private:
    SqliteDatabase& m_database;
};

} // namespace classmngr::engine
