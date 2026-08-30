#include "classmngr/engine/speaking_evaluation_report_output_policy.h"

#include <iostream>
#include <string>
#include <string_view>

namespace
{
using classmngr::engine::ClassInfo;
using classmngr::engine::SpeakingEvaluationReportOutputPolicy;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineSpeakingEvaluationReportOutputPolicyTests: "
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
    classInfo.classLevel = "Zeus";
    classInfo.classTimes = {
        { "Monday", "4:00 PM", "4:50 PM" },
        { "Wednesday", "4:00 PM", "4:50 PM" }
    };

    passed &= expect(
        SpeakingEvaluationReportOutputPolicy::defaultDirectory(
            classInfo,
            "Winter",
            "C:/Documents"
            ) == "C:/Documents/DYB/SpeakingEvals/E5 Zeus (MW - 4pm)/Winter",
        "default report directory changed"
        );

    classInfo.classGrade.clear();
    classInfo.classLevel.clear();
    classInfo.classTimes.clear();
    passed &= expect(
        SpeakingEvaluationReportOutputPolicy::defaultDirectory(
            classInfo,
            "  ",
            "C:/Documents",
            "Speaking Evaluation",
            "Evaluation"
            ) == "C:/Documents/DYB/SpeakingEvals/Speaking Evaluation/Evaluation",
        "empty report directory components did not use fallbacks"
        );

    classInfo.classGrade = "E6";
    classInfo.classLevel = "Gaia/Blue";
    classInfo.classTimes = {
        { "Tuesday", "16:30", "17:20" }
    };
    const std::string portableDirectory =
        SpeakingEvaluationReportOutputPolicy::defaultDirectory(
            classInfo,
            "Spring:2026",
            "C:/Documents/"
            );
    passed &= expect(
        portableDirectory
            == "C:/Documents/DYB/SpeakingEvals/E6 Gaia-Blue (T - 4-30pm)/Spring-2026",
        "folder safety or 24-hour schedule formatting changed"
        );

    passed &= expect(
        SpeakingEvaluationReportOutputPolicy::batchArchivePath(
            "C:/Documents/Speaking Evals/Winter"
            ) == "C:/Documents/Speaking Evals/Winter/Winter.zip",
        "batch archive path changed"
        );

    passed &= expect(
        SpeakingEvaluationReportOutputPolicy::studentFileName(
            "Jane: Doe",
            "\xEA\xB9\x80/\xEC\xB2\xA0\xEC\x88\x98"
            ) == "Jane- Doe (\xEA\xB9\x80-\xEC\xB2\xA0\xEC\x88\x98).pdf",
        "unsafe student filename changed"
        );
    passed &= expect(
        SpeakingEvaluationReportOutputPolicy::studentFileName(
            "Report.PDF",
            {}
            ) == "Report.pdf",
        "existing PDF suffix was not normalized"
        );
    passed &= expect(
        SpeakingEvaluationReportOutputPolicy::studentFileName(
            "CON",
            {}
            ) == "_CON.pdf",
        "Windows reserved student filename was not protected"
        );
    passed &= expect(
        SpeakingEvaluationReportOutputPolicy::studentFileName(
            {},
            {}
            ) == "Student.pdf",
        "empty student filename did not use its fallback"
        );

    const std::string longName(300, 'A');
    const std::string limitedName =
        SpeakingEvaluationReportOutputPolicy::studentFileName(
            longName,
            {}
            );
    passed &= expect(
        limitedName.size() == 244,
        "student filename UTF-8 length limit changed"
        );

    return passed ? 0 : 1;
}
