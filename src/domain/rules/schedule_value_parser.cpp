#include "schedule_value_parser.h"

#include <array>

namespace ScheduleValueParser
{
namespace
{
struct WeekdayEntry
{
    Weekday value;
    const char* name;
};

constexpr std::array Weekdays{
    WeekdayEntry{Weekday::Monday, "Monday"},
    WeekdayEntry{Weekday::Tuesday, "Tuesday"},
    WeekdayEntry{Weekday::Wednesday, "Wednesday"},
    WeekdayEntry{Weekday::Thursday, "Thursday"},
    WeekdayEntry{Weekday::Friday, "Friday"},
    WeekdayEntry{Weekday::Saturday, "Saturday"},
    WeekdayEntry{Weekday::Sunday, "Sunday"}
};
}

Result<CanonicalWeekday> parseWeekday(QStringView input)
{
    const QString normalized = input.trimmed().toString();

    for (const WeekdayEntry& weekday : Weekdays)
    {
        const QString canonical = QString::fromLatin1(weekday.name);
        if (normalized.compare(canonical, Qt::CaseInsensitive) == 0)
        {
            return CanonicalWeekday{weekday.value, canonical};
        }
    }

    return std::unexpected(QStringLiteral("Invalid weekday."));
}

Result<CanonicalTime> parseTime(QStringView input)
{
    const QString normalized = input.trimmed().toString();
    const QTime parsed = QTime::fromString(normalized, QStringLiteral("HH:mm"));

    if (!parsed.isValid()
        || parsed.toString(QStringLiteral("HH:mm")) != normalized)
    {
        return std::unexpected(QStringLiteral("Invalid time; expected HH:mm."));
    }

    return CanonicalTime{parsed, normalized};
}

} // namespace ScheduleValueParser
