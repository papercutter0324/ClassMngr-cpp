#pragma once

#include "next/application/teacher_profile_read_port.h"

#include <charconv>
#include <string>
#include <system_error>

namespace ClassMngr::Next::Application
{

class TeacherProfileReadQuery final
{
public:
    explicit TeacherProfileReadQuery(
        const TeacherProfileReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] TeacherProfileReadResult execute(
        const Domain::TeacherId& id
        ) const
    {
        if (!isCanonicalPositiveId(id))
        {
            return TeacherProfileReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Teacher ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        TeacherProfileReadResult result = m_port.readTeacherProfile(id);
        if (!result)
        {
            return TeacherProfileReadResult::failure(result.error());
        }

        if (result.value().teacherId != id)
        {
            return TeacherProfileReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "Teacher profile read returned a different teacher identifier.",
                .recoverable = false
            });
        }

        return result;
    }

private:
    [[nodiscard]] static bool isCanonicalPositiveId(
        const Domain::TeacherId& id
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
            && parsed > 0
            && std::to_string(parsed) == value;
    }

    const TeacherProfileReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
