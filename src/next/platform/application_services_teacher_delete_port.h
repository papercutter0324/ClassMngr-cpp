#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/teacher_delete.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesTeacherDeletePort final
    : public Application::TeacherDeletePort
{
public:
    explicit ApplicationServicesTeacherDeletePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesTeacherDeletePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesTeacherDeletePort(
        const ApplicationServicesTeacherDeletePort&
        ) = delete;
    ApplicationServicesTeacherDeletePort& operator=(
        const ApplicationServicesTeacherDeletePort&
        ) = delete;
    ApplicationServicesTeacherDeletePort(
        ApplicationServicesTeacherDeletePort&&
        ) = delete;
    ApplicationServicesTeacherDeletePort& operator=(
        ApplicationServicesTeacherDeletePort&&
        ) = delete;

    [[nodiscard]] Application::TeacherDeleteResult deleteTeacher(
        const Application::TeacherDeleteRequest& request
        ) const override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return Application::TeacherDeleteResult::failure(
                validation.error()
                );
        }

        const int teacherId = legacyPositiveInteger(request.teacherId.value());
        if (teacherId <= 0)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Teacher ID must be a canonical positive integer."
                );
        }

        try
        {
            DatabaseSession* const session =
                m_services ? m_services->databaseSession() : nullptr;
            if (!session || !session->isOpen())
            {
                return unavailableFailure();
            }

            TeacherRepository* const repository = session->teacherRepository();
            if (!repository)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "The teacher repository is unavailable."
                    );
            }

            const Status deleted = repository->deleteTeacher(teacherId);
            if (!deleted)
            {
                return Application::TeacherDeleteResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = toStdString(deleted.error()),
                    .recoverable = false
                });
            }

            return Application::TeacherDeleteResult::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The teacher could not be deleted."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The teacher could not be deleted."
                );
        }
    }

private:
    [[nodiscard]] static int legacyPositiveInteger(
        const std::string& value
        )
    {
        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0
            || std::to_string(parsed) != value)
        {
            return -1;
        }

        return parsed;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
        };
    }

    [[nodiscard]] static Application::TeacherDeleteResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "The teacher could not be deleted.";
        }

        return Application::TeacherDeleteResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Technical
        });
    }

    [[nodiscard]] static Application::TeacherDeleteResult unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for teachers is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
