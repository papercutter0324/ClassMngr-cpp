#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <optional>
#include <string>
#include <system_error>
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
    Domain::ClassId classId = Domain::ClassId::fromString("-1").value();
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

        for (const ScheduleTestingAssignmentReadRow& row : loaded.value().rows)
        {
            if (isValidClassId(row.classId))
            {
                continue;
            }

            return ScheduleTestingAssignmentReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "The schedule testing assignment contains an invalid class identifier.",
                .recoverable = false
            });
        }

        return loaded;
    }

private:
    [[nodiscard]] static bool isValidClassId(
        const Domain::ClassId& classId
        )
    {
        const std::string& value = classId.value();
        if (value == "-1")
        {
            return true;
        }

        int parsedClassId = 0;
        const std::from_chars_result parsed = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsedClassId,
            10
            );

        return parsed.ec == std::errc{}
            && parsed.ptr == value.data() + value.size()
            && parsedClassId > 0;
    }
};

} // namespace ClassMngr::Next::Application
