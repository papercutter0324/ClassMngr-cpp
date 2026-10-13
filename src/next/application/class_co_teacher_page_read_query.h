#pragma once

#include "next/application/class_co_teacher_page_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

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
        if (!isCanonicalPositiveDecimalId(classId.value()))
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

        if (snapshot.classFields
            && snapshot.classFields.value().selectedTeacherId
            && !isCanonicalPositiveDecimalId64(
                snapshot.classFields.value().selectedTeacherId->value()
                ))
        {
            return ClassCoTeacherPageReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "A co-teacher page read returned an invalid teacher identifier.",
                .recoverable = false
            });
        }

        return ClassCoTeacherPageReadResult::success(std::move(snapshot));
    }

private:
    const ClassCoTeacherPageReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
