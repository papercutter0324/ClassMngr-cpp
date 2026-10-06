#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "next/application/class_create.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassCreatePort final
    : public Application::ClassCreatePort
{
public:
    explicit ApplicationServicesClassCreatePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesClassCreatePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesClassCreatePort(
        const ApplicationServicesClassCreatePort&
        ) = delete;
    ApplicationServicesClassCreatePort& operator=(
        const ApplicationServicesClassCreatePort&
        ) = delete;
    ApplicationServicesClassCreatePort(
        ApplicationServicesClassCreatePort&&
        ) = delete;
    ApplicationServicesClassCreatePort& operator=(
        ApplicationServicesClassCreatePort&&
        ) = delete;

    [[nodiscard]] Application::ClassCreateResult createClass(
        const Application::ClassCreateRequest& request
        ) const override
    {
        Q_UNUSED(request);

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

            const Result<int> created = repository->createClass(QString());
            if (!created)
            {
                return failure(Domain::ErrorCode::Technical, toStdString(created.error()));
            }
            if (*created <= 0)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "Creating the class did not return a valid ID."
                    );
            }

            const auto classId = Domain::ClassId::fromString(
                std::to_string(*created)
                );
            if (!classId)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "Creating the class did not return a valid ID."
                    );
            }

            return Application::ClassCreateResult::success(*classId);
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The class could not be created."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The class could not be created."
                );
        }
    }

private:
    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    [[nodiscard]] static Application::ClassCreateResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "The class could not be created.";
        }

        return Application::ClassCreateResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Technical
        });
    }

    [[nodiscard]] static Application::ClassCreateResult unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for classes is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
