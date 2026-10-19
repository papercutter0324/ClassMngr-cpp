#pragma once

#include "domain/models/calendar_event.h"
#include "next/domain/calendar_event_timing.h"

#include <QByteArray>
#include <QString>

#include <string_view>

namespace ClassMngr::Next::Platform
{

// These conversions stay at Qt/legacy boundaries. The legacy normalizer
// trims Qt whitespace, matches canonical names case-sensitively, and falls
// back to Timed for unsupported persisted text.
[[nodiscard]] inline Domain::CalendarEventTimeStatus
calendarEventTimeStatusFromLegacyText(
    const QString& value
    )
{
    const QByteArray normalized =
        normalizedCalendarEventTimeStatus(value).toUtf8();
    const std::string_view name(
        normalized.constData(),
        static_cast<std::size_t>(normalized.size())
        );
    return Domain::calendarEventTimeStatusFromName(name).value_or(
        Domain::CalendarEventTimeStatus::Timed
        );
}

[[nodiscard]] inline QString calendarEventTimeStatusToLegacyText(
    const Domain::CalendarEventTimeStatus value
    )
{
    const auto name = Domain::calendarEventTimeStatusName(value);
    if (!name)
    {
        return {};
    }

    return QString::fromLatin1(
        name->data(),
        static_cast<qsizetype>(name->size())
        );
}

} // namespace ClassMngr::Next::Platform
