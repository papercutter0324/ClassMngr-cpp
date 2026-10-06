#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Application
{

struct ClassDeleteRequest final
{
    Domain::ClassId classId;

    [[nodiscard]] Domain::Result<void> validate() const
    {
        const std::string& value = classId.value();
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return invalidInput();
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0
            || std::to_string(parsed) != value)
        {
            return invalidInput();
        }

        return Domain::Result<void>::success();
    }

private:
    [[nodiscard]] static Domain::Result<void> invalidInput()
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Class ID must be a canonical positive integer.",
            .recoverable = true
        });
    }
};

using ClassDeleteResult = Domain::Result<void>;

class ClassDeletePort
{
public:
    virtual ~ClassDeletePort() = default;

    [[nodiscard]] virtual ClassDeleteResult deleteClass(
        const ClassDeleteRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
