#include "next/application/speaking_evaluation_report_content.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using ClassMngr::Next::Application::SpeakingEvaluationReportClassValues;
using ClassMngr::Next::Application::SpeakingEvaluationReportContentRequest;
using ClassMngr::Next::Application::SpeakingEvaluationReportSourceRow;
using ClassMngr::Next::Application::SpeakingEvaluationReportStudentContent;
using ClassMngr::Next::Application::SpeakingEvaluationReportTemplate;
using ClassMngr::Next::Application::projectSpeakingEvaluationReportContent;
using ClassMngr::Next::Domain::CourseGradeBand;

static_assert(std::is_same_v<
    decltype(SpeakingEvaluationReportStudentContent::sourceRow),
    SpeakingEvaluationReportSourceRow
    >);
static_assert(!std::is_convertible_v<std::size_t, SpeakingEvaluationReportSourceRow>);
static_assert(!std::is_convertible_v<SpeakingEvaluationReportSourceRow, std::size_t>);

constexpr SpeakingEvaluationReportSourceRow DefaultSourceRow{};
constexpr SpeakingEvaluationReportSourceRow ZeroSourceRow{0};
constexpr SpeakingEvaluationReportSourceRow NonzeroSourceRow{7};
static_assert(DefaultSourceRow.rawValue() == 0);
static_assert(DefaultSourceRow == ZeroSourceRow);
static_assert(NonzeroSourceRow.rawValue() == 7);
static_assert(NonzeroSourceRow != ZeroSourceRow);

