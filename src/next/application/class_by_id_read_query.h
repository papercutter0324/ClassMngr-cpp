#pragma once

#include "next/application/class_by_id_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

namespace ClassMngr::Next::Application
{

class ClassByIdReadQuery final
{
public:
    explicit ClassByIdReadQuery(
        const ClassByIdReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] ClassByIdReadResult execute(
        const Domain::ClassId& classId
        ) const
    {
        if (!isCanonicalPositiveDecimalId(classId.value()))
        {
            return ClassByIdReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        ClassByIdReadResult result = m_port.readClassById(classId);
        if (!result)
        {
            return ClassByIdReadResult::failure(result.error());
        }

        if (result.value().classId != classId)
        {
            return ClassByIdReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "Class read returned a different class identifier.",
                .recoverable = false
            });
        }

        return result;
    }

private:
    const ClassByIdReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
