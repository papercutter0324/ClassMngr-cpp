#pragma once

namespace ClassMngr::Next::Application
{

// Describes which PowerPoint presentation affordances the current platform
// supports. Office installation availability remains an operation-time
// concern owned by SpeakingEvalPowerPointAutomationPort.
struct SpeakingEvaluationPowerPointPresentationCapabilities final
{
    bool rendererOptionSupported = false;
    bool protectedDataAccessNoticeSupported = false;

    [[nodiscard]] constexpr bool shouldShowProtectedDataAccessNotice(
        const bool preferenceEnabled
        ) const noexcept
    {
        return protectedDataAccessNoticeSupported && preferenceEnabled;
    }

    friend bool operator==(
        const SpeakingEvaluationPowerPointPresentationCapabilities&,
        const SpeakingEvaluationPowerPointPresentationCapabilities&
        ) = default;
};

}  // namespace ClassMngr::Next::Application
