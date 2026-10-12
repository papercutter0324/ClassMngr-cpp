#pragma once

#include "next/application/teacher_profile_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

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
        if (!isCanonicalPositiveDecimalId(id.value()))
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
    const TeacherProfileReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
