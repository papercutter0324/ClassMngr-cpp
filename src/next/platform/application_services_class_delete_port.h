#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "next/application/class_delete.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassDeletePort final
    : public Application::ClassDeletePort
{
public:
    explicit ApplicationServicesClassDeletePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesClassDeletePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesClassDeletePort(
        const ApplicationServicesClassDeletePort&
        ) = delete;
    ApplicationServicesClassDeletePort& operator=(
        const ApplicationServicesClassDeletePort&
        ) = delete;
    ApplicationServicesClassDeletePort(
        ApplicationServicesClassDeletePort&&
        ) = delete;
    ApplicationServicesClassDeletePort& operator=(
        ApplicationServicesClassDeletePort&&
        ) = delete;

    [[nodiscard]] Application::ClassDeleteResult deleteClass(
        const Application::ClassDeleteRequest& request
        ) const override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return Application::ClassDeleteResult::failure(
                validation.error()
                );
        }

        const int classId = legacyPositiveInteger(request.classId.value());
        if (classId <= 0)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."
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

            ClassRepository* const repository = session->classRepository();
            if (!repository)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "The class repository is unavailable."
                    );
            }

            const Status deleted = repository->deleteClass(classId);
            if (!deleted)
            {
                return Application::ClassDeleteResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = toStdString(deleted.error()),
                    .recoverable = false
                });
            }

            return Application::ClassDeleteResult::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The class could not be deleted."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The class could not be deleted."
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

    [[nodiscard]] static Application::ClassDeleteResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "The class could not be deleted.";
        }

        return Application::ClassDeleteResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Technical
        });
    }

    [[nodiscard]] static Application::ClassDeleteResult unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for classes is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
