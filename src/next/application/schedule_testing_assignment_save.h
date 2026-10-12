#pragma once

#include "domain/rules/schedule_key_rules.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/application/qt_compatible_text.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <string>
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

    template <typename WeekdayCaseInsensitiveEquals>
    [[nodiscard]] Domain::Result<void> validate(
        WeekdayCaseInsensitiveEquals&& weekdayCaseInsensitiveEquals
        ) const
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
            break;

        case ScheduleTestingAssignmentMutation::AssignTestingClass:
            if (!classId || !isValidClassId(*classId))
            {
                return invalidInput("A valid testing class is required.");
            }
            break;
        }

        if (mutation != ScheduleTestingAssignmentMutation::RemoveAssignment
            && mutation != ScheduleTestingAssignmentMutation::SavePlainTesting
            && mutation
                != ScheduleTestingAssignmentMutation::AssignTestingClass)
        {
            return invalidInput("Testing assignment action is invalid.");
        }

        const std::u16string trimmedDay = trimQtWhitespace(day);
        const std::u16string trimmedStartTime = trimQtWhitespace(startTime);
        if (!ScheduleKeyRules::parseWeekday(
                trimmedDay,
                weekdayCaseInsensitiveEquals
                )
            || !ScheduleKeyRules::parseTime(trimmedStartTime))
        {
            return invalidInput(
                "A testing block requires a valid weekday and start time."
                );
        }

        return Domain::Result<void>::success();
    }

private:
    [[nodiscard]] static bool isValidClassId(
        const Domain::ClassId& id
        )
    {
        return parsePositiveDecimalId(id.value()).has_value();
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
