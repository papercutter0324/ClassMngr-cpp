#pragma once

// Compatibility-only Qt parser retained for SharedPolicy tests. New
// production callers must use the portable engine schedule rule APIs.

#include "core/result.h"

#include <QString>
#include <QStringView>
#include <QTime>

namespace ScheduleValueParser
{

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

} // namespace ScheduleValueParser
