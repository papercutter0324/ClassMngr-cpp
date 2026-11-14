#pragma once

#include "next/application/speaking_evaluation_powerpoint_presentation_capabilities.h"

#include <QtGlobal>

namespace ClassMngr::Next::Platform
{

// Resolves platform presentation policy at the Qt boundary. This policy is
// independent of whether Microsoft Office is installed and available.
class QtSpeakingEvaluationPowerPointPresentationAdapter final
{
public:
    [[nodiscard]] static constexpr
        Application::SpeakingEvaluationPowerPointPresentationCapabilities
        capabilities() noexcept
    {
#if defined(Q_OS_MACOS)
        return {
            .rendererOptionSupported = true,
            .protectedDataAccessNoticeSupported = true
        };
#elif defined(Q_OS_WIN)
        return {
            .rendererOptionSupported = true,
            .protectedDataAccessNoticeSupported = false
        };
#else
        return {};
#endif
    }
};

}  // namespace ClassMngr::Next::Platform
