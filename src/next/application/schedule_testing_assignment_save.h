#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Application
{

enum class ScheduleTestingAssignmentMutation
{
    RemoveAssignment,
    AssignTestingClass,
    SavePlainTesting
};

struct ScheduleTestingAssignmentSaveRequest final
{
    ScheduleTestingAssignmentMutation mutation =
        ScheduleTestingAssignmentMutation::SavePlainTesting;
    std::u16string day;
    std::u16string startTime;
    std::u16string room;
    std::optional<Domain::ClassId> classId;
    bool replaceExisting = false;

    [[nodiscard]] Domain::Result<void> validate() const
    {
        if (day.empty() || startTime.empty())
        {
            return invalidInput(
                "A testing block requires a valid weekday and start time."
                );
        }

        switch (mutation)
        {
        case ScheduleTestingAssignmentMutation::RemoveAssignment:
        case ScheduleTestingAssignmentMutation::SavePlainTesting:
            if (classId)
            {
                return invalidInput(
                    "A class ID is only valid for a testing class assignment."
                    );
            }
            return Domain::Result<void>::success();

        case ScheduleTestingAssignmentMutation::AssignTestingClass:
            if (!classId || !isValidClassId(*classId))
            {
                return invalidInput("A valid testing class is required.");
            }
            return Domain::Result<void>::success();
        }

        return invalidInput("Testing assignment action is invalid.");
    }

private:
    [[nodiscard]] static bool isValidClassId(
        const Domain::ClassId& id
        )
    {
        const std::string& value = id.value();
        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        return error == std::errc{}
            && end == value.data() + value.size()
            && parsed > 0;
    }

    [[nodiscard]] static Domain::Result<void> invalidInput(
        std::string message
        )
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = std::move(message),
            .recoverable = true
        });
    }
};

using ScheduleTestingAssignmentSaveResult = Domain::Result<void>;

class ScheduleTestingAssignmentSavePort
{
public:
    virtual ~ScheduleTestingAssignmentSavePort() = default;

    [[nodiscard]] virtual ScheduleTestingAssignmentSaveResult
    saveTestingAssignment(
        const ScheduleTestingAssignmentSaveRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
