#pragma once

#include "next/application/calendar_event_query_port.h"

#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct CalendarEventImportSignatureRangeRequest final
{
    CalendarEventDate startDate;
    CalendarEventDate endDate;

    friend bool operator==(
        const CalendarEventImportSignatureRangeRequest&,
        const CalendarEventImportSignatureRangeRequest&
        ) = default;
};

using CalendarEventImportSignatureKeys = std::vector<std::u16string>;
using CalendarEventImportSignatureQueryResult =
    Domain::Result<CalendarEventImportSignatureKeys>;
using CalendarEventImportSignatureQueryError = Domain::OperationError;

// Read-only import de-duplication capability. Results preserve the order and
// exact UTF-16 identity of the legacy QString signature values.
class CalendarEventImportSignatureQueryPort
{
public:
    virtual ~CalendarEventImportSignatureQueryPort() = default;

    [[nodiscard]] virtual bool isAvailable() const noexcept = 0;

    [[nodiscard]] virtual CalendarEventImportSignatureQueryResult
    loadSignaturesInRange(
        const CalendarEventImportSignatureRangeRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
