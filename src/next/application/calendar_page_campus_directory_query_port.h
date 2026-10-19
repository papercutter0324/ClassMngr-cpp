#pragma once

#include "next/domain/domain_types.h"

#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// The campus metadata CalendarPage needs to build campus filter aliases. The
// identifier is typed and remaining text fields are owning UTF-8 values;
// unrelated campus record fields stay outside this query contract.
struct CalendarPageCampusMetadata final
{
    Domain::CampusId id;
    std::string campusName;
    std::optional<std::string> campusCode;
};

class CalendarPageCampusDirectoryQueryPort
{
public:
    virtual ~CalendarPageCampusDirectoryQueryPort() = default;

    [[nodiscard]] virtual std::vector<CalendarPageCampusMetadata>
    loadCampuses() const = 0;
};

} // namespace ClassMngr::Next::Application
