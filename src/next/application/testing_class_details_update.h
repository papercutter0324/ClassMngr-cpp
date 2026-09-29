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
        if (!isCanonicalPositiveInteger(classId.value()))
        {
            return invalidInput(
                "Testing class ID must be a canonical positive integer."
                );
        }

        if (
            teacherId
            && !isCanonicalPositiveInteger(teacherId->value())
            )
        {
            return invalidInput(
                "Testing class teacher ID must be a canonical positive integer."
                );
        }

        return Domain::Result<void>::success();
    }

private:
    [[nodiscard]] static bool isCanonicalPositiveInteger(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return false;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        return error == std::errc{}
            && end == value.data() + value.size()
            && parsed > 0
            && std::to_string(parsed) == value;
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
