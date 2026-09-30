#pragma once

#include "next/application/class_co_teacher_page_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Application
{

class ClassCoTeacherPageReadQuery final
{
public:
    explicit ClassCoTeacherPageReadQuery(
        const ClassCoTeacherPageReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] ClassCoTeacherPageReadResult execute(
        const Domain::ClassId& classId
        ) const
    {
        if (!isCanonicalPositiveInteger(classId.value()))
        {
            return ClassCoTeacherPageReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = false
            });
        }

        auto source = m_port.readClassCoTeacherPage(classId);
        if (!source)
        {
            return ClassCoTeacherPageReadResult::failure(source.error());
        }

        auto snapshot = std::move(source.value());
        if (snapshot.classId != classId)
        {
            return ClassCoTeacherPageReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "A co-teacher page read returned a different class identifier.",
                .recoverable = false
            });
        }

        return ClassCoTeacherPageReadResult::success(std::move(snapshot));
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

    const ClassCoTeacherPageReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
