#pragma once

#include "next/application/class_by_id_read_port.h"

#include <charconv>
#include <string>
#include <system_error>

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
        if (!isCanonicalPositiveId(classId))
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
    [[nodiscard]] static bool isCanonicalPositiveId(
        const Domain::ClassId& classId
        )
    {
        const std::string& value = classId.value();
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

    const ClassByIdReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
