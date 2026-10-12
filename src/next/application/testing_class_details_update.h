#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/application/testing_class_required_fields_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

struct TestingClassDetailsUpdateRequest final
{
    Domain::ClassId classId;
    std::u16string name;
    std::u16string grade;
    std::u16string level;
    std::u16string room;
    std::optional<Domain::TeacherId> teacherId;
    std::u16string classColor;
    std::u16string fontColor;
    std::u16string notes;

    [[nodiscard]] Domain::Result<void> validate() const
    {
        if (!isCanonicalPositiveDecimalId(classId.value()))
        {
            return invalidInput(
                "Testing class ID must be a canonical positive integer."
                );
        }

        if (
            teacherId
            && !isCanonicalPositiveDecimalId(teacherId->value())
            )
        {
            return invalidInput(
                "Testing class teacher ID must be a canonical positive integer."
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

};

using TestingClassDetailsUpdateResult = Domain::Result<void>;

class TestingClassDetailsUpdatePort
{
public:
    virtual ~TestingClassDetailsUpdatePort() = default;

    [[nodiscard]] virtual TestingClassDetailsUpdateResult
    updateTestingClassDetails(
        const TestingClassDetailsUpdateRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
