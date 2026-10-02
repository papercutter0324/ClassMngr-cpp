#pragma once

#include "domain/models/speaking_evaluation.h"
#include "domain/validation/validation_result.h"
#include "next/application/speaking_evaluation_validation.h"

#include <QList>
#include <QString>

namespace SpeakingEvalPageValidationAdapter
{

[[nodiscard]] ClassMngr::Next::Application::SpeakingEvaluationSaveRequest
makeSaveRequest(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& changedCells,
    bool allowQuestionableKoreanNameLengths
    );

[[nodiscard]] ValidationResult toFormValidation(
    const ClassMngr::Next::Application::SpeakingEvaluationValidationResult& result
    );

} // namespace SpeakingEvalPageValidationAdapter
