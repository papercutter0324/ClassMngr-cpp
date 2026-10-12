#pragma once

#include "next/application/class_notes_page_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

#include <utility>

namespace ClassMngr::Next::Application
{

class ClassNotesPageReadQuery final
{
public:
    explicit ClassNotesPageReadQuery(
        const ClassNotesPageReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] ClassNotesPageReadResult execute(
        const Domain::ClassId& classId
        ) const
    {
        if (!isCanonicalPositiveDecimalId(classId.value()))
        {
            return ClassNotesPageReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = false
            });
        }

        auto source = m_port.readClassNotesPage(classId);
        if (!source)
        {
            return ClassNotesPageReadResult::failure(source.error());
        }

        auto snapshot = std::move(source.value());
        if (snapshot.classId != classId)
        {
            return ClassNotesPageReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "A class notes page read returned a different class identifier.",
                .recoverable = false
            });
        }

        return ClassNotesPageReadResult::success(std::move(snapshot));
    }

private:
    const ClassNotesPageReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
