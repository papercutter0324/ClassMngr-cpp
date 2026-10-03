#pragma once

#include "next/application/my_classes_class_information_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Application
{

class MyClassesClassInformationReadQuery final
{
public:
    explicit MyClassesClassInformationReadQuery(
        const MyClassesClassInformationReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] MyClassesClassInformationReadResult execute(
        const Domain::ClassId& classId
        ) const
    {
        if (!isCanonicalPositiveInteger(classId.value()))
        {
            return MyClassesClassInformationReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = false
            });
        }

        auto source = m_port.readMyClassesClassInformation(classId);
        if (!source)
        {
            return MyClassesClassInformationReadResult::failure(
                source.error()
                );
        }

        auto snapshot = std::move(source.value());
        if (snapshot.classId != classId)
        {
            return MyClassesClassInformationReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "My Classes read returned a different class identifier.",
                .recoverable = false
            });
        }

        if (snapshot.fields.teacherId
            && !isCanonicalPositiveInteger(
                snapshot.fields.teacherId->value()
                ))
        {
            return MyClassesClassInformationReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "My Classes read returned an invalid teacher identifier.",
                .recoverable = false
            });
        }

        return MyClassesClassInformationReadResult::success(
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

    const MyClassesClassInformationReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
