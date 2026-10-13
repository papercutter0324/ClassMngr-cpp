#pragma once

#include "next/application/class_co_teacher_teacher_choices_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

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
            if (!isCanonicalPositiveDecimalId64(teacher.teacherId.value()))
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
    const ClassCoTeacherTeacherChoicesReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
