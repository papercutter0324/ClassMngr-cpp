#include "classmngr/engine/calendar_event_rules.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using classmngr::engine::CalendarEventRules;

bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineCalendarEventRulesTests: "
              << message
              << '\n';
    return false;
}
} // namespace

int main()
{
    bool passed = true;

    const auto& eventTypes = CalendarEventRules::eventTypes();
    passed &= expect(
        eventTypes.size() == 6
            && eventTypes[0] == "Vacation"
            && eventTypes[1] == "Holiday"
            && eventTypes[2] == "Workshop"
            && eventTypes[3] == "CM"
            && eventTypes[4] == "Meeting"
            && eventTypes[5] == "Other",
        "event type catalog changed"
        );

    const auto& timeStatuses = CalendarEventRules::timeStatuses();
    passed &= expect(
        timeStatuses.size() == 3
            && timeStatuses[0] == "Timed"
            && timeStatuses[1] == "Unknown"
            && timeStatuses[2] == "Unconfirmed",
        "time status catalog changed"
        );

    passed &= expect(
        CalendarEventRules::normalizedEventType(" Holiday ") == "Holiday"
            && CalendarEventRules::normalizedEventType("holiday") == "Other"
            && CalendarEventRules::normalizedEventType("Unknown") == "Other",
        "event type normalization changed"
        );
    passed &= expect(
        CalendarEventRules::normalizedTimeStatus(" Unknown ") == "Unknown"
            && CalendarEventRules::normalizedTimeStatus("unknown") == "Timed",
        "time status normalization changed"
        );

    passed &= expect(
        CalendarEventRules::isStartOfTerm("  New   Semester  ", "Other")
            && CalendarEventRules::isStartOfTerm("Term Starts", "invalid")
            && !CalendarEventRules::isStartOfTerm(
                "New Semester",
                "Holiday"
                ),
        "start-of-term recognition changed"
        );

    const std::vector<std::string> allCodes{"bdg", "S-2"};
    const std::vector<std::string> bdg{" BDG "};
    const std::vector<std::string> s2{"s-2"};
    const std::vector<std::string> snu{"SNU"};
    passed &= expect(
        CalendarEventRules::eventMatchesCampus(
            "Workshop (BDG)",
            bdg,
            allCodes,
            false
            )
            && !CalendarEventRules::eventMatchesCampus(
                "Workshop (BDG)",
                snu,
                allCodes,
                false
                )
            && CalendarEventRules::eventMatchesCampus(
                "Workshop S-2",
                s2,
                {"S-2"},
                false
                ),
        "known campus matching changed"
        );
    passed &= expect(
        !CalendarEventRules::eventMatchesCampus(
            "BDGX S-2 workshop",
            bdg,
            allCodes,
            false
            )
            && CalendarEventRules::eventMatchesCampus(
                "General workshop",
                snu,
                allCodes,
                false
                )
            && CalendarEventRules::eventMatchesCampus(
                "BDGX S-2 workshop",
                snu,
                allCodes,
                true
                )
            && CalendarEventRules::eventMatchesCampus(
                "Campus event",
                {},
                allCodes,
                false
                ),
        "campus token boundaries and fallback behavior changed"
        );

    return passed ? 0 : 1;
}
