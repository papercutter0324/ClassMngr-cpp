#pragma once

#include "next/application/class_teacher_assignments_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

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
                && !isCanonicalPositiveDecimalId(
                    assignment.teacherId->value()))
            {
                assignment.teacherId.reset();
            }
        }

        return result;
    }

private:
    const ClassTeacherAssignmentsReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
