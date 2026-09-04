#pragma once

#include "classmngr/engine/speaking_evaluation.h"
#include "classmngr/engine/validation_result.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

class SpeakingEvaluationValidator final
{
public:
    static constexpr std::size_t MaximumRows =
        static_cast<std::size_t>(SpeakingEvaluationRowCount);
    static constexpr std::size_t MaximumColumns =
        static_cast<std::size_t>(SpeakingEvaluationColumnCount);
    static constexpr std::size_t MaximumEvaluationNameLength =
        SpeakingEvaluationMaximumEvaluationNameLength;
    static constexpr std::size_t MaximumNotesLength =
        SpeakingEvaluationMaximumNotesLength;
    static constexpr std::size_t CommentMaxLength =
        static_cast<std::size_t>(SpeakingEvaluationCommentMaxLength);

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
