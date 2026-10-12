#include "schedule_value_parser.h"

#include "domain/rules/schedule_key_rules.h"

#include <array>
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

[[nodiscard]] QString qtFormat(const DisplayTimeFormat format)
{
    switch (format)
    {
    case DisplayTimeFormat::HourMinuteAmPmSpaced:
        return QStringLiteral("h:mm AP");
    case DisplayTimeFormat::HourMinuteAmPmCompact:
        return QStringLiteral("h:mmAP");
    case DisplayTimeFormat::PaddedHourMinuteAmPmSpaced:
        return QStringLiteral("hh:mm AP");
    case DisplayTimeFormat::PaddedHourMinuteAmPmCompact:
        return QStringLiteral("hh:mmAP");
    case DisplayTimeFormat::HourMinute24Hour:
        return QStringLiteral("H:mm");
    case DisplayTimeFormat::PaddedHourMinute24Hour:
        return QStringLiteral("HH:mm");
    case DisplayTimeFormat::HourMinute24HourWithSeconds:
        return QStringLiteral("H:mm:ss");
    case DisplayTimeFormat::PaddedHourMinute24HourWithSeconds:
        return QStringLiteral("HH:mm:ss");
    }

    return {};
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

Result<QTime> parseDisplayTime(QStringView input)
{
    constexpr std::array allDisplayFormats{
        DisplayTimeFormat::HourMinuteAmPmSpaced,
        DisplayTimeFormat::HourMinuteAmPmCompact,
        DisplayTimeFormat::PaddedHourMinuteAmPmSpaced,
        DisplayTimeFormat::PaddedHourMinuteAmPmCompact,
        DisplayTimeFormat::HourMinute24Hour,
        DisplayTimeFormat::PaddedHourMinute24Hour,
        DisplayTimeFormat::HourMinute24HourWithSeconds,
        DisplayTimeFormat::PaddedHourMinute24HourWithSeconds
    };

    return parseDisplayTime(input, allDisplayFormats);
}

Result<QTime> parseDisplayTime(
    QStringView input,
    const std::span<const DisplayTimeFormat> allowedFormats
    )
{
    const QString trimmed = input.trimmed().toString();
    if (trimmed.isEmpty())
    {
        return std::unexpected(
            QStringLiteral("Invalid display time.")
            );
    }

    for (const DisplayTimeFormat allowedFormat : allowedFormats)
    {
        const QString format = qtFormat(allowedFormat);
        if (format.isEmpty())
        {
            continue;
        }

        const QTime time = QTime::fromString(trimmed, format);
        if (time.isValid())
        {
            return time;
        }
    }

    return std::unexpected(
        QStringLiteral("Invalid display time.")
        );
}

} // namespace ScheduleValueParser
