#include "classmngr/engine/speaking_evaluation_report_content.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace
{
using classmngr::engine::ClassInfo;
using classmngr::engine::SpeakingEvaluationReportContentService;
using classmngr::engine::SpeakingEvaluationReportTemplate;
using classmngr::engine::SpeakingEvaluationReportStudentInput;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSpeakingEvaluationReportContentTests: "
              << message
              << '\n';
    return false;
}
} // namespace

int main()
{
    bool passed = true;

    ClassInfo classInfo;
    classInfo.classGrade = "E5";
    classInfo.classLevel = "Athena";
    classInfo.teacherEn = "Teacher";
    classInfo.teacherKr =
        "\xEC\x84\xA0" "\xEC\x83\x9D" "\xEB\x8B\x98";

    SpeakingEvaluationReportStudentInput emptyStudent;
    SpeakingEvaluationReportStudentInput student;
    student.englishName = " Alice ";
    student.koreanName =
        " " "\xEA\xB9\x80" "\xEB\xAF\xBC" "\xEC\xA7\x80" " ";
    student.scores = { "A+", "A", "B+", "B", "A", "A+" };
    student.comments = "Comment";
    student.notes = "Notes";

    const auto reports = SpeakingEvaluationReportContentService::buildReports(
        std::vector<SpeakingEvaluationReportStudentInput>{
            emptyStudent,
            student
        },
        classInfo,
        2026,
        7
        );

    passed &= expect(reports.size() == 1, "blank students were not filtered");
    if (!reports.empty())
    {
        const auto& report = reports.front();
        passed &= expect(
            report.displayName
                == "Alice (" "\xEA\xB9\x80" "\xEB\xAF\xBC" "\xEC\xA7\x80" ")",
            "display name normalization changed"
            );
        passed &= expect(
            report.sourceRow == 1,
            "source row identity was not preserved"
            );
        passed &= expect(
            report.classLabel == "E5 Athena"
                && report.nativeTeacher == "Teacher",
            "class and teacher metadata was not assembled"
            );
        passed &= expect(
            report.grade == 5
                && report.reportTemplate
                    == SpeakingEvaluationReportTemplate::Advanced
                && report.date == "Jul. 2026",
            "report metadata composition changed"
            );
        passed &= expect(
            report.scores[0] == "A+"
                && report.scores[5] == "A+"
                && report.comments == "Comment"
                && report.notes == "Notes",
            "student report fields were not copied"
            );
    }

    return passed ? 0 : 1;
}
