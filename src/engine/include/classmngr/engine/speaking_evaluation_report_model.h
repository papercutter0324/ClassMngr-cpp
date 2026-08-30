#pragma once

#include <string>
#include <string_view>

namespace classmngr::engine
{

enum class SpeakingEvaluationReportTemplate
{
    Standard,
    Advanced
};

class SpeakingEvaluationReportModel final
{
public:
    [[nodiscard]] static int elementaryGrade(
        std::string_view classGrade
        );

    [[nodiscard]] static std::string classLabel(
        std::string_view classGrade,
        std::string_view classLevel
        );

    [[nodiscard]] static SpeakingEvaluationReportTemplate templateForClass(
        std::string_view classGrade,
        std::string_view classLevel
        );

    // Formats the date text placed in the report template.  The month is
    // one-based; an invalid month returns an empty string.
    [[nodiscard]] static std::string reportDate(
        int year,
        unsigned month,
        SpeakingEvaluationReportTemplate reportTemplate
        );
};

} // namespace classmngr::engine
