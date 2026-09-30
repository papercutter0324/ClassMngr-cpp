#pragma once

#include "next/application/class_co_teacher_teacher_choices_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Application
{

class ClassCoTeacherTeacherChoicesReadQuery final
{
public:
    explicit ClassCoTeacherTeacherChoicesReadQuery(
        const ClassCoTeacherTeacherChoicesReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] ClassCoTeacherTeacherChoicesResult execute() const
    {
        auto source = m_port.readClassCoTeacherTeacherChoices();
        if (!source)
        {
            return ClassCoTeacherTeacherChoicesResult::failure(
                source.error()
                );
        }

        auto snapshot = std::move(source.value());
        for (const ClassCoTeacherTeacherChoice& teacher : snapshot.teachers)
        {
            if (!isCanonicalPositiveInteger(teacher.teacherId.value()))
            {
                return ClassCoTeacherTeacherChoicesResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message =
                        "Co-teacher teacher choices contain an invalid teacher identifier.",
                    .recoverable = false
                });
            }
        }

        return ClassCoTeacherTeacherChoicesResult::success(
            std::move(snapshot)
            );
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

    const ClassCoTeacherTeacherChoicesReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
