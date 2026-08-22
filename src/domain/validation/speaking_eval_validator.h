#pragma once

#include "domain/models/speaking_evaluation.h"
#include "domain/validation/validation_result.h"

class SpeakingEvalValidator final
{
public:
    static constexpr qsizetype MaximumEvaluationNameLength = 128;
    static constexpr qsizetype MaximumNotesLength = 10000;

    // Maps the historic score aliases accepted by the editor to their
    // canonical letter forms, while retaining unrecognized input for
    // validate() to report.
    [[nodiscard]] static QString normalizedScore(const QString& value);
    [[nodiscard]] static SpeakingEvalRows normalized(const SpeakingEvalRows& rows);

    [[nodiscard]] static ValidationResult validate(
        int classId,
        const QString& evaluationName,
        const SpeakingEvalRows& rows
        );
};
