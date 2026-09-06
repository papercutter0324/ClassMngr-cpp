#include "classmngr/engine/speaking_evaluation_report_model.h"

#include <array>
#include <charconv>
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
        && std::isspace(
            static_cast<unsigned char>(value[first])
            )
        )
    {
        ++first;
    }

    std::size_t last = value.size();
    while (
        last > first
        && std::isspace(
            static_cast<unsigned char>(value[last - 1])
            )
        )
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

char asciiUpper(char value)
{
    return static_cast<char>(
        std::toupper(static_cast<unsigned char>(value))
        );
}

bool equalsIgnoreAsciiCase(
    std::string_view left,
    std::string_view right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (asciiUpper(left[index]) != asciiUpper(right[index]))
        {
            return false;
        }
    }

    return true;
}

int parseGrade(std::string_view value)
{
    if (value.empty())
    {
        return 0;
    }

    if (value.front() == 'E' || value.front() == 'e')
    {
        value.remove_prefix(1);
    }

    if (value.empty())
    {
        return 0;
    }

    if (value.front() == '+')
    {
        value.remove_prefix(1);
    }

    if (value.empty())
    {
        return 0;
    }

    int grade = 0;
    const auto parsed = std::from_chars(
        value.data(),
        value.data() + value.size(),
        grade
        );
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
    {
        return 0;
    }

    return grade >= 4 && grade <= 6 ? grade : 0;
}
} // namespace

int SpeakingEvaluationReportModel::elementaryGrade(
    std::string_view classGrade
    )
{
    return parseGrade(trimAsciiWhitespace(classGrade));
}

std::string SpeakingEvaluationReportModel::classLabel(
    std::string_view classGrade,
    std::string_view classLevel
    )
{
    const std::string grade = trimAsciiWhitespace(classGrade);
    const std::string level = trimAsciiWhitespace(classLevel);

    if (grade.empty())
    {
        return level;
    }
    if (level.empty())
    {
        return grade;
    }

    return grade + " " + level;
}

SpeakingEvaluationReportTemplate
SpeakingEvaluationReportModel::templateForClass(
    std::string_view classGrade,
    std::string_view classLevel
    )
{
    const std::string grade = trimAsciiWhitespace(classGrade);
    const std::string level = trimAsciiWhitespace(classLevel);

    const bool usesAdvancedTemplate = (
               equalsIgnoreAsciiCase(grade, "E5")
               && equalsIgnoreAsciiCase(level, "Athena")
               )
        || (
               equalsIgnoreAsciiCase(grade, "E6")
               && equalsIgnoreAsciiCase(level, "Song's")
               );

    return usesAdvancedTemplate
        ? SpeakingEvaluationReportTemplate::Advanced
        : SpeakingEvaluationReportTemplate::Standard;
}

std::string SpeakingEvaluationReportModel::reportDate(
    int year,
    unsigned month,
    SpeakingEvaluationReportTemplate reportTemplate
    )
{
    if (month < 1 || month > 12)
    {
        return {};
    }

    constexpr std::array<std::string_view, 12> shortMonths{
        "Jan.", "Feb.", "Mar.", "Apr.", "May.", "Jun.",
        "Jul.", "Aug.", "Sep.", "Oct.", "Nov.", "Dec."
    };
    constexpr std::array<std::string_view, 12> standardMonths{
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    const auto index = static_cast<std::size_t>(month - 1);
    const bool useFullMonthName =
        reportTemplate == SpeakingEvaluationReportTemplate::Standard
        && month >= 5
        && month <= 7;
    const std::string_view monthText =
        useFullMonthName ? standardMonths[index] : shortMonths[index];

    return std::string(monthText) + " " + std::to_string(year);
}

} // namespace classmngr::engine
