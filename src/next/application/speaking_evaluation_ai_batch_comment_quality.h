#pragma once

#include <cstddef>

namespace ClassMngr::Next::Application
{

enum class SpeakingEvaluationAiBatchCommentRejection
{
    None,
    Empty,
    TooLong
};

struct SpeakingEvaluationAiBatchCommentQualityInput final
{
    std::size_t normalizedCodeUnitLength = 0;
    bool hadNamePlaceholder = false;
    std::size_t minimumCodeUnitLength = 0;
    std::size_t preferredMaximumCodeUnitLength = 0;
    std::size_t maximumCodeUnitLength = 0;
};

struct SpeakingEvaluationAiBatchCommentQuality final
{
    std::size_t codeUnitLength = 0;
    SpeakingEvaluationAiBatchCommentRejection rejection =
        SpeakingEvaluationAiBatchCommentRejection::None;
    bool outsidePreferredLength = false;
    bool missingNamePlaceholder = false;

    [[nodiscard]] bool isValid() const noexcept
    {
        return rejection == SpeakingEvaluationAiBatchCommentRejection::None;
    }
};

[[nodiscard]] inline SpeakingEvaluationAiBatchCommentQuality
assessSpeakingEvaluationAiBatchCommentQuality(
    const SpeakingEvaluationAiBatchCommentQualityInput& input
    ) noexcept
{
    using Rejection = SpeakingEvaluationAiBatchCommentRejection;
    SpeakingEvaluationAiBatchCommentQuality result;
    result.codeUnitLength = input.normalizedCodeUnitLength;

    if (input.normalizedCodeUnitLength == 0)
    {
        result.rejection = Rejection::Empty;
        return result;
    }
    if (input.normalizedCodeUnitLength > input.maximumCodeUnitLength)
    {
        result.rejection = Rejection::TooLong;
        return result;
    }

    result.outsidePreferredLength =
        input.normalizedCodeUnitLength < input.minimumCodeUnitLength
        || input.normalizedCodeUnitLength
            > input.preferredMaximumCodeUnitLength;
    result.missingNamePlaceholder = !input.hadNamePlaceholder;
    return result;
}

} // namespace ClassMngr::Next::Application
