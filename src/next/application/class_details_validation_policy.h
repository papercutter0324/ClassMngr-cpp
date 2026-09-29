#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ClassDetailsScheduleValidationRow final
{
    std::u16string day;
    std::u16string startTime;
    std::u16string endTime;
};

struct ClassDetailsValidationInput final
{
    int classId = -1;
    int teacherId = -1;
    std::u16string classGrade;
    std::u16string classLevel;
    std::u16string readingBook;
    std::u16string essayBook;
    std::u16string classColor = u"#FFFFFF";
    std::u16string fontColor = u"#000000";
    std::u16string notes;
    std::u16string timeFillerActivities;
    std::vector<ClassDetailsScheduleValidationRow> regularTimes;
    std::vector<ClassDetailsScheduleValidationRow> intensiveTimes;
};

struct ClassDetailsValidationLevelCatalog final
{
    std::u16string name;
    std::vector<std::u16string> readingBooks;
    std::vector<std::u16string> essayBooks;
};

struct ClassDetailsValidationGradeCatalog final
{
    std::u16string name;
    std::vector<ClassDetailsValidationLevelCatalog> levels;
};

// The feature adapter supplies this snapshot from ClassInfoConfig. Catalog
// values and membership rules stay in their existing single source.
struct ClassDetailsValidationCatalog final
{
    std::vector<ClassDetailsValidationGradeCatalog> grades;
};

enum class ClassDetailsValidationCode
{
    ClassIdInvalid,
    TeacherIdInvalid,
    GradeRequired,
    LevelRequired,
    ValueNotAllowed,
    BookRequiresGradeLevel,
    InvalidHexColor,
    TextLengthOutOfBounds,
    InvalidWeekday,
    InvalidTimeFormat,
    EndNotAfterStart,
    DuplicateSlot
};

enum class ClassDetailsValidationField
{
    ClassId,
    TeacherId,
    ClassGrade,
    ClassLevel,
    ReadingBook,
    EssayBook,
    ClassColor,
    FontColor,
    Notes,
    TimeFillerActivities,
    ScheduleDay,
    ScheduleStartTime,
    ScheduleEndTime
};

enum class ClassDetailsValidationSchedule
{
    Regular,
    Intensive
};

struct ClassDetailsValidationIssue final
{
    ClassDetailsValidationCode code;
    ClassDetailsValidationField field;
    std::optional<ClassDetailsValidationSchedule> schedule;
    std::size_t row = 0;
    std::optional<int> integerValue;
    std::optional<std::u16string> value;
    std::optional<std::u16string> start;
    std::optional<std::u16string> end;
    std::vector<std::u16string> allowedValues;
    std::vector<int> duplicateRows;
    std::optional<int> length;
    std::optional<int> minimum;
    std::optional<int> maximum;
};

struct ClassDetailsValidationOutput final
{
    ClassDetailsValidationInput normalized;
    std::vector<ClassDetailsValidationIssue> issues;

    [[nodiscard]] bool hasErrors() const noexcept
    {
        return !issues.empty();
    }
};

// Qt-free pre-save normalization and validation for the class details form.
// Schedule rows deliberately keep raw text so malformed UI values can still
// produce the same field diagnostics before they are converted to ScheduleTime.
class ClassDetailsValidationPolicy final
{
public:
    [[nodiscard]] static ClassDetailsValidationOutput normalizeAndValidate(
        ClassDetailsValidationInput input,
        const ClassDetailsValidationCatalog& catalog
        )
    {
        normalize(input, catalog);

        ClassDetailsValidationOutput output;
        output.normalized = std::move(input);
        validate(output, catalog);
        return output;
    }

private:
    struct ParsedTime final
    {
        int minutes = 0;
        std::u16string canonical;
    };

    struct SlotGroup final
    {
        int weekday = 0;
        int startMinute = 0;
        int endMinute = 0;
        std::vector<int> rows;
    };

