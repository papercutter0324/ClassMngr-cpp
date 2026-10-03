#pragma once

#include "next/domain/domain_types.h"

#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct MyClassesScheduleEntry final
{
    std::u16string day;
    std::u16string startTime;
    std::u16string endTime;

    friend bool operator==(
        const MyClassesScheduleEntry&,
        const MyClassesScheduleEntry&
        ) = default;
};

struct MyClassesClassInformationFields final
{
    std::u16string classGrade;
    std::u16string classLevel;
    std::vector<MyClassesScheduleEntry> regularSchedule;
    std::vector<MyClassesScheduleEntry> intensiveSchedule;
    std::u16string notes;
    std::u16string timeFillerActivities;
    std::optional<Domain::TeacherId> teacherId;

    friend bool operator==(
        const MyClassesClassInformationFields&,
        const MyClassesClassInformationFields&
        ) = default;
};

struct MyClassesClassInformationSnapshot final
{
    Domain::ClassId classId;
    MyClassesClassInformationFields fields;
};

} // namespace ClassMngr::Next::Application
