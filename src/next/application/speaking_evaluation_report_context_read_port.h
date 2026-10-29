#pragma once
#include "next/application/speaking_evaluation_report_content.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{
struct SpeakingEvaluationReportSchedule final
{
    std::u16string day;
    std::u16string startTime;
    std::u16string endTime;
};
struct SpeakingEvaluationReportContext final
{
    SpeakingEvaluationReportClassValues classValues;
    std::vector<SpeakingEvaluationReportSchedule> regularTimes;
    std::string signatureImage;
};
using SpeakingEvaluationReportContextReadResult =
    Domain::Result<SpeakingEvaluationReportContext>;
class SpeakingEvaluationReportContextReadPort
{
public:
    virtual ~SpeakingEvaluationReportContextReadPort() = default;
    [[nodiscard]] virtual SpeakingEvaluationReportContextReadResult readReportContext(
        const Domain::ClassId& classId) const = 0;
};
}
