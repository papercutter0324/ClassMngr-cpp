#include "schedule_value_parser.h"

#include "domain/rules/schedule_key_rules.h"

#include <string>
#include <string_view>

namespace ScheduleValueParser
{
namespace
{
[[nodiscard]] bool qtCaseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return QString::fromStdU16String(std::u16string(left)).compare(
               QString::fromStdU16String(std::u16string(right)),
               Qt::CaseInsensitive
               ) == 0;
}

[[nodiscard]] Weekday toQtWeekday(const ScheduleKeyRules::Weekday weekday)
{
    switch (weekday)
    {
    case ScheduleKeyRules::Weekday::Monday:
        return Weekday::Monday;
    case ScheduleKeyRules::Weekday::Tuesday:
        return Weekday::Tuesday;
    case ScheduleKeyRules::Weekday::Wednesday:
        return Weekday::Wednesday;
    case ScheduleKeyRules::Weekday::Thursday:
        return Weekday::Thursday;
    case ScheduleKeyRules::Weekday::Friday:
        return Weekday::Friday;
    case ScheduleKeyRules::Weekday::Saturday:
        return Weekday::Saturday;
    case ScheduleKeyRules::Weekday::Sunday:
        return Weekday::Sunday;
    }

    return Weekday::Monday;
}
}

Result<CanonicalWeekday> parseWeekday(QStringView input)
{
    const QString normalized = input.trimmed().toString();
    const std::u16string normalizedUtf16 = normalized.toStdU16String();
    const auto parsed = ScheduleKeyRules::parseWeekday(
        normalizedUtf16,
        qtCaseInsensitiveEquals
        );
    if (!parsed)
    {
        return std::unexpected(QStringLiteral("Invalid weekday."));
    }

    return CanonicalWeekday{
        toQtWeekday(parsed->value),
        QString::fromStdU16String(std::u16string(parsed->canonicalName))
    };
}

Result<CanonicalTime> parseTime(QStringView input)
{
    const QString normalized = input.trimmed().toString();
    const auto parsed = ScheduleKeyRules::parseTime(
        normalized.toStdU16String()
        );
    if (!parsed)
    {
        return std::unexpected(QStringLiteral("Invalid time; expected HH:mm."));
    }

    return CanonicalTime{
        QTime(
            static_cast<int>(parsed->hour),
            static_cast<int>(parsed->minute)
            ),
        normalized
    };
}

} // namespace ScheduleValueParser
