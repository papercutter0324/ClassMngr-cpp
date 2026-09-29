#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Application
{

struct TestingClassDeleteRequest final
{
    Domain::ClassId classId;

    [[nodiscard]] Domain::Result<void> validate() const
    {
        if (!isCanonicalPositiveInteger(classId.value()))
        {
            return invalidInput(
                "Testing class ID must be a canonical positive integer."
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

using TestingClassDeleteResult = Domain::Result<void>;

class TestingClassDeletePort
{
public:
    virtual ~TestingClassDeletePort() = default;

    [[nodiscard]] virtual TestingClassDeleteResult deleteTestingClass(
        const TestingClassDeleteRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
