#pragma once

#include "classmngr/engine/roster.h"
#include "classmngr/engine/validation_result.h"

#include <cstddef>

namespace classmngr::engine
{

class RosterValidator final
{
public:
    static constexpr std::size_t MaximumRows = 25;
    static constexpr std::size_t MaximumColumnNameLength = 64;
    static constexpr std::size_t MaximumCellLength = 10000;

    // Canonicalizes supported roster names and values without discarding
    // malformed input. Call validate() after normalization.
    [[nodiscard]] static Roster normalized(const Roster& roster);

    [[nodiscard]] static ValidationResult validate(
        const Roster& roster,
        bool allowQuestionableKoreanNameLengths = false
        );
};

} // namespace classmngr::engine
