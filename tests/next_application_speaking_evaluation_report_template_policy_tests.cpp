#include "next/application/speaking_evaluation_report_template_policy.h"

#include <cstdlib>
#include <vector>

using ClassMngr::Next::Application::SpeakingEvaluationReportTemplate;
using ClassMngr::Next::Application::
    usesSingleSpeakingEvaluationReportTemplate;

int main()
{
    const std::vector<SpeakingEvaluationReportTemplate> noTemplates;
    const std::vector<SpeakingEvaluationReportTemplate> oneStandard{
        SpeakingEvaluationReportTemplate::Standard
    };
    const std::vector<SpeakingEvaluationReportTemplate> allStandard{
        SpeakingEvaluationReportTemplate::Standard,
        SpeakingEvaluationReportTemplate::Standard,
        SpeakingEvaluationReportTemplate::Standard
    };
    const std::vector<SpeakingEvaluationReportTemplate> allAdvanced{
        SpeakingEvaluationReportTemplate::Advanced,
        SpeakingEvaluationReportTemplate::Advanced
    };
    const std::vector<SpeakingEvaluationReportTemplate> mixedTemplates{
        SpeakingEvaluationReportTemplate::Standard,
        SpeakingEvaluationReportTemplate::Advanced,
        SpeakingEvaluationReportTemplate::Standard
    };

    if (!usesSingleSpeakingEvaluationReportTemplate(noTemplates)
        || !usesSingleSpeakingEvaluationReportTemplate(oneStandard)
        || !usesSingleSpeakingEvaluationReportTemplate(allStandard)
        || !usesSingleSpeakingEvaluationReportTemplate(allAdvanced)
        || usesSingleSpeakingEvaluationReportTemplate(mixedTemplates))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
