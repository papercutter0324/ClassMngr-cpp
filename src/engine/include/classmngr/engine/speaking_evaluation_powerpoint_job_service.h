#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/speaking_evaluation_report_content.h"

#include <cstdint>
#include <string>
#include <vector>

namespace classmngr::engine
{

struct SpeakingEvaluationPowerPointJobRequest
{
    std::vector<SpeakingEvaluationReportContent> reports;
    std::vector<std::string> pdfPaths;
    std::vector<std::string> completionPaths;
    std::vector<std::uint8_t> signatureImage;
};

struct SpeakingEvaluationPowerPointStudentJob
{
    std::string displayName;
    std::string pdfPath;
    std::string completionPath;
    std::string englishName;
    std::string koreanName;
    std::string classLabel;
    std::string nativeTeacher;
    std::string koreanTeacher;
    std::string date;
    std::string comments;
    std::string overallGrade;
    SpeakingEvaluationScores scores{};
};

struct SpeakingEvaluationPowerPointJob
{
    SpeakingEvaluationReportTemplate reportTemplate =
        SpeakingEvaluationReportTemplate::Standard;
    std::vector<std::uint8_t> signatureImage;
    std::vector<SpeakingEvaluationPowerPointStudentJob> students;
};

class SpeakingEvaluationPowerPointJobService final
{
public:
    // This builds renderer-neutral job content. Unicode normalization,
    // comment text measurement, resource paths, JSON, and Office automation
    // remain in the presentation adapter.
    [[nodiscard]] static Result<SpeakingEvaluationPowerPointJob> build(
        const SpeakingEvaluationPowerPointJobRequest& request
        );
};

} // namespace classmngr::engine