    [[nodiscard]] static char16_t asciiLower(char16_t value) noexcept
    {
        if (value >= u'A' && value <= u'Z')
        {
            return static_cast<char16_t>(value + (u'a' - u'A'));
        }
        return value;
    }

    [[nodiscard]] static char16_t asciiUpper(char16_t value) noexcept
    {
        if (value >= u'a' && value <= u'z')
        {
            return static_cast<char16_t>(value - (u'a' - u'A'));
        }
        return value;
    }

    [[nodiscard]] static bool equalsCaseInsensitive(
        std::u16string_view left,
        std::u16string_view right
        ) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            if (asciiLower(left[index]) != asciiLower(right[index]))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] static bool isQtSpace(char16_t value) noexcept
    {
        return (value >= u'\t' && value <= u'\r')
            || value == u' '
            || value == 0x0085
            || value == 0x00a0
            || value == 0x1680
            || (value >= 0x2000 && value <= 0x200a)
            || value == 0x2028
            || value == 0x2029
            || value == 0x202f
            || value == 0x205f
            || value == 0x3000;
    }

    [[nodiscard]] static std::u16string trim(std::u16string_view value)
    {
        std::size_t first = 0;
        while (first < value.size() && isQtSpace(value[first]))
        {
            ++first;
        }

        std::size_t last = value.size();
        while (last > first && isQtSpace(value[last - 1]))
        {
            --last;
        }

        return std::u16string(value.substr(first, last - first));
    }

    [[nodiscard]] static std::u16string canonicalChoice(
        std::u16string_view value,
        const std::vector<std::u16string>& choices
        )
    {
        const std::u16string trimmed = trim(value);
        for (const std::u16string& choice : choices)
        {
            if (equalsCaseInsensitive(choice, trimmed))
            {
                return choice;
            }
        }
        return trimmed;
    }

    [[nodiscard]] static const ClassDetailsValidationGradeCatalog*
    findGrade(
        const ClassDetailsValidationCatalog& catalog,
        std::u16string_view grade
        ) noexcept
    {
        const auto found = std::find_if(
            catalog.grades.begin(),
            catalog.grades.end(),
            [grade](const ClassDetailsValidationGradeCatalog& candidate)
            {
                return candidate.name == grade;
            }
            );
        return found == catalog.grades.end() ? nullptr : &*found;
    }

    [[nodiscard]] static const ClassDetailsValidationLevelCatalog*
    findLevel(
        const ClassDetailsValidationGradeCatalog* grade,
        std::u16string_view level
        ) noexcept
    {
        if (!grade)
        {
            return nullptr;
        }
        const auto found = std::find_if(
            grade->levels.begin(),
            grade->levels.end(),
            [level](const ClassDetailsValidationLevelCatalog& candidate)
            {
                return candidate.name == level;
            }
            );
        return found == grade->levels.end() ? nullptr : &*found;
    }

    [[nodiscard]] static std::vector<std::u16string> gradeNames(
        const ClassDetailsValidationCatalog& catalog
        )
    {
        std::vector<std::u16string> names;
        names.reserve(catalog.grades.size());
        for (const auto& grade : catalog.grades)
        {
            names.push_back(grade.name);
        }
        return names;
    }

    [[nodiscard]] static std::vector<std::u16string> levelNames(
        const ClassDetailsValidationGradeCatalog* grade
        )
    {
        std::vector<std::u16string> names;
        if (!grade)
        {
            return names;
        }
        names.reserve(grade->levels.size());
        for (const auto& level : grade->levels)
        {
            names.push_back(level.name);
        }
        return names;
    }

    [[nodiscard]] static std::optional<int> weekdayIndex(
        std::u16string_view value
        )
    {
        static constexpr std::u16string_view weekdays[]{
            u"Monday", u"Tuesday", u"Wednesday", u"Thursday",
            u"Friday", u"Saturday", u"Sunday"
        };
        const std::u16string trimmed = trim(value);
        for (std::size_t index = 0; index < std::size(weekdays); ++index)
        {
            if (equalsCaseInsensitive(trimmed, weekdays[index]))
            {
                return static_cast<int>(index);
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::u16string weekdayName(int index)
    {
        static constexpr std::u16string_view weekdays[]{
            u"Monday", u"Tuesday", u"Wednesday", u"Thursday",
            u"Friday", u"Saturday", u"Sunday"
        };
        return std::u16string(weekdays[index]);
    }

    [[nodiscard]] static std::optional<ParsedTime> parseTime(
        std::u16string_view value
        )
    {
        const std::u16string trimmed = trim(value);
        if (trimmed.size() == 5
            && trimmed[2] == u':'
            && isDigit(trimmed[0])
            && isDigit(trimmed[1])
            && isDigit(trimmed[3])
            && isDigit(trimmed[4]))
        {
            const int hour = twoDigits(trimmed[0], trimmed[1]);
            const int minute = twoDigits(trimmed[3], trimmed[4]);
            if (hour <= 23 && minute <= 59)
            {
                const int minutes = hour * 60 + minute;
                return ParsedTime{minutes, displayTime(minutes)};
            }
        }

        std::u16string upper = trimmed;
        for (char16_t& character : upper)
        {
            character = asciiUpper(character);
        }

        const std::size_t colon = upper.find(u':');
        if (colon == std::u16string::npos
            || (colon != 1 && colon != 2)
            || upper.size() != colon + 6
            || (colon == 2 && upper[0] != u'1')
            || upper[colon + 1] < u'0'
            || upper[colon + 1] > u'5'
            || !isDigit(upper[colon + 2])
            || upper[colon + 3] != u' '
            || (upper.substr(colon + 4) != u"AM"
                && upper.substr(colon + 4) != u"PM"))
        {
            return std::nullopt;
        }

        int hour = 0;
        for (std::size_t index = 0; index < colon; ++index)
        {
            if (!isDigit(upper[index]))
            {
                return std::nullopt;
            }
            hour = hour * 10 + static_cast<int>(upper[index] - u'0');
        }
        if (hour < 1 || hour > 12)
        {
            return std::nullopt;
        }
        const int minute = twoDigits(upper[colon + 1], upper[colon + 2]);
        const int hour24 = (hour % 12)
            + (upper.substr(colon + 4) == u"PM" ? 12 : 0);
        const int minutes = hour24 * 60 + minute;
        return ParsedTime{minutes, displayTime(minutes)};
    }

    [[nodiscard]] static bool isDigit(char16_t value) noexcept
    {
        return value >= u'0' && value <= u'9';
    }

    [[nodiscard]] static int twoDigits(char16_t first, char16_t second) noexcept
    {
        return static_cast<int>(first - u'0') * 10
            + static_cast<int>(second - u'0');
    }

    [[nodiscard]] static std::u16string displayTime(int minutes)
    {
        const int hour24 = minutes / 60;
        const int hour12 = hour24 % 12 == 0 ? 12 : hour24 % 12;
        const int minute = minutes % 60;
        std::u16string value;
        if (hour12 >= 10)
        {
            value.push_back(static_cast<char16_t>(u'0' + hour12 / 10));
        }
        value.push_back(static_cast<char16_t>(u'0' + hour12 % 10));
        value.push_back(u':');
        value.push_back(static_cast<char16_t>(u'0' + minute / 10));
        value.push_back(static_cast<char16_t>(u'0' + minute % 10));
        value += hour24 < 12 ? u" AM" : u" PM";
        return value;
    }

    [[nodiscard]] static bool canonicalColor(
        std::u16string_view value,
        std::u16string* canonical
        )
    {
        std::u16string trimmed = trim(value);
        if (trimmed.size() != 7 || trimmed.front() != u'#')
        {
            return false;
        }
        for (std::size_t index = 1; index < trimmed.size(); ++index)
        {
            const char16_t character = trimmed[index];
            if (!isDigit(character)
                && !(character >= u'a' && character <= u'f')
                && !(character >= u'A' && character <= u'F'))
            {
                return false;
            }
            trimmed[index] = asciiUpper(character);
        }
        *canonical = std::move(trimmed);
        return true;
    }

    static void normalize(
        ClassDetailsValidationInput& input,
        const ClassDetailsValidationCatalog& catalog
        )
    {
        const std::vector<std::u16string> grades = gradeNames(catalog);
        input.classGrade = canonicalChoice(input.classGrade, grades);
        const auto* grade = findGrade(catalog, input.classGrade);
        input.classLevel = canonicalChoice(
            input.classLevel,
            levelNames(grade)
            );
        const auto* level = findLevel(grade, input.classLevel);
        if (grade && level)
        {
            input.readingBook = canonicalChoice(
                input.readingBook,
                level->readingBooks
                );
            input.essayBook = canonicalChoice(
                input.essayBook,
                level->essayBooks
                );
        }
        else
        {
            input.readingBook = trim(input.readingBook);
            input.essayBook = trim(input.essayBook);
        }

        normalizeSchedules(input.regularTimes);
        normalizeSchedules(input.intensiveTimes);
        input.notes = trim(input.notes);
        input.timeFillerActivities = trim(input.timeFillerActivities);

        std::u16string canonical;
        input.classColor = canonicalColor(input.classColor, &canonical)
            ? std::move(canonical)
            : trim(input.classColor);
        input.fontColor = canonicalColor(input.fontColor, &canonical)
            ? std::move(canonical)
            : trim(input.fontColor);
    }

    static void normalizeSchedules(
        std::vector<ClassDetailsScheduleValidationRow>& rows
        )
    {
        for (auto& row : rows)
        {
            if (const auto weekday = weekdayIndex(row.day))
            {
                row.day = weekdayName(*weekday);
            }
            else
            {
                row.day = trim(row.day);
            }

            if (const auto start = parseTime(row.startTime))
            {
                row.startTime = start->canonical;
            }
            else
            {
                row.startTime = trim(row.startTime);
            }

            if (const auto end = parseTime(row.endTime))
            {
                row.endTime = end->canonical;
            }
            else
            {
                row.endTime = trim(row.endTime);
            }
        }
    }

    static void add(
        ClassDetailsValidationOutput& output,
        ClassDetailsValidationCode code,
        ClassDetailsValidationField field,
        std::optional<int> integerValue = std::nullopt,
        std::optional<std::u16string> value = std::nullopt,
        std::optional<std::u16string> start = std::nullopt,
        std::optional<std::u16string> end = std::nullopt,
        std::vector<std::u16string> allowedValues = {},
        std::vector<int> duplicateRows = {},
        std::optional<int> length = std::nullopt,
        std::optional<int> minimum = std::nullopt,
        std::optional<int> maximum = std::nullopt,
        std::optional<ClassDetailsValidationSchedule> schedule = std::nullopt,
        std::size_t row = 0
        )
    {
        output.issues.push_back({
            .code = code,
            .field = field,
            .schedule = schedule,
            .row = row,
            .integerValue = integerValue,
            .value = std::move(value),
            .start = std::move(start),
            .end = std::move(end),
            .allowedValues = std::move(allowedValues),
            .duplicateRows = std::move(duplicateRows),
            .length = length,
            .minimum = minimum,
            .maximum = maximum
        });
    }

    static void validate(
        ClassDetailsValidationOutput& output,
        const ClassDetailsValidationCatalog& catalog
        )
    {
        const ClassDetailsValidationInput& input = output.normalized;
        if (input.classId <= 0)
        {
            add(
                output,
                ClassDetailsValidationCode::ClassIdInvalid,
                ClassDetailsValidationField::ClassId,
                input.classId
                );
        }
        if (input.teacherId == 0 || input.teacherId < -1)
        {
            add(
                output,
                ClassDetailsValidationCode::TeacherIdInvalid,
                ClassDetailsValidationField::TeacherId,
                input.teacherId
                );
        }

        const std::u16string grade = trim(input.classGrade);
        const std::u16string level = trim(input.classLevel);
        if (grade.empty() != level.empty())
        {
            add(
                output,
                grade.empty()
                    ? ClassDetailsValidationCode::GradeRequired
                    : ClassDetailsValidationCode::LevelRequired,
                grade.empty()
                    ? ClassDetailsValidationField::ClassGrade
                    : ClassDetailsValidationField::ClassLevel
                );
        }
        else if (!grade.empty())
        {
            const std::vector<std::u16string> grades = gradeNames(catalog);
            addIfNotAllowed(
                output,
                grade,
                grades,
                ClassDetailsValidationField::ClassGrade
                );

            const auto* gradeCatalog = findGrade(catalog, grade);
            const std::vector<std::u16string> levels =
                levelNames(gradeCatalog);
            addIfNotAllowed(
                output,
                level,
                levels,
                ClassDetailsValidationField::ClassLevel
                );

            if (gradeCatalog && findLevel(gradeCatalog, level))
            {
                const auto* levelCatalog = findLevel(gradeCatalog, level);
                addIfNotAllowed(
                    output,
                    trim(input.readingBook),
                    levelCatalog->readingBooks,
                    ClassDetailsValidationField::ReadingBook
                    );
                addIfNotAllowed(
                    output,
                    trim(input.essayBook),
                    levelCatalog->essayBooks,
                    ClassDetailsValidationField::EssayBook
                    );
            }
        }
        else if (!trim(input.readingBook).empty()
                 || !trim(input.essayBook).empty())
        {
            if (!trim(input.readingBook).empty())
            {
                add(
                    output,
                    ClassDetailsValidationCode::BookRequiresGradeLevel,
                    ClassDetailsValidationField::ReadingBook
                    );
            }
            if (!trim(input.essayBook).empty())
            {
                add(
                    output,
                    ClassDetailsValidationCode::BookRequiresGradeLevel,
                    ClassDetailsValidationField::EssayBook
                    );
            }
        }

        std::u16string ignoredCanonical;
        if (!canonicalColor(input.classColor, &ignoredCanonical))
        {
            add(
                output,
                ClassDetailsValidationCode::InvalidHexColor,
                ClassDetailsValidationField::ClassColor,
                std::nullopt,
                input.classColor
                );
        }
        if (!canonicalColor(input.fontColor, &ignoredCanonical))
        {
            add(
                output,
                ClassDetailsValidationCode::InvalidHexColor,
                ClassDetailsValidationField::FontColor,
                std::nullopt,
                input.fontColor
                );
        }

        if (input.classId <= 0)
        {
            add(
                output,
                ClassDetailsValidationCode::ClassIdInvalid,
                ClassDetailsValidationField::ClassId,
                input.classId
                );
        }
        validateTextLength(
            output,
            input.notes,
            ClassDetailsValidationField::Notes
            );
        validateTextLength(
            output,
            input.timeFillerActivities,
            ClassDetailsValidationField::TimeFillerActivities
            );

        validateSchedules(
            output,
            input.regularTimes,
            ClassDetailsValidationSchedule::Regular
            );
        validateSchedules(
            output,
            input.intensiveTimes,
            ClassDetailsValidationSchedule::Intensive
            );
    }

    static void addIfNotAllowed(
        ClassDetailsValidationOutput& output,
        std::u16string value,
        const std::vector<std::u16string>& allowedValues,
        ClassDetailsValidationField field
        )
    {
        if (std::find(allowedValues.begin(), allowedValues.end(), value)
            != allowedValues.end())
        {
            return;
        }
        add(
            output,
            ClassDetailsValidationCode::ValueNotAllowed,
            field,
            std::nullopt,
            std::move(value),
            std::nullopt,
            std::nullopt,
            allowedValues
            );
    }

    static void validateTextLength(
        ClassDetailsValidationOutput& output,
        const std::u16string& value,
        ClassDetailsValidationField field
        )
    {
        constexpr int MaximumLength = 10000;
        if (value.size() > static_cast<std::size_t>(MaximumLength))
        {
            add(
                output,
                ClassDetailsValidationCode::TextLengthOutOfBounds,
                field,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                {},
                {},
                static_cast<int>(value.size()),
                0,
                MaximumLength
                );
        }
    }

    static void validateSchedules(
        ClassDetailsValidationOutput& output,
        const std::vector<ClassDetailsScheduleValidationRow>& rows,
        ClassDetailsValidationSchedule schedule
        )
    {
        std::vector<SlotGroup> groups;
        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            const auto& row = rows[index];
            const auto weekday = weekdayIndex(row.day);
            const auto start = parseTime(row.startTime);
            const auto end = parseTime(row.endTime);
            if (!weekday)
            {
                add(
                    output,
                    ClassDetailsValidationCode::InvalidWeekday,
                    ClassDetailsValidationField::ScheduleDay,
                    std::nullopt,
                    row.day,
                    std::nullopt,
                    std::nullopt,
                    {},
                    {},
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    schedule,
                    index
                    );
            }
            if (!start)
            {
                add(
                    output,
                    ClassDetailsValidationCode::InvalidTimeFormat,
                    ClassDetailsValidationField::ScheduleStartTime,
                    std::nullopt,
                    row.startTime,
                    std::nullopt,
                    std::nullopt,
                    {},
                    {},
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    schedule,
                    index
                    );
            }
            if (!end)
            {
                add(
                    output,
                    ClassDetailsValidationCode::InvalidTimeFormat,
                    ClassDetailsValidationField::ScheduleEndTime,
                    std::nullopt,
                    row.endTime,
                    std::nullopt,
                    std::nullopt,
                    {},
                    {},
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    schedule,
                    index
                    );
            }
            if (start && end && end->minutes <= start->minutes)
            {
                add(
                    output,
                    ClassDetailsValidationCode::EndNotAfterStart,
                    ClassDetailsValidationField::ScheduleEndTime,
                    std::nullopt,
                    std::nullopt,
                    row.startTime,
                    row.endTime,
                    {},
                    {},
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    schedule,
                    index
                    );
            }

            if (weekday && start && end)
            {
                auto group = std::find_if(
                    groups.begin(),
                    groups.end(),
                    [&](const SlotGroup& candidate)
                    {
                        return candidate.weekday == *weekday
                            && candidate.startMinute == start->minutes
                            && candidate.endMinute == end->minutes;
                    }
                    );
                if (group == groups.end())
                {
                    groups.push_back({
                        .weekday = *weekday,
                        .startMinute = start->minutes,
                        .endMinute = end->minutes,
                        .rows = {static_cast<int>(index)}
                    });
                }
                else
                {
                    group->rows.push_back(static_cast<int>(index));
                }
            }
        }

        // Duplicate diagnostics historically followed all row diagnostics.
        // QHash gave groups no defined relative order; first-seen order makes
        // the typed boundary deterministic while preserving each group payload.
        for (const SlotGroup& group : groups)
        {
            if (group.rows.size() < 2)
            {
                continue;
            }
            for (const int row : group.rows)
            {
                add(
                    output,
                    ClassDetailsValidationCode::DuplicateSlot,
                    ClassDetailsValidationField::ScheduleStartTime,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    {},
                    group.rows,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    schedule,
                    static_cast<std::size_t>(row)
                    );
            }
        }
    }
};

} // namespace ClassMngr::Next::Application
