#pragma once

#include "next/application/teacher_import_latest_source_date_read_port.h"
#include "next/domain/gregorian_date.h"

namespace ClassMngr::Next::Application
{

class TeacherImportLatestSourceDateReadQuery final
{
public:
    explicit TeacherImportLatestSourceDateReadQuery(
        const TeacherImportLatestSourceDateReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] TeacherImportLatestSourceDateReadResult execute() const
    {
        TeacherImportLatestSourceDateReadResult result =
            m_port.readLatestTeacherImportSourceDate();
        if (!result)
        {
            return TeacherImportLatestSourceDateReadResult::failure(
                result.error()
                );
        }

        const std::optional<std::string>& sourceDate = result.value();
        if (sourceDate.has_value()
            && !Domain::GregorianDate::fromIsoString(*sourceDate).has_value())
        {
            return TeacherImportLatestSourceDateReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message =
                    "Teacher import source date must be a valid Gregorian ISO date.",
                .recoverable = false
            });
        }

        return result;
    }

private:
    const TeacherImportLatestSourceDateReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
