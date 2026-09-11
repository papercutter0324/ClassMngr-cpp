#include "classmngr/engine/speaking_evaluation_powerpoint_job_service.h"

#include "classmngr/engine/speaking_evaluation_report_service.h"

#include <cstddef>
#include <optional>
#include <utility>

namespace classmngr::engine
{
namespace
{
std::unexpected<Error> invalidArgument(
    const char* message
    )
{
    return std::unexpected(Error{
        ErrorCode::InvalidArgument,
        message,
        std::nullopt
    });
}

std::unexpected<Error> constraintViolation(
    const char* message
    )
{
    return std::unexpected(Error{
        ErrorCode::Constraint,
        message,
        std::nullopt
    });
}
} // namespace

Result<SpeakingEvaluationPowerPointJob>
SpeakingEvaluationPowerPointJobService::build(
    const SpeakingEvaluationPowerPointJobRequest& request
    )
{
    if (request.reports.empty())
    {
        return invalidArgument("no-reports");
    }

    if (request.pdfPaths.size() != request.reports.size())
    {
        return invalidArgument("pdf-path-count-mismatch");
    }

    if (request.completionPaths.size() != request.reports.size())
    {
        return invalidArgument("completion-path-count-mismatch");
    }

    const SpeakingEvaluationReportTemplate reportTemplate =
        request.reports.front().reportTemplate;
    for (const SpeakingEvaluationReportContent& report : request.reports)
    {
        if (report.reportTemplate != reportTemplate)
        {
            return constraintViolation("mixed-powerpoint-templates");
        }
    }

    SpeakingEvaluationPowerPointJob job;
    job.reportTemplate = reportTemplate;
    job.signatureImage = request.signatureImage;
    job.students.reserve(request.reports.size());

    for (std::size_t index = 0; index < request.reports.size(); ++index)
    {
        const SpeakingEvaluationReportContent& report =
            request.reports[index];

        SpeakingEvaluationPowerPointStudentJob student;
        student.displayName = report.displayName;
        student.pdfPath = request.pdfPaths[index];
        student.completionPath = request.completionPaths[index];
        student.englishName = report.englishName;
        student.koreanName = report.koreanName;
        student.classLabel = report.classLabel;
        student.nativeTeacher = report.nativeTeacher;
        student.koreanTeacher = report.koreanTeacher;
        student.date = report.date;
        student.comments = report.comments;
        student.overallGrade =
            SpeakingEvaluationReportService::overallGrade(report.scores);
        student.scores = report.scores;
        job.students.push_back(std::move(student));
    }

    return job;
}

} // namespace classmngr::engine
