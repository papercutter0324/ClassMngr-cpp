#pragma once

#include "next/application/qt_compatible_text.h"
#include "next/domain/domain_types.h"
#include "next/domain/gregorian_date.h"
#include "next/domain/operation_result.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t
    kCalendarEventSeriesDeleteMaxRepeatSeriesIdLength = 128;

// A bounded, adapter-neutral request for deleting one repeat-series suffix.
// The request carries a valid domain date before reaching the port.
struct CalendarEventSeriesDeleteRequest final
{
    std::optional<Domain::CalendarEventSeriesId> repeatSeriesId;
    Domain::GregorianDate startDate;

    [[nodiscard]] Domain::Result<void> validate() const;

    friend bool operator==(
        const CalendarEventSeriesDeleteRequest&,
        const CalendarEventSeriesDeleteRequest&
        ) = default;
};

// Raw date text from the UI is converted at this boundary, keeping invalid
// input out of the typed request and platform port.
struct CalendarEventSeriesDeleteInput final
{
    std::optional<Domain::CalendarEventSeriesId> repeatSeriesId;
    std::string startDate;

    [[nodiscard]] Domain::Result<CalendarEventSeriesDeleteRequest>
    toRequest() const;
};

namespace CalendarEventSeriesDeleteRequestDetail
{

inline constexpr char kInvalidRequestMessage[] =
    "Calendar repeat-series delete request must contain a non-blank "
    "bounded series identifier and a valid ISO start date.";

[[nodiscard]] inline bool isBlank(
    const std::string_view value
    ) noexcept
{
    return isBlankUtf8Text(value);
}

[[nodiscard]] inline Domain::Result<void> validateRepeatSeriesId(
    const std::optional<Domain::CalendarEventSeriesId>& repeatSeriesId
    )
{
    using CalendarEventSeriesDeleteRequestDetail::isBlank;

    if (!repeatSeriesId.has_value()
        || repeatSeriesId->value().empty()
        || repeatSeriesId->value().size()
            > kCalendarEventSeriesDeleteMaxRepeatSeriesIdLength
        || isBlank(repeatSeriesId->value()))
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = kInvalidRequestMessage,
            .recoverable = false
        });
    }

    return Domain::Result<void>::success();
}

} // namespace CalendarEventSeriesDeleteRequestDetail

[[nodiscard]] inline Domain::Result<void>
validateCalendarEventSeriesDeleteRequest(
    const CalendarEventSeriesDeleteRequest& request
    )
{
    return CalendarEventSeriesDeleteRequestDetail::validateRepeatSeriesId(
        request.repeatSeriesId
        );
}

inline Domain::Result<void>
CalendarEventSeriesDeleteRequest::validate() const
{
    return validateCalendarEventSeriesDeleteRequest(*this);
}

inline Domain::Result<CalendarEventSeriesDeleteRequest>
CalendarEventSeriesDeleteInput::toRequest() const
{
    const Domain::Result<void> identifierValidation =
        CalendarEventSeriesDeleteRequestDetail::validateRepeatSeriesId(
            repeatSeriesId
            );
    if (!identifierValidation)
    {
        return Domain::Result<CalendarEventSeriesDeleteRequest>::failure(
            identifierValidation.error()
            );
    }

    const std::optional<Domain::GregorianDate> parsedStartDate =
        Domain::GregorianDate::fromIsoString(startDate);
    if (!parsedStartDate)
    {
        return Domain::Result<CalendarEventSeriesDeleteRequest>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message =
                CalendarEventSeriesDeleteRequestDetail::kInvalidRequestMessage,
            .recoverable = false
        });
    }

    return Domain::Result<CalendarEventSeriesDeleteRequest>::success({
        repeatSeriesId,
        *parsedStartDate
    });
}

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
