#pragma once

#include "classmngr/engine/class_info.h"
#include "classmngr/engine/speaking_evaluation_report_model.h"
#include "classmngr/engine/speaking_evaluation_report_service.h"

#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

struct SpeakingEvaluationReportStudentInput
{
    std::string englishName;
    std::string koreanName;
    SpeakingEvaluationScores scores{};
    std::string comments;
    std::string notes;
};

struct SpeakingEvaluationReportContent
{
    std::string displayName;
    std::string englishName;
    std::string koreanName;
    std::string classLabel;
    std::string nativeTeacher;
    std::string koreanTeacher;
    std::string date;
    std::string comments;
    std::string notes;
    int grade = 0;
    SpeakingEvaluationScores scores{};
    SpeakingEvaluationReportTemplate reportTemplate =
        SpeakingEvaluationReportTemplate::Standard;
    int sourceRow = -1;
};

class SpeakingEvaluationReportContentService final
{
public:
    [[nodiscard]] static std::string displayName(
        std::string_view englishName,
        std::string_view koreanName
        );

    [[nodiscard]] static SpeakingEvaluationReportContent reportForStudent(
        const SpeakingEvaluationReportStudentInput& student,
        const ClassInfo& classInfo,
        int sourceRow,
        int year,
        unsigned month
        );

    [[nodiscard]] static std::vector<SpeakingEvaluationReportContent>
    buildReports(
        const std::vector<SpeakingEvaluationReportStudentInput>& students,
        const ClassInfo& classInfo,
        int year,
        unsigned month
        );
};

} // namespace classmngr::engine
