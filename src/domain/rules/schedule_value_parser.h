#pragma once

#include "core/result.h"

#include <QString>
#include <QStringView>
#include <QTime>

#include <span>

namespace ScheduleValueParser
{

enum class DisplayTimeFormat
{
    HourMinuteAmPmSpaced,
    HourMinuteAmPmCompact,
    PaddedHourMinuteAmPmSpaced,
    PaddedHourMinuteAmPmCompact,
    HourMinute24Hour,
    PaddedHourMinute24Hour,
    HourMinute24HourWithSeconds,
    PaddedHourMinute24HourWithSeconds
};

enum class Weekday
{
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday
};

struct CanonicalWeekday
{
    Weekday value = Weekday::Monday;
    QString text;
};

struct CanonicalTime
{
    QTime value;
    QString text;
};

[[nodiscard]] Result<CanonicalWeekday> parseWeekday(QStringView input);
[[nodiscard]] Result<CanonicalTime> parseTime(QStringView input);
[[nodiscard]] Result<QTime> parseDisplayTime(QStringView input);
[[nodiscard]] Result<QTime> parseDisplayTime(
    QStringView input,
    std::span<const DisplayTimeFormat> allowedFormats
    );

} // namespace ScheduleValueParser
