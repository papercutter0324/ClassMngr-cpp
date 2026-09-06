#include "classmngr/engine/schedule_builder.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using namespace classmngr::engine;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineScheduleBuilderServiceTests: "
              << message
              << '\n';
    return false;
}

ClassInfo sampleInfo()
{
    ClassInfo info;
    info.classId = 7;
    info.teacherKr = "Emma KR";
    info.teacherEn = "Emma";
    info.teacherPreferredName = "Em";
    info.roomNumber = "506";
    info.classGrade = "E4";
    info.classLevel = "Hercules";
    info.classTimes = {{"Monday", "4:00 PM", "4:50 PM"}};
    info.intensiveTimes = {{"Tuesday", "9:05", "9:55"}};
    return info;
}
} // namespace

int main()
{
    bool passed = true;

    const ClassInfo regularInfo = sampleInfo();
    const ScheduleReportBuildResult regular =
        ScheduleBuilderService::build(
            {regularInfo},
            false,
            {"Monday", "Tuesday"}
            );
    passed &= expect(
        regular.days == std::vector<std::string>{"Monday", "Tuesday"}
            && regular.rows.size() == 6
            && regular.rows.front().label == "16:00"
            && regular.rows.back().label == "21:00"
            && regular.scheduleOffset == 0
            && !regular.uses55Endings
            && regular.schedule.at("Monday").at("16:00").size() == 1
            && regular.schedule.at("Monday").at("16:00").front().classId == 7,
        "regular schedule build changed"
        );

    ClassInfo offsetInfo = sampleInfo();
    offsetInfo.classTimes = {{"Monday", "16:55", "17:55"}};
    const ScheduleReportBuildResult offset =
        ScheduleBuilderService::build({offsetInfo}, false, {"Monday"});
    passed &= expect(
        offset.scheduleOffset == 55
            && offset.uses55Endings
            && offset.rows.front().label == "15:55"
            && offset.schedule.at("Monday").at("16:55").size() == 1,
        "55-minute schedule offset changed"
        );

    const ScheduleReportBuildResult intensive =
        ScheduleBuilderService::build(
            {regularInfo},
            true,
            {"Monday", "Tuesday"}
            );
    const auto intensiveMonday = intensive.schedule.find("Monday");
    const bool intensiveHasNoRegularClass =
        intensiveMonday != intensive.schedule.end()
        && intensiveMonday->second.find("16:00")
            == intensiveMonday->second.end();
    passed &= expect(
        intensive.rows.size() == 13
            && intensive.rows.front().label == "09:00"
            && intensive.rows.back().label == "21:00"
            && intensive.scheduleOffset == 0
            && !intensive.uses55Endings
            && intensive.schedule.at("Tuesday").at("09:05").size() == 1
            && intensiveHasNoRegularClass,
        "intensive schedule build changed"
        );

    ClassInfo mixedInfo = sampleInfo();
    mixedInfo.classTimes = {
        {"Wednesday", "not-a-time", "5:00 PM"},
        {"Thursday", "4:00:30 PM", "4:50 PM"},
        {"Friday", "18:00:00", "18:50:00"}
    };
    const ScheduleReportBuildResult filtered =
        ScheduleBuilderService::build(
            {mixedInfo},
            false,
            {"Friday"}
            );
    const auto filteredWednesday = filtered.schedule.find("Wednesday");
    const auto filteredThursday = filtered.schedule.find("Thursday");
    passed &= expect(
        filtered.schedule.at("Friday").at("18:00").size() == 1
            && filteredWednesday == filtered.schedule.end()
            && filteredThursday == filtered.schedule.end(),
        "invalid or hidden schedule times were not filtered"
        );

    return passed ? 0 : 1;
}
