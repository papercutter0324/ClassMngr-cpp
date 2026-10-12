#pragma once

#include "domain/rules/schedule_key_rules.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/application/testing_class_required_fields_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

struct TestingClassCreateRequest final
{
    std::u16string name;
    std::u16string grade;
    std::u16string level;
    std::u16string room;
    std::optional<Domain::TeacherId> teacherId;
    std::u16string classColor;
    std::u16string fontColor;
    std::u16string notes;
    std::optional<std::u16string> assignmentDay;
    std::optional<std::u16string> assignmentStartTime;

    template <typename WeekdayCaseInsensitiveEquals>
    [[nodiscard]] Domain::Result<void> validate(
        WeekdayCaseInsensitiveEquals&& weekdayCaseInsensitiveEquals
        ) const
    {
        if (
            teacherId
            && !isCanonicalPositiveDecimalId(teacherId->value())
            )
        {
            return invalidInput(
                "Testing class teacher ID must be a canonical positive integer."
                );
        }

        if (assignmentDay.has_value() != assignmentStartTime.has_value())
        {
            return invalidInput(
                "Testing class assignment weekday and start time must be provided together."
                );
        }

        const auto requiredFieldsResult = validateTestingClassRequiredFields(
            name,
            grade,
            level,
            room
            );
        if (!requiredFieldsResult)
        {
            return requiredFieldsResult;
        }

        if (assignmentDay)
        {
            const std::u16string trimmedDay =
                trimQtWhitespace(*assignmentDay);
            const std::u16string trimmedStartTime =
                trimQtWhitespace(*assignmentStartTime);
            if (!trimmedDay.empty() || !trimmedStartTime.empty())
            {
                if (!ScheduleKeyRules::parseWeekday(
                        trimmedDay,
                        weekdayCaseInsensitiveEquals
                        )
                    || !ScheduleKeyRules::parseTime(trimmedStartTime))
                {
                    return validationFailure(
                        "A testing assignment requires a valid weekday and start time."
                        );
                }
            }
        }

        return Domain::Result<void>::success();
    }

private:
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

    [[nodiscard]] static Domain::Result<void> validationFailure(
        std::string message
        )
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Validation,
            .message = std::move(message),
            .recoverable = true
        });
    }
};

using TestingClassCreateResult = Domain::Result<Domain::ClassId>;

class TestingClassCreatePort
{
public:
    virtual ~TestingClassCreatePort() = default;

    [[nodiscard]] virtual TestingClassCreateResult createTestingClass(
        const TestingClassCreateRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
