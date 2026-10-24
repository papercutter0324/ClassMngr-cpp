#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
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

            const RepositoryStatus deleted = repository->deleteClass(classId);
            if (!deleted)
            {
                return failure(
                    domainErrorCode(deleted.error().code),
                    toStdString(toLegacyDiagnostic(deleted.error()))
                    );
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
