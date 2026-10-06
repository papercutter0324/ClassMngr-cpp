#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <string>
#include <system_error>

namespace ClassMngr::Next::Application
{

struct TeacherDeleteRequest final
{
    Domain::TeacherId teacherId;

    [[nodiscard]] Domain::Result<void> validate() const
    {
        const std::string& value = teacherId.value();
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
            .message = "Teacher ID must be a canonical positive integer.",
            .recoverable = true
        });
    }
};

using TeacherDeleteResult = Domain::Result<void>;

class TeacherDeletePort
{
public:
    virtual ~TeacherDeletePort() = default;

    [[nodiscard]] virtual TeacherDeleteResult deleteTeacher(
        const TeacherDeleteRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
