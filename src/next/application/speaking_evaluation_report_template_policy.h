#pragma once

#include "next/application/speaking_evaluation_report_content.h"

#include <span>

namespace ClassMngr::Next::Application
{

// A homogeneous sequence is compatible with a single-template operation. The
// empty sequence is compatible because there is no template conflict.
[[nodiscard]] inline bool usesSingleSpeakingEvaluationReportTemplate(
    const std::span<const SpeakingEvaluationReportTemplate> reportTemplates
    ) noexcept
{
    if (reportTemplates.empty())
    {
        return true;
    }

    const SpeakingEvaluationReportTemplate firstTemplate =
        reportTemplates.front();
    for (
        const SpeakingEvaluationReportTemplate reportTemplate :
        reportTemplates
        )
    {
        if (reportTemplate != firstTemplate)
        {
            return false;
        }
    }

    return true;
}

} // namespace ClassMngr::Next::Application
