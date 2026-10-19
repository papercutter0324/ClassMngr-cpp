#pragma once

#include "next/domain/course.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class SpeakingEvaluationReportTemplate
{
    Standard,
    Advanced
};

struct SpeakingEvaluationReportClassValues final
{
    std::u16string grade;
    std::u16string level;
    std::u16string nativeTeacher;
    std::u16string koreanTeacher;

    friend bool operator==(
        const SpeakingEvaluationReportClassValues&,
        const SpeakingEvaluationReportClassValues&
        ) = default;
};

struct SpeakingEvaluationReportContentRequest final
{
    std::vector<std::vector<std::u16string>> rows;
    SpeakingEvaluationReportClassValues classValues;

    friend bool operator==(
        const SpeakingEvaluationReportContentRequest&,
        const SpeakingEvaluationReportContentRequest&
        ) = default;
};

struct SpeakingEvaluationReportStudentContent final
{
    std::size_t sourceRow = 0;
    std::u16string englishName;
    std::u16string koreanName;
    std::u16string displayEnglishName;
    std::u16string displayKoreanName;
    std::u16string comments;
    std::u16string notes;
    std::array<std::u16string, 6> scores;

    friend bool operator==(
        const SpeakingEvaluationReportStudentContent&,
        const SpeakingEvaluationReportStudentContent&
        ) = default;
};

struct SpeakingEvaluationReportContent final
{
    std::u16string classLabel;
    std::u16string nativeTeacher;
    std::u16string koreanTeacher;
    Domain::CourseGradeBand grade = Domain::CourseGradeBand::Other;
    SpeakingEvaluationReportTemplate reportTemplate =
        SpeakingEvaluationReportTemplate::Standard;
    std::vector<SpeakingEvaluationReportStudentContent> students;

    friend bool operator==(
        const SpeakingEvaluationReportContent&,
        const SpeakingEvaluationReportContent&
        ) = default;
};

namespace SpeakingEvaluationReportContentDetail
{

// Match QString::trimmed()'s QChar::isSpace() boundary without bringing Qt
// into the application contract.
inline bool isUnicodeSpace(const char16_t character) noexcept
{
    return (character >= 0x0009 && character <= 0x000D)
        || character == 0x0020
        || character == 0x0085
        || character == 0x00A0
        || character == 0x1680
        || (character >= 0x2000 && character <= 0x200A)
        || character == 0x2028
        || character == 0x2029
        || character == 0x202F
        || character == 0x205F
        || character == 0x3000;
}

inline std::u16string trim(const std::u16string_view value)
{
    std::size_t first = 0;
    while (first < value.size() && isUnicodeSpace(value[first]))
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && isUnicodeSpace(value[last - 1]))
    {
        --last;
    }

    return std::u16string(value.substr(first, last - first));
}

inline char16_t asciiUpper(const char16_t character) noexcept
{
    if (character >= u'a' && character <= u'z')
    {
        return static_cast<char16_t>(character - (u'a' - u'A'));
    }
    return character;
}

inline bool equalsAsciiCaseInsensitive(
    const std::u16string_view left,
    const std::u16string_view right
    ) noexcept
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

inline Domain::CourseGradeBand elementaryGrade(
    const std::u16string_view rawGrade
    )
{
    std::u16string value = trim(rawGrade);
    for (char16_t& character : value)
    {
        character = asciiUpper(character);
    }

    if (!value.empty() && value.front() == u'E')
    {
        value.erase(value.begin());
    }

    if (value.empty())
    {
        return Domain::CourseGradeBand::Other;
    }

    std::size_t position = 0;
    if (value.front() == u'+')
    {
        position = 1;
    }
    if (position == value.size())
    {
        return Domain::CourseGradeBand::Other;
    }

    int grade = 0;
    for (; position < value.size(); ++position)
    {
        const char16_t character = value[position];
        if (character < u'0' || character > u'9')
        {
            return Domain::CourseGradeBand::Other;
        }

        grade = grade * 10 + static_cast<int>(character - u'0');
        if (grade > 6)
        {
            return Domain::CourseGradeBand::Other;
        }
    }

    switch (grade)
    {
    case 4:
        return Domain::CourseGradeBand::E4;
    case 5:
        return Domain::CourseGradeBand::E5;
    case 6:
        return Domain::CourseGradeBand::E6;
    default:
        return Domain::CourseGradeBand::Other;
    }
}

