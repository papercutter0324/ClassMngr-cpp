#include "next/application/schedule_import_schedules_cleared_projection.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;
using ClassMngr::Next::Domain::ClassId;
using ClassMngr::Next::Domain::TeacherId;

namespace
{
void require(const bool condition)
{
    if (!condition)
        throw std::runtime_error("Schedule import schedules-cleared assertion failed");
}

ClassId classId(const std::string& value)
{
    return *ClassId::fromString(value);
}

std::vector<ScheduleImportStateReadTime> oneTime()
{
    return {{u"Tuesday", u"4:00 PM", u"4:50 PM"}};
}

ScheduleImportStateReadClassSnapshot classroom(
    const std::string& id,
    std::vector<ScheduleImportStateReadTime> normalTimes,
    std::vector<ScheduleImportStateReadTime> intensiveTimes
    )
{
    return {
        classId(id),
        *TeacherId::fromString("-1"),
        u"Class",
        u"E4",
        u"Hercules",
        u"#FFFFFF",
        std::move(normalTimes),
        std::move(intensiveTimes),
        u"413"
    };
}

std::optional<ClassId> maybeClassId(const std::string& value)
{
    return ClassId::fromString(value);
}
}

int main()
{
    ScheduleImportStateSnapshot snapshot;
    snapshot.classes = {
        classroom("1", oneTime(), {}),
        classroom("2", {}, oneTime()),
        classroom("3", oneTime(), oneTime()),
        classroom("4", {}, {}),
        // The legacy dialog compared parsed integer IDs, so a full-parse
        // persisted value with leading zeroes still matches target 5.
        classroom("0005", oneTime(), {}),
        // A partial parse must not match a selected numeric target.
        classroom("5suffix", oneTime(), {})
    };

    ScheduleImportReviewDecisionRequest decisions;
    decisions.classes = {
        {0, ScheduleImportReviewClassAction::UpdateExisting, maybeClassId("1")},
        {1, ScheduleImportReviewClassAction::Skip, maybeClassId("2")},
        {2, ScheduleImportReviewClassAction::UpdateExisting, maybeClassId("5")}
    };

    require(projectScheduleImportSchedulesCleared(
                snapshot,
                decisions,
                ScheduleImportStateKind::Normal,
                ScheduleImportStateIntensiveMode::ReplaceWithNew
                ) == 2);
    require(projectScheduleImportSchedulesCleared(
                snapshot,
                decisions,
                ScheduleImportStateKind::Intensive,
                ScheduleImportStateIntensiveMode::ReplaceWithNew
                ) == 1);

    const ScheduleImportReviewDecisionRequest noTargets;
    require(projectScheduleImportSchedulesCleared(
                snapshot,
                noTargets,
                ScheduleImportStateKind::Normal,
                ScheduleImportStateIntensiveMode::ReplaceWithNew
                ) == 4);
    require(projectScheduleImportSchedulesCleared(
                snapshot,
                noTargets,
                ScheduleImportStateKind::Normal,
                ScheduleImportStateIntensiveMode::UpdateExisting
                ) == 4);
    require(projectScheduleImportSchedulesCleared(
                snapshot,
                noTargets,
                ScheduleImportStateKind::Intensive,
                ScheduleImportStateIntensiveMode::UpdateExisting
                ) == 0);
}
