#pragma once

#include "next/application/teacher_import_latest_source_date_read_port.h"
#include "next/domain/gregorian_date.h"

namespace ClassMngr::Next::Application
{

using TeacherImportLatestSourceDateReadResult =
    Domain::Result<std::optional<Domain::GregorianDate>>;

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
        TeacherImportLatestSourceDateReadPortResult result =
            m_port.readLatestTeacherImportSourceDate();
        if (!result)
        {
            return TeacherImportLatestSourceDateReadResult::failure(
                result.error()
                );
        }

        const std::optional<std::string>& sourceDateText = result.value();
        if (!sourceDateText)
        {
            return TeacherImportLatestSourceDateReadResult::success(std::nullopt);
        }

        const std::optional<Domain::GregorianDate> sourceDate =
            Domain::GregorianDate::fromIsoString(*sourceDateText);
        if (!sourceDate)
        {
            return TeacherImportLatestSourceDateReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message =
                    "Teacher import source date must be a valid Gregorian ISO date.",
                .recoverable = false
            });
        }

        return TeacherImportLatestSourceDateReadResult::success(sourceDate);
    }

private:
    const TeacherImportLatestSourceDateReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
