#pragma once

#include "next/application/calendar_event_query_port.h"

namespace ClassMngr::Next::Application
{

// The application boundary carries the existing adapter-neutral date value
// and exposes only the dismissal write. Legacy settings storage belongs to
// the platform adapter.
class UpcomingBirthdayDismissalPort
{
public:
    virtual ~UpcomingBirthdayDismissalPort() = default;

    virtual void write(
        const CalendarEventDate& date
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
