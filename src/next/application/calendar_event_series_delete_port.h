#pragma once

#include "next/domain/operation_result.h"

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t
    kCalendarEventSeriesDeleteMaxRepeatSeriesIdLength = 128;
inline constexpr std::size_t kCalendarEventSeriesDeleteIsoDateLength = 10;

// A bounded, adapter-neutral request for deleting one repeat-series suffix.
// The date is canonical ISO-8601 calendar text (yyyy-MM-dd); validation and
// conversion to a platform date type belong to the platform adapter.
struct CalendarEventSeriesDeleteRequest final
{
    std::string repeatSeriesId;
    std::string startDate;

    friend bool operator==(
        const CalendarEventSeriesDeleteRequest&,
        const CalendarEventSeriesDeleteRequest&
        ) = default;
};

using CalendarEventSeriesDeleteResult = Domain::Result<void>;
using CalendarEventSeriesDeleteError = Domain::OperationError;

// Adapter-neutral repeat-series deletion. Legacy service pointers and Qt date
// values remain outside this application boundary.
class CalendarEventSeriesDeletePort
{
public:
    virtual ~CalendarEventSeriesDeletePort() = default;

    [[nodiscard]] virtual CalendarEventSeriesDeleteResult
    deleteRepeatSeriesFromDate(
        const CalendarEventSeriesDeleteRequest& request
        ) = 0;
};

} // namespace ClassMngr::Next::Application
