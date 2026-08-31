#pragma once

#include "classmngr/engine/validation_result.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

using SpeakingEvaluationRow = std::vector<std::string>;
using SpeakingEvaluationRows = std::vector<SpeakingEvaluationRow>;

class SpeakingEvaluationValidator final
{
public:
    static constexpr std::size_t MaximumRows = 25;
    static constexpr std::size_t MaximumColumns = 11;
    static constexpr std::size_t MaximumEvaluationNameLength = 128;
    static constexpr std::size_t MaximumNotesLength = 10000;
    static constexpr std::size_t CommentMaxLength = 450;

    // Maps historic score aliases to their canonical letter forms while
    // retaining unrecognized input for validate() to report.
    [[nodiscard]] static std::string normalizedScore(
        std::string_view value
        );

    // Canonicalizes each supplied row without discarding extra cells or rows.
    // Call validate() after normalization.
    [[nodiscard]] static SpeakingEvaluationRows normalized(
        const SpeakingEvaluationRows& rows
        );

    [[nodiscard]] static ValidationResult validate(
        int classId,
        std::string_view evaluationName,
        const SpeakingEvaluationRows& rows,
        bool allowQuestionableKoreanNameLengths = false
        );
};

} // namespace classmngr::engine
