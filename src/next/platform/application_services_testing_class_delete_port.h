#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/testing_class_delete.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesTestingClassDeletePort final
    : public Application::TestingClassDeletePort
{
public:
    explicit ApplicationServicesTestingClassDeletePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesTestingClassDeletePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesTestingClassDeletePort(
        const ApplicationServicesTestingClassDeletePort&
        ) = delete;
    ApplicationServicesTestingClassDeletePort& operator=(
        const ApplicationServicesTestingClassDeletePort&
        ) = delete;
    ApplicationServicesTestingClassDeletePort(
        ApplicationServicesTestingClassDeletePort&&
        ) = delete;
    ApplicationServicesTestingClassDeletePort& operator=(
        ApplicationServicesTestingClassDeletePort&&
        ) = delete;

    [[nodiscard]] Application::TestingClassDeleteResult deleteTestingClass(
        const Application::TestingClassDeleteRequest& request
        ) const override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return Application::TestingClassDeleteResult::failure(
                validation.error()
                );
        }

        const int classId = legacyPositiveInteger(request.classId.value());
        if (classId <= 0)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Testing class ID must be a canonical positive integer."
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

            TestingClassRepository* const repository =
                store->testingClassRepository();
            if (!repository)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "Testing class could not be deleted."
                    );
            }

            const RepositoryStatus deleted =
                repository->deleteTestingClassDetailed(classId);
            if (!deleted)
            {
                return failure(
                    mapRepositoryErrorCode(deleted.error().code),
                    toStdString(deleted.error().diagnostic)
                    );
            }

            return Application::TestingClassDeleteResult::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing class could not be deleted."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing class could not be deleted."
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
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    [[nodiscard]] static Domain::ErrorCode mapRepositoryErrorCode(
        const RepositoryErrorCode code
        )
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

    [[nodiscard]] static Application::TestingClassDeleteResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Testing class could not be deleted.";
        }

        return Application::TestingClassDeleteResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Technical
        });
    }

    [[nodiscard]] static Application::TestingClassDeleteResult
    unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for testing classes is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