namespace
{

std::vector<std::u16string> fullRow()
{
    return std::vector<std::u16string>(11);
}

bool filtersOnlyRowsWithBothNamesBlankAndPreservesRawContent()
{
    SpeakingEvaluationReportContentRequest request;
    request.classValues = {
        .grade = u" e5 ",
        .level = u" aTHENA ",
        .nativeTeacher = u" Teacher En ",
        .koreanTeacher = u"Teacher Kr"
    };

    std::vector<std::u16string> blankNames = fullRow();
    std::vector<std::u16string> whitespaceNames = fullRow();
    whitespaceNames[1] = u"\u00A0\u3000";
    whitespaceNames[2] = u" \t\n\u2003";

    std::vector<std::u16string> namedStudent = fullRow();
    namedStudent[1] = u"\u2003 Amy Smith \u3000";
    namedStudent[2] = u" \uAE40\uBBFC\uC9C0 ";
    namedStudent[3] = u" A+ ";
    namedStudent[4] = u"B";
    namedStudent[5] = u"C";
    namedStudent[6] = u"A";
    namedStudent[7] = u"B+";
    namedStudent[8] = u"A+";
    namedStudent[9] = u" raw comment ";
    namedStudent[10] = u" raw notes ";

    std::vector<std::u16string> koreanOnly{
        u"",
        u"",
        u"Korean student"
    };
    request.rows = {
        std::move(blankNames),
        std::move(whitespaceNames),
        std::move(namedStudent),
        std::move(koreanOnly)
    };

    const auto result = projectSpeakingEvaluationReportContent(
        std::move(request)
        );
    request.rows = {{u"", u"Mutated source after projection"}};
    request.classValues = {
        .grade = u"E4",
        .level = u"Changed",
        .nativeTeacher = u"Changed teacher",
        .koreanTeacher = u"Changed teacher"
    };
    if (result.students.size() != 2
        || result.students[0].sourceRow.rawValue() != 2
        || result.students[1].sourceRow.rawValue() != 3
        || result.students[0].displayEnglishName != u"Amy Smith"
        || result.students[0].displayKoreanName != u"\uAE40\uBBFC\uC9C0"
        || result.students[0].englishName != u"\u2003 Amy Smith \u3000"
        || result.students[0].koreanName != u" \uAE40\uBBFC\uC9C0 "
        || result.students[1].displayEnglishName != u""
        || result.students[1].displayKoreanName != u"Korean student"
        || result.students[0].comments != u" raw comment "
        || result.students[0].notes != u" raw notes "
        || result.students[0].scores
            != std::array<std::u16string, 6>{
                u" A+ ", u"B", u"C", u"A", u"B+", u"A+"
            }
        || result.classLabel != u"e5 aTHENA"
        || result.nativeTeacher != u" Teacher En "
        || result.koreanTeacher != u"Teacher Kr"
        || result.grade != CourseGradeBand::E5
        || result.reportTemplate != SpeakingEvaluationReportTemplate::Advanced)
    {
        return false;
    }

    return true;
}

bool classClassificationAndElementaryGradeMappingMatchTheExistingRules()
{
    SpeakingEvaluationReportContentRequest request;
    std::vector<std::u16string> namedStudent = fullRow();
    namedStudent[1] = u"Student";
    request.rows.push_back(std::move(namedStudent));

    request.classValues.grade = u" e6 ";
    request.classValues.level = u" sOnG's ";
    auto result = projectSpeakingEvaluationReportContent(request);
    if (result.grade != CourseGradeBand::E6
        || result.reportTemplate != SpeakingEvaluationReportTemplate::Advanced)
    {
        return false;
    }

    request.classValues.level = u"Athena";
    result = projectSpeakingEvaluationReportContent(request);
    if (result.reportTemplate != SpeakingEvaluationReportTemplate::Standard)
    {
        return false;
    }

    const std::vector<std::pair<std::u16string, CourseGradeBand>> gradeCases{
        {u"E4", CourseGradeBand::E4},
        {u"e5", CourseGradeBand::E5},
        {u" E6 ", CourseGradeBand::E6},
        {u"4", CourseGradeBand::E4},
        {u"+6", CourseGradeBand::E6},
        {u"\u3000e+0005\u00A0", CourseGradeBand::E5},
        {u"", CourseGradeBand::Other},
        {u"\u2003", CourseGradeBand::Other},
        {u"E3", CourseGradeBand::Other},
        {u"E7", CourseGradeBand::Other},
        {u"M1", CourseGradeBand::Other},
        {u"M2", CourseGradeBand::Other},
        {u"M3", CourseGradeBand::Other},
        {u"EE5", CourseGradeBand::Other},
        {u"E++5", CourseGradeBand::Other},
        {u"5.0", CourseGradeBand::Other},
        {u"\u0664", CourseGradeBand::Other},
        {u"not a grade", CourseGradeBand::Other}
    };
    for (const auto& [grade, expected] : gradeCases)
    {
        request.classValues.grade = grade;
        if (projectSpeakingEvaluationReportContent(request).grade != expected)
        {
            return false;
        }
    }

    request.classValues.grade = u"E5";
    request.classValues.level = u"ATHENA";
    if (projectSpeakingEvaluationReportContent(request).reportTemplate
        != SpeakingEvaluationReportTemplate::Advanced)
    {
        return false;
    }

    request.classValues.grade = u"5";
    if (projectSpeakingEvaluationReportContent(request).reportTemplate
        != SpeakingEvaluationReportTemplate::Standard)
    {
        return false;
    }

    request.classValues.grade = u"E5";
    request.classValues.level = u"Song's";
    return projectSpeakingEvaluationReportContent(request).reportTemplate
        == SpeakingEvaluationReportTemplate::Standard;
}

bool shortRowsDefaultMissingColumnsAndThereIsNoArbitraryRowCap()
{
    SpeakingEvaluationReportContentRequest request;
    request.rows.push_back({u"", u"", u"Korean only"});
    for (int index = 0; index < 32; ++index)
    {
        request.rows.push_back({u"", u"Student"});
    }

    const auto result = projectSpeakingEvaluationReportContent(request);
    if (result.students.size() != 33
        || result.students.front().sourceRow.rawValue() != 0
        || result.students.front().englishName != u""
        || result.students.front().koreanName != u"Korean only"
        || result.students.front().scores
            != std::array<std::u16string, 6>{}
        || !result.students.front().comments.empty()
        || !result.students.front().notes.empty())
    {
        return false;
    }

    for (std::size_t index = 0; index < result.students.size(); ++index)
    {
        if (result.students[index].sourceRow.rawValue() != index)
        {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    if (!filtersOnlyRowsWithBothNamesBlankAndPreservesRawContent())
    {
        std::fprintf(
            stderr,
            "Speaking-evaluation report filtering or field projection was incorrect.\n"
            );
        return EXIT_FAILURE;
    }
    if (!classClassificationAndElementaryGradeMappingMatchTheExistingRules())
    {
        std::fprintf(
            stderr,
            "Speaking-evaluation report class mapping was incorrect.\n"
            );
        return EXIT_FAILURE;
    }
    if (!shortRowsDefaultMissingColumnsAndThereIsNoArbitraryRowCap())
    {
        std::fprintf(
            stderr,
            "Speaking-evaluation report short-row defaults or row count were incorrect.\n"
            );
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
