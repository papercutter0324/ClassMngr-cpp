#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

class CalendarEventRules final
{
public:
    [[nodiscard]] static std::string normalizedEventType(
        std::string_view eventType
        );
    [[nodiscard]] static std::string normalizedTimeStatus(
        std::string_view timeStatus
        );
    [[nodiscard]] static bool isStartOfTerm(
        std::string_view title,
        std::string_view eventType
        );
    [[nodiscard]] static bool eventMatchesCampus(
        std::string_view title,
        const std::vector<std::string>& currentCampusCodes,
        const std::vector<std::string>& allCampusCodes,
        bool showAllCampuses
        );
};

} // namespace classmngr::engine
