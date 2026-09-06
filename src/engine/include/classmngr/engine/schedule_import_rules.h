#pragma once

#include "classmngr/engine/schedule_import.h"

#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

enum class ScheduleImportMeetingPatternStatus
{
    Valid,
    InvalidWeekdayOrDuplicate,
    UnsupportedPattern
};

enum class ScheduleImportMeetingPatternExpectation
{
    WeekdayPairs,
    WeekdayTripleOrTuesdayThursday,
    OneWeekday,
    Unsupported
};

struct ScheduleImportMeetingPatternResult
{
    ScheduleImportMeetingPatternStatus status =
        ScheduleImportMeetingPatternStatus::Valid;
    ScheduleImportMeetingPatternExpectation expectation =
        ScheduleImportMeetingPatternExpectation::Unsupported;
    std::vector<std::string> meetingDays;
};

class ScheduleImportRules final
{
public:
    [[nodiscard]] static int weekdayIndex(
        std::string_view day
        );

    [[nodiscard]] static int dayGroup(
        const std::vector<ClassTime>& times
        );

    [[nodiscard]] static bool daysAreCompatible(
        const std::vector<ClassTime>& importedTimes,
        const std::vector<ClassTime>& existingTimes
        );

    [[nodiscard]] static std::vector<std::string> meetingDays(
        const std::vector<ClassTime>& times
        );

    [[nodiscard]] static bool meetingDaysMatch(
        const std::vector<ClassTime>& importedTimes,
        const std::vector<ClassTime>& existingTimes
        );

    [[nodiscard]] static std::vector<ClassTime> timesForKind(
        const ClassInfo& info,
        ScheduleImportKind kind
        );

    [[nodiscard]] static const std::vector<ClassTime>& targetTimesForKind(
        const ClassInfo& info,
        ScheduleImportKind kind
        );

    [[nodiscard]] static bool classOptionIsEligible(
        const ScheduleImportClassCandidate& candidate,
        const ClassInfo& existing,
        ScheduleImportKind kind
        );

    [[nodiscard]] static ScheduleImportMeetingPatternExpectation
    meetingPatternExpectation(
        std::string_view classGrade,
        std::string_view classLevel
        );

    [[nodiscard]] static std::vector<std::vector<std::string>>
    allowedDayPatterns(
        std::string_view classGrade,
        std::string_view classLevel
        );

    [[nodiscard]] static ScheduleImportMeetingPatternResult
    validateMeetingPattern(
        const ScheduleImportClassCandidate& candidate
        );
};

} // namespace classmngr::engine
