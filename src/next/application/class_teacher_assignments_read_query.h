#pragma once

#include "next/application/class_teacher_assignments_read_port.h"

#include <charconv>
#include <string>
#include <system_error>

namespace ClassMngr::Next::Application
{

class ClassTeacherAssignmentsReadQuery final
{
public:
    explicit ClassTeacherAssignmentsReadQuery(
        const ClassTeacherAssignmentsReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] ClassTeacherAssignmentsReadResult execute() const
    {
        auto result = m_port.readClassTeacherAssignments();
        if (!result)
        {
            return ClassTeacherAssignmentsReadResult::failure(
                result.error()
                );
        }

        for (ClassTeacherAssignmentReadRow& assignment :
             result.value().assignments)
        {
            if (assignment.teacherId
                && !isCanonicalPositiveTeacherId(
                    assignment.teacherId->value()))
            {
                assignment.teacherId.reset();
            }
        }

        return result;
    }

private:
    [[nodiscard]] static bool isCanonicalPositiveTeacherId(
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

    const ClassTeacherAssignmentsReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
