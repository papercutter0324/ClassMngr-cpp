#include "classmngr/engine/speaking_evaluation_report_content.h"

#include <cctype>

namespace classmngr::engine
{
namespace
{
std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (
        first < value.size()
        && std::isspace(static_cast<unsigned char>(value[first]))
        )
    {
        ++first;
    }

    std::size_t last = value.size();
    while (
        last > first
        && std::isspace(static_cast<unsigned char>(value[last - 1]))
        )
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}
} // namespace

std::string SpeakingEvaluationReportContentService::displayName(
    std::string_view englishName,
    std::string_view koreanName
    )
{
    const std::string english = trimAsciiWhitespace(englishName);
    const std::string korean = trimAsciiWhitespace(koreanName);

    if (english.empty())
    {
        return korean;
    }
    if (korean.empty())
    {
        return english;
    }

    return english + " (" + korean + ')';
}

SpeakingEvaluationReportContent
SpeakingEvaluationReportContentService::reportForStudent(
    const SpeakingEvaluationReportStudentInput& student,
    const ClassInfo& classInfo,
    int sourceRow,
    int year,
    unsigned month
    )
{
    SpeakingEvaluationReportContent report;
    report.displayName = displayName(
        student.englishName,
        student.koreanName
        );
    report.englishName = student.englishName;
    report.koreanName = student.koreanName;
    report.classLabel = SpeakingEvaluationReportModel::classLabel(
        classInfo.classGrade,
        classInfo.classLevel
        );
    report.nativeTeacher = classInfo.teacherEn;
    report.koreanTeacher = classInfo.teacherKr;
    report.date = SpeakingEvaluationReportModel::reportDate(
        year,
        month,
        SpeakingEvaluationReportModel::templateForClass(
            classInfo.classGrade,
            classInfo.classLevel
            )
        );
    report.comments = student.comments;
    report.notes = student.notes;
    report.grade = SpeakingEvaluationReportModel::elementaryGrade(
        classInfo.classGrade
        );
    report.scores = student.scores;
    report.reportTemplate = SpeakingEvaluationReportModel::templateForClass(
        classInfo.classGrade,
        classInfo.classLevel
        );
    report.sourceRow = sourceRow;
    return report;
}

std::vector<SpeakingEvaluationReportContent>
SpeakingEvaluationReportContentService::buildReports(
    const std::vector<SpeakingEvaluationReportStudentInput>& students,
    const ClassInfo& classInfo,
    int year,
    unsigned month
    )
{
    std::vector<SpeakingEvaluationReportContent> reports;
    reports.reserve(students.size());
    for (std::size_t index = 0; index < students.size(); ++index)
    {
        const auto& student = students[index];
        if (displayName(student.englishName, student.koreanName).empty())
        {
            continue;
        }

        reports.push_back(
            reportForStudent(
                student,
                classInfo,
                static_cast<int>(index),
                year,
                month
                )
            );
    }
    return reports;
}

} // namespace classmngr::engine
