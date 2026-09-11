#include "classmngr/engine/speaking_evaluation_powerpoint_job_service.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace
{
using classmngr::engine::ErrorCode;
using classmngr::engine::SpeakingEvaluationPowerPointJobRequest;
using classmngr::engine::SpeakingEvaluationPowerPointJobService;
using classmngr::engine::SpeakingEvaluationReportContent;
using classmngr::engine::SpeakingEvaluationReportTemplate;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSpeakingEvaluationPowerPointJobServiceTests: "
              << message
              << '\n';
    return false;
}

SpeakingEvaluationReportContent report(
    std::string_view displayName,
    SpeakingEvaluationReportTemplate reportTemplate
    )
{
    SpeakingEvaluationReportContent value;
    value.displayName = displayName;
    value.englishName = "Alex";
    value.koreanName = "\xEA\xB9\x80\xEB\xAF\xBC\xEC\xA7\x80";
    value.classLabel = "E5 Advanced";
    value.nativeTeacher = "Taylor";
    value.koreanTeacher = "\xEC\x9D\xB4\xEC\x84\xB8\xEC\xA7\x84";
    value.date = "2026-08-30";
    value.comments = "Good work";
    value.scores = { "A+", "A+", "A+", "A+", "A+", "A+" };
    value.reportTemplate = reportTemplate;
    return value;
}

SpeakingEvaluationPowerPointJobRequest validRequest()
{
    SpeakingEvaluationPowerPointJobRequest request;
    request.reports = {
        report("Alex (\xEA\xB9\x80\xEB\xAF\xBC\xEC\xA7\x80)", SpeakingEvaluationReportTemplate::Standard),
        report("Jamie", SpeakingEvaluationReportTemplate::Standard)
    };
    request.pdfPaths = { "C:/reports/alex.pdf", "C:/reports/jamie.pdf" };
    request.completionPaths = {
        "C:/work/completed-000000",
        "C:/work/completed-000001"
    };
    request.signatureImage = { 0x89, 0x50, 0x4e, 0x47 };
    return request;
}

bool expectError(
    const SpeakingEvaluationPowerPointJobRequest& request,
    ErrorCode code,
    std::string_view message
    )
{
    const auto result = SpeakingEvaluationPowerPointJobService::build(request);
    return expect(!result, "invalid request unexpectedly produced a job")
        && expect(result.error().code == code, message)
        && expect(result.error().message == message, message);
}
} // namespace

int main()
{
    bool passed = true;

    const auto request = validRequest();
    const auto jobResult = SpeakingEvaluationPowerPointJobService::build(
        request
        );
    passed &= expect(jobResult.has_value(), "valid job request was rejected");
    if (jobResult)
    {
        passed &= expect(
            jobResult->reportTemplate
                == SpeakingEvaluationReportTemplate::Standard,
            "job template was not preserved"
            );
        passed &= expect(
            jobResult->signatureImage == request.signatureImage,
            "signature bytes were not preserved"
            );
        passed &= expect(
            jobResult->students.size() == 2,
            "student count changed"
            );
        if (jobResult->students.size() == 2)
        {
            passed &= expect(
                jobResult->students[0].displayName
                    == request.reports[0].displayName,
                "display name changed"
                );
            passed &= expect(
                jobResult->students[0].pdfPath == request.pdfPaths[0]
                    && jobResult->students[0].completionPath
                        == request.completionPaths[0],
                "job paths changed"
                );
            passed &= expect(
                jobResult->students[0].overallGrade == "A+"
                    && jobResult->students[0].scores
                        == request.reports[0].scores,
                "score or overall-grade mapping changed"
                );
            passed &= expect(
                jobResult->students[1].englishName == "Alex"
                    && jobResult->students[1].comments == "Good work",
                "report fields were not mapped"
                );
        }
    }

    auto emptyRequest = request;
    emptyRequest.reports.clear();
    passed &= expectError(
        emptyRequest,
        ErrorCode::InvalidArgument,
        "no-reports"
        );

    auto pdfMismatchRequest = request;
    pdfMismatchRequest.pdfPaths.pop_back();
    passed &= expectError(
        pdfMismatchRequest,
        ErrorCode::InvalidArgument,
        "pdf-path-count-mismatch"
        );

    auto completionMismatchRequest = request;
    completionMismatchRequest.completionPaths.pop_back();
    passed &= expectError(
        completionMismatchRequest,
        ErrorCode::InvalidArgument,
        "completion-path-count-mismatch"
        );

    auto mixedTemplateRequest = request;
    mixedTemplateRequest.reports[1].reportTemplate =
        SpeakingEvaluationReportTemplate::Advanced;
    passed &= expectError(
        mixedTemplateRequest,
        ErrorCode::Constraint,
        "mixed-powerpoint-templates"
        );

    return passed ? 0 : 1;
}
