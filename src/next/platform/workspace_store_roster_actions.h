#pragma once

#include "next/application/roster_save_use_case.h"
#include "next/application/roster_row_transfer_use_case.h"
#include "next/application/roster_transfer_target_choices.h"
#include "next/application/roster_window_read_query.h"
#include "next/application/speaking_evaluation_roster_score_import_batch_use_case.h"

#include <QString>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] Application::RosterWindowReadResult readRosterWindowAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId
    );

[[nodiscard]] Domain::Result<void> saveRosterAtPath(
    const QString& databasePath,
    const Application::RosterSaveRequest& request
    );

[[nodiscard]] Domain::Result<bool> rosterColumnHasTailDataAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId,
    const std::u16string& columnName
    );

[[nodiscard]] Application::SpeakingEvaluationRosterScoreImportBatchResult
readSpeakingEvaluationRosterScoresAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId,
    const std::vector<std::u16string>& evaluationNames
    );

[[nodiscard]] Application::RosterTransferTargetChoicesResult
readRosterTransferTargetChoicesAtPath(
    const QString& databasePath,
    const Domain::ClassId& sourceClassId
    );

[[nodiscard]] Domain::Result<void> saveRosterTransferAtPath(
    const QString& databasePath,
    const Application::RosterRowTransferSaveRequest& request
    );

} // namespace ClassMngr::Next::Platform
