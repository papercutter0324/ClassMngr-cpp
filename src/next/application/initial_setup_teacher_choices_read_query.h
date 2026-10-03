#pragma once

#include "next/application/initial_setup_teacher_choices_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace ClassMngr::Next::Application
{

class InitialSetupTeacherChoicesReadQuery final
{
public:
    explicit InitialSetupTeacherChoicesReadQuery(
        const InitialSetupTeacherChoicesReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] InitialSetupTeacherChoicesResult execute() const
    {
        InitialSetupTeacherChoicesResult source =
            m_port.readInitialSetupTeacherChoices();
        if (!source)
        {
            return InitialSetupTeacherChoicesResult::failure(
                source.error()
                );
        }

        std::unordered_set<std::string> seenTeacherIds;
        seenTeacherIds.reserve(source.value().teachers.size());
        for (const InitialSetupTeacherChoice& teacher :
             source.value().teachers)
        {
            if (!isCanonicalPositiveTeacherId(teacher.teacherId.value()))
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Initial setup teacher choices contain an invalid teacher identifier."
                    );
            }

            if (!seenTeacherIds.insert(teacher.teacherId.value()).second)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Initial setup teacher choices contain a duplicate teacher identifier."
                    );
            }
        }

        return source;
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

    [[nodiscard]] static InitialSetupTeacherChoicesResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return InitialSetupTeacherChoicesResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    const InitialSetupTeacherChoicesReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
