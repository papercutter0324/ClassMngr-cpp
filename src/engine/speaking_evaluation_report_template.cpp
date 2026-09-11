#include "classmngr/engine/speaking_evaluation_report_template.h"

namespace classmngr::engine
{
namespace
{
const SpeakingEvaluationReportTemplatePolicy standard{
    SpeakingEvaluationReportTemplate::Standard,
    540.0,
    780.0,
    { 374.0, 731.0, 131.0, 26.0 },
    "Speaking Evaluations/SpeakingEvaluationTemplate-Full.pptx",
    false,
    true,
    true,
    "Grades & Scores",
    12,
    6,
    2,
    217,
    217,
    217
};

const SpeakingEvaluationReportTemplatePolicy advanced{
    SpeakingEvaluationReportTemplate::Advanced,
    540.0,
    780.0,
    { 408.5, 746.25, 114.0, 24.75 },
    "Speaking Evaluations/SpeakingEvaluationTemplate_Advanced-Full.pptx",
    true,
    true,
    false,
    "Report_Table",
    12,
    7,
    3,
    229,
    229,
    231
};
} // namespace

const SpeakingEvaluationReportTemplatePolicy&
SpeakingEvaluationReportTemplateService::policy(
    SpeakingEvaluationReportTemplate reportTemplate
    )
{
    switch (reportTemplate)
    {
    case SpeakingEvaluationReportTemplate::Advanced:
        return advanced;
    case SpeakingEvaluationReportTemplate::Standard:
    default:
        return standard;
    }
}

} // namespace classmngr::engine
