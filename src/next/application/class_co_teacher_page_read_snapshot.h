#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class ClassCoTeacherPageScheduleSlot final
{
public:
    ClassCoTeacherPageScheduleSlot() = default;

    explicit ClassCoTeacherPageScheduleSlot(
        std::u16string day,
        std::u16string startTime
        )
        : m_day(std::move(day))
        , m_startTime(std::move(startTime))
    {
    }

    [[nodiscard]] const std::u16string& day() const noexcept
    {
        return m_day;
    }

    [[nodiscard]] const std::u16string& startTime() const noexcept
    {
        return m_startTime;
    }

    friend bool operator==(
        const ClassCoTeacherPageScheduleSlot&,
        const ClassCoTeacherPageScheduleSlot&
        ) = default;

private:
    std::u16string m_day;
    std::u16string m_startTime;
};

struct ClassCoTeacherPageFields final
{
    std::optional<Domain::TeacherId> selectedTeacherId;
    std::u16string classGrade;
    std::u16string classLevel;
    std::vector<ClassCoTeacherPageScheduleSlot> regularSchedule;

    friend bool operator==(
        const ClassCoTeacherPageFields&,
        const ClassCoTeacherPageFields&
        ) = default;
};

struct ClassCoTeacherPageReadSnapshot final
{
    Domain::ClassId classId;
    Domain::Result<ClassCoTeacherPageFields> classFields;
    Domain::Result<std::u16string> teacherDisplayName;
};

} // namespace ClassMngr::Next::Application
