#pragma once

#include "next/application/native_english_teacher_directory_read_port.h"

#include <string>
#include <unordered_set>
#include <utility>

namespace ClassMngr::Next::Application
{

class NativeEnglishTeacherDirectoryReadQuery final
{
public:
    explicit NativeEnglishTeacherDirectoryReadQuery(
        const NativeEnglishTeacherDirectoryReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] NativeEnglishTeacherDirectoryReadResult execute() const
    {
        NativeEnglishTeacherDirectoryReadResult source =
            m_port.readNativeEnglishTeacherDirectory();
        if (!source)
        {
            return NativeEnglishTeacherDirectoryReadResult::failure(
                source.error()
                );
        }

        std::unordered_set<int> seenIds;
        seenIds.reserve(source.value().size());
        for (const NativeEnglishTeacherDirectoryEntry& teacher :
             source.value())
        {
            const int id = teacher.id.value();
            if (id <= 0)
            {
                return validationFailure(
                    "The Native English Teacher directory contains an invalid teacher identifier."
                    );
            }

            if (!seenIds.insert(id).second)
            {
                return validationFailure(
                    "The Native English Teacher directory contains a duplicate teacher identifier."
                    );
            }
        }

        return source;
    }

private:
    [[nodiscard]] static NativeEnglishTeacherDirectoryReadResult
    validationFailure(
        std::string message
        )
    {
        return NativeEnglishTeacherDirectoryReadResult::failure({
            .code = Domain::ErrorCode::Validation,
            .message = std::move(message),
            .recoverable = false
        });
    }

    const NativeEnglishTeacherDirectoryReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
