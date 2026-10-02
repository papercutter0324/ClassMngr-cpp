#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationPrivateNotes final
{
    std::u16string didWell;
    std::u16string needsImprovement;

    friend bool operator==(
        const SpeakingEvaluationPrivateNotes&,
        const SpeakingEvaluationPrivateNotes&
        ) = default;
};

// Split only the exact stored section markers. Keep each section body intact.
[[nodiscard]] inline SpeakingEvaluationPrivateNotes
splitSpeakingEvaluationPrivateNotes(
    const std::u16string_view notes
    )
{
    constexpr std::u16string_view didWellMarker = u"[Did Well]\n";
    constexpr std::u16string_view needsImprovementMarker =
        u"\n[Needs Improvement]\n";

    if (!notes.starts_with(didWellMarker))
    {
        return { std::u16string(notes), {} };
    }

    const std::size_t separator =
        notes.find(needsImprovementMarker, didWellMarker.size());
    if (separator == std::u16string_view::npos)
    {
        return { std::u16string(notes), {} };
    }

    return {
        std::u16string(
            notes.substr(
                didWellMarker.size(),
                separator - didWellMarker.size()
                )
            ),
        std::u16string(
            notes.substr(
                separator + needsImprovementMarker.size()
                )
            )
    };
}

} // namespace ClassMngr::Next::Application
