#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/application/teacher_directory_read_port.h"

#include <unordered_set>
#include <utility>

namespace ClassMngr::Next::Application
{

class TeacherDirectoryReadQuery final
{
public:
    explicit TeacherDirectoryReadQuery(
        const TeacherDirectoryReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] TeacherDirectoryReadResult execute() const
    {
        TeacherDirectoryReadResult loaded = m_port.readTeacherDirectory();
        if (!loaded)
        {
            return TeacherDirectoryReadResult::failure(loaded.error());
        }

        std::unordered_set<std::string> seenIds;
        seenIds.reserve(loaded.value().teachers.size());
        for (const TeacherDirectoryEntry& teacher : loaded.value().teachers)
        {
            const std::string& value = teacher.teacherId.value();
            if (!isCanonicalPositiveDecimalId64(value))
            {
                return validationFailure(
                    "Teacher directory returned a noncanonical teacher identifier."
                    );
            }
            if (!seenIds.insert(value).second)
            {
                return validationFailure(
                    "Teacher directory returned a duplicate teacher identifier."
                    );
            }
        }

        return loaded;
    }

private:
    [[nodiscard]] static TeacherDirectoryReadResult validationFailure(
        std::string message
        )
    {
        return TeacherDirectoryReadResult::failure({
            .code = Domain::ErrorCode::Validation,
            .message = std::move(message),
            .recoverable = false
        });
    }

    const TeacherDirectoryReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
