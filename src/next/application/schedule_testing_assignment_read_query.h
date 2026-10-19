#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
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

// An exact owning key for a testing assignment's raw weekday and start-time
// values. These UTF-16 strings are not parsed or normalized on the read path.
class ScheduleTestingAssignmentKey final
{
public:
    ScheduleTestingAssignmentKey() = default;

    explicit ScheduleTestingAssignmentKey(
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
        const ScheduleTestingAssignmentKey&,
        const ScheduleTestingAssignmentKey&
        ) = default;

private:
    std::u16string m_day;
    std::u16string m_startTime;
};

struct ScheduleTestingAssignmentReadRow final
{
    ScheduleTestingAssignmentKey assignmentKey;
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