inline std::u16string_view cell(
    const std::vector<std::u16string>& row,
    const std::size_t column
    ) noexcept
{
    return column < row.size() ? row[column] : std::u16string_view{};
}

inline std::u16string takeCell(
    std::vector<std::u16string>& row,
    const std::size_t column
    )
{
    return column < row.size()
        ? std::move(row[column])
        : std::u16string{};
}

inline void releaseRowStorage(std::vector<std::u16string>& row)
{
    std::vector<std::u16string>().swap(row);
}

} // namespace SpeakingEvaluationReportContentDetail

// Consume the request's owned rows and return an independently owned report
// projection without retaining references into the source cells.
[[nodiscard]] inline SpeakingEvaluationReportContent
projectSpeakingEvaluationReportContent(
    SpeakingEvaluationReportContentRequest request
    )
{
    using namespace SpeakingEvaluationReportContentDetail;

    const std::u16string grade = trim(request.classValues.grade);
    const std::u16string level = trim(request.classValues.level);

    SpeakingEvaluationReportContent result;
    if (!grade.empty())
    {
        result.classLabel = grade;
    }
    if (!level.empty())
    {
        if (!result.classLabel.empty())
        {
            result.classLabel.push_back(u' ');
        }
        result.classLabel += level;
    }
    result.nativeTeacher = std::move(request.classValues.nativeTeacher);
    result.koreanTeacher = std::move(request.classValues.koreanTeacher);
    result.grade = elementaryGrade(request.classValues.grade);

    const bool usesAdvancedTemplate =
        (equalsAsciiCaseInsensitive(grade, u"E5")
         && equalsAsciiCaseInsensitive(level, u"Athena"))
        || (equalsAsciiCaseInsensitive(grade, u"E6")
            && equalsAsciiCaseInsensitive(level, u"Song's"));
    result.reportTemplate = usesAdvancedTemplate
        ? SpeakingEvaluationReportTemplate::Advanced
        : SpeakingEvaluationReportTemplate::Standard;

    constexpr std::size_t EnglishNameColumn = 1;
    constexpr std::size_t KoreanNameColumn = 2;
    constexpr std::size_t CommentsColumn = 9;
    constexpr std::size_t NotesColumn = 10;
    constexpr std::array<std::size_t, 6> ScoreColumns{3, 4, 5, 6, 7, 8};

    result.students.reserve(request.rows.size());
    for (std::size_t rowIndex = 0; rowIndex < request.rows.size(); ++rowIndex)
    {
        std::vector<std::u16string>& row = request.rows[rowIndex];
        const std::u16string displayEnglishName =
            trim(cell(row, EnglishNameColumn));
        const std::u16string displayKoreanName =
            trim(cell(row, KoreanNameColumn));
        if (displayEnglishName.empty() && displayKoreanName.empty())
        {
            releaseRowStorage(row);
            continue;
        }

        SpeakingEvaluationReportStudentContent student;
        student.sourceRow = rowIndex;
        student.englishName = takeCell(row, EnglishNameColumn);
        student.koreanName = takeCell(row, KoreanNameColumn);
        student.displayEnglishName = displayEnglishName;
        student.displayKoreanName = displayKoreanName;
        student.comments = takeCell(row, CommentsColumn);
        student.notes = takeCell(row, NotesColumn);
        for (std::size_t scoreIndex = 0;
             scoreIndex < ScoreColumns.size();
             ++scoreIndex)
        {
            student.scores[scoreIndex] = takeCell(
                row,
                ScoreColumns[scoreIndex]
                );
        }
        result.students.push_back(std::move(student));
        releaseRowStorage(row);
    }

    std::vector<std::vector<std::u16string>>().swap(request.rows);

    return result;
}

} // namespace ClassMngr::Next::Application
