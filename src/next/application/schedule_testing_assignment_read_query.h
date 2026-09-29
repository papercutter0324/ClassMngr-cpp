#pragma once

#include "next/domain/operation_result.h"

#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct ScheduleTestingAssignmentSpecialClass final
{
    std::u16string name;
    std::u16string teacherKoreanName;
    std::u16string teacherEnglishName;
    std::u16string teacherPreferredName;
    std::u16string room;
    std::u16string grade;
    std::u16string level;
    std::u16string classColor;
    std::u16string fontColor;

    friend bool operator==(
        const ScheduleTestingAssignmentSpecialClass&,
        const ScheduleTestingAssignmentSpecialClass&
        ) = default;
};

struct ScheduleTestingAssignmentReadRow final
{
    std::u16string day;
    std::u16string startTime;
    std::u16string room;
    int classId{-1};
    std::optional<ScheduleTestingAssignmentSpecialClass> specialClass;

    friend bool operator==(
        const ScheduleTestingAssignmentReadRow&,
        const ScheduleTestingAssignmentReadRow&
        ) = default;
};

struct ScheduleTestingAssignmentReadQuery final
{
    friend bool operator==(
        const ScheduleTestingAssignmentReadQuery&,
        const ScheduleTestingAssignmentReadQuery&
        ) = default;
};

struct ScheduleTestingAssignmentReadSnapshot final
{
    std::vector<ScheduleTestingAssignmentReadRow> rows;

    friend bool operator==(
        const ScheduleTestingAssignmentReadSnapshot&,
        const ScheduleTestingAssignmentReadSnapshot&
        ) = default;
};

using ScheduleTestingAssignmentReadResult =
    Domain::Result<ScheduleTestingAssignmentReadSnapshot>;

class ScheduleTestingAssignmentReadPort
{
public:
    virtual ~ScheduleTestingAssignmentReadPort() = default;

    [[nodiscard]] virtual ScheduleTestingAssignmentReadResult
    readTestingAssignments(
        const ScheduleTestingAssignmentReadQuery& query
        ) const = 0;
};

class ScheduleTestingAssignmentReadQueryHandler final
{
public:
    [[nodiscard]] static ScheduleTestingAssignmentReadResult execute(
        const ScheduleTestingAssignmentReadQuery& query,
        const ScheduleTestingAssignmentReadPort& port
        )
    {
        ScheduleTestingAssignmentReadResult loaded =
            port.readTestingAssignments(query);
        if (!loaded)
        {
            return ScheduleTestingAssignmentReadResult::failure(
                loaded.error()
                );
        }
        return loaded;
    }
};

} // namespace ClassMngr::Next::Application
