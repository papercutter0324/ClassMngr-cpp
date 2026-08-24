#pragma once

#include "domain/models/roster.h"
#include "domain/validation/validation_result.h"

class RosterValidator final
{
public:
    static constexpr qsizetype MaximumRows = 25;
    static constexpr qsizetype MaximumColumnNameLength = 64;
    static constexpr qsizetype MaximumCellLength = 10000;

    // Canonicalizes supported roster names and values without discarding
    // malformed input. Call validate() after normalization.
    [[nodiscard]] static Roster normalized(const Roster& roster);

    [[nodiscard]] static ValidationResult validate(
        const Roster& roster,
        bool allowQuestionableKoreanNameLengths = false
        );
};
