#include "next/application/speaking_evaluation_ai_batch_comment_quality.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchCommentQualityInput;
using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchCommentRejection;
using ClassMngr::Next::Application::
    assessSpeakingEvaluationAiBatchCommentQuality;

namespace
{

auto assess(
    const std::size_t length,
    const bool hadNamePlaceholder
    )
{
    return assessSpeakingEvaluationAiBatchCommentQuality({
        .normalizedCodeUnitLength = length,
        .hadNamePlaceholder = hadNamePlaceholder,
        .minimumCodeUnitLength = 100,
        .preferredMaximumCodeUnitLength = 420,
        .maximumCodeUnitLength = 450
    });
}

bool emptyCommentIsInvalidWithoutWarnings()
{
    const auto quality = assess(0, true);
    return !quality.isValid()
        && quality.rejection == SpeakingEvaluationAiBatchCommentRejection::Empty
        && !quality.outsidePreferredLength
        && !quality.missingNamePlaceholder;
}

bool minimumLengthBoundaryUsesConfiguredLimit()
{
    const auto belowMinimum = assess(99, true);
    const auto atMinimum = assess(100, true);
    return belowMinimum.isValid()
        && belowMinimum.outsidePreferredLength
        && atMinimum.isValid()
        && !atMinimum.outsidePreferredLength;
}

bool preferredLengthBoundaryWarnsOnlyAboveFourTwenty()
{
    const auto atPreferredLimit = assess(420, true);
    const auto abovePreferredLimit = assess(421, true);
    return atPreferredLimit.isValid()
        && !atPreferredLimit.outsidePreferredLength
        && abovePreferredLimit.isValid()
        && abovePreferredLimit.outsidePreferredLength;
}

bool maximumLengthBoundaryRejectsOnlyAfterConfiguredLimit()
{
    const auto atMaximum = assess(450, true);
    const auto aboveMaximum = assess(451, true);
    return atMaximum.isValid()
        && atMaximum.outsidePreferredLength
        && !aboveMaximum.isValid()
        && aboveMaximum.rejection
            == SpeakingEvaluationAiBatchCommentRejection::TooLong
        && !aboveMaximum.outsidePreferredLength
        && !aboveMaximum.missingNamePlaceholder;
}

bool placeholderAndCombinedWarningsAreReportedSeparately()
{
    const auto missingPlaceholder = assess(100, false);
    const auto combinedWarnings = assess(99, false);
    return missingPlaceholder.isValid()
        && !missingPlaceholder.outsidePreferredLength
        && missingPlaceholder.missingNamePlaceholder
        && combinedWarnings.isValid()
        && combinedWarnings.outsidePreferredLength
        && combinedWarnings.missingNamePlaceholder;
}

bool supplementaryUnicodeUsesUtf16CodeUnitLength()
{
    std::u16string text(98, u'x');
    text += u"\U0001F600";
    const auto quality = assess(text.size(), true);
    return text.size() == 100
        && quality.codeUnitLength == 100
        && quality.isValid()
        && !quality.outsidePreferredLength;
}

} // namespace

int main()
{
    if (!emptyCommentIsInvalidWithoutWarnings())
    {
        std::fprintf(stderr, "Empty comment quality was not rejected.\n");
        return EXIT_FAILURE;
    }
    if (!minimumLengthBoundaryUsesConfiguredLimit())
    {
        std::fprintf(stderr, "Minimum comment length boundary changed.\n");
        return EXIT_FAILURE;
    }
    if (!preferredLengthBoundaryWarnsOnlyAboveFourTwenty())
    {
        std::fprintf(stderr, "Preferred upper length boundary changed.\n");
        return EXIT_FAILURE;
    }
    if (!maximumLengthBoundaryRejectsOnlyAfterConfiguredLimit())
    {
        std::fprintf(stderr, "Maximum comment length boundary changed.\n");
        return EXIT_FAILURE;
    }
    if (!placeholderAndCombinedWarningsAreReportedSeparately())
    {
        std::fprintf(stderr, "Placeholder warning decision changed.\n");
        return EXIT_FAILURE;
    }
    if (!supplementaryUnicodeUsesUtf16CodeUnitLength())
    {
        std::fprintf(stderr, "Quality length did not use UTF-16 code units.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
