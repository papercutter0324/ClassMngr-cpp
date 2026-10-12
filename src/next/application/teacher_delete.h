#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

struct TeacherDeleteRequest final
{
    Domain::TeacherId teacherId;

    [[nodiscard]] Domain::Result<void> validate() const
    {
        if (!isCanonicalPositiveDecimalId(teacherId.value()))
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
