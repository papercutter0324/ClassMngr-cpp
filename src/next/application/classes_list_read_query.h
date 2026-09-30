#pragma once

#include "next/application/classes_list_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace ClassMngr::Next::Application
{

class ClassesListReadQuery final
{
public:
    explicit ClassesListReadQuery(
        const ClassesListReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] ClassesListReadResult execute() const
    {
        ClassesListReadResult source = m_port.readClassesList();
        if (!source)
        {
            return ClassesListReadResult::failure(source.error());
        }

        std::unordered_set<std::string> seenClassIds;
        seenClassIds.reserve(source.value().classes.size());
        for (const ClassesListEntry& classroom : source.value().classes)
        {
            if (!isCanonicalPositiveClassId(classroom.classId.value()))
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "The classes list contains an invalid class identifier."
                    );
            }

            if (!seenClassIds.insert(classroom.classId.value()).second)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "The classes list contains a duplicate class identifier."
                    );
            }
        }

        return source;
    }

private:
    [[nodiscard]] static bool isCanonicalPositiveClassId(
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

    [[nodiscard]] static ClassesListReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return ClassesListReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    const ClassesListReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
