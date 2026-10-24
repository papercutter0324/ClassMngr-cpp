#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
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
            WorkspaceStore* const store =
                m_services ? m_services->workspaceStore() : nullptr;
            if (!store || !store->isOpen())
            {
                return unavailableFailure();
            }

            ClassRepository* const repository = store->classRepository();
            if (!repository)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "The class repository is unavailable."
                    );
            }

            const RepositoryResult<int> created =
                repository->createClass(QString());
            if (!created)
            {
                return failure(
                    domainErrorCode(created.error().code),
                    toStdString(toLegacyDiagnostic(created.error())),
                    created.error().code != RepositoryErrorCode::Validation
                    );
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
    [[nodiscard]] static Domain::ErrorCode domainErrorCode(
        const RepositoryErrorCode code
        ) noexcept
    {
        switch (code)
        {
        case RepositoryErrorCode::InvalidInput:
            return Domain::ErrorCode::InvalidInput;
        case RepositoryErrorCode::NotFound:
            return Domain::ErrorCode::NotFound;
        case RepositoryErrorCode::Conflict:
            return Domain::ErrorCode::Conflict;
        case RepositoryErrorCode::Validation:
            return Domain::ErrorCode::Validation;
        case RepositoryErrorCode::Technical:
            return Domain::ErrorCode::Technical;
        }
        return Domain::ErrorCode::Technical;
    }

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
        return failure(
            code,
            std::move(message),
            code != Domain::ErrorCode::Technical
            );
    }

    [[nodiscard]] static Application::ClassCreateResult failure(
        const Domain::ErrorCode code,
        std::string message,
        const bool recoverable
        )
    {
        if (message.empty())
        {
            message = "The class could not be created.";
        }

        return Application::ClassCreateResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = recoverable
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
