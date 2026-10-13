#pragma once

#include "next/application/initial_setup_teacher_choices_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

#include <string>
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
            if (!isCanonicalPositiveDecimalId64(teacher.teacherId.value()))
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
