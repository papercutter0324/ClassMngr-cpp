#pragma once

#include "next/domain/domain_types.h"

#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct RosterPrintScheduleRow final
{
    std::u16string day;
    std::u16string startTime;
    std::u16string endTime;
};

struct RosterPrintClassInfoReadSnapshot final
{
    explicit RosterPrintClassInfoReadSnapshot(
        Domain::ClassId value
        )
        : classId(std::move(value))
    {
    }

    Domain::ClassId classId;
    std::u16string classGrade;
    std::u16string classLevel;
    std::u16string teacherEn;
    std::u16string teacherKr;
    std::u16string roomNumber;
    std::u16string wifiName;
    std::u16string wifiPassword;
    std::u16string zoomId;
    std::u16string zoomPassword;
    std::vector<RosterPrintScheduleRow> regularSchedule;
};

} // namespace ClassMngr::Next::Application
