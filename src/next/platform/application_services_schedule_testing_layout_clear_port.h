#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_block_repository.h"
#include "next/application/schedule_testing_layout_clear_port.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesScheduleTestingLayoutClearPort final
    : public Application::ScheduleTestingLayoutClearPort
{
public:
    explicit ApplicationServicesScheduleTestingLayoutClearPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesScheduleTestingLayoutClearPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesScheduleTestingLayoutClearPort(
        const ApplicationServicesScheduleTestingLayoutClearPort&
        ) = delete;
    ApplicationServicesScheduleTestingLayoutClearPort& operator=(
        const ApplicationServicesScheduleTestingLayoutClearPort&
        ) = delete;
    ApplicationServicesScheduleTestingLayoutClearPort(
        ApplicationServicesScheduleTestingLayoutClearPort&&
        ) = delete;
    ApplicationServicesScheduleTestingLayoutClearPort& operator=(
        ApplicationServicesScheduleTestingLayoutClearPort&&
        ) = delete;

    [[nodiscard]] bool isAvailable() const override
    {
        try
        {
            DatabaseSession* const session =
                m_services ? m_services->databaseSession() : nullptr;
            return session
                && session->isOpen()
                && session->testingBlockRepository() != nullptr;
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] Application::ScheduleTestingLayoutClearResult
        clearTestingLayout() const override
    {
        try
        {
            DatabaseSession* const session =
                m_services ? m_services->databaseSession() : nullptr;
            if (!session || !session->isOpen())
            {
                return unavailableFailure();
            }

            TestingBlockRepository* const repository =
                session->testingBlockRepository();
            if (!repository)
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The testing block repository is unavailable."
                    );
            }

            const Status cleared = repository->clearTestingAssignments();
            if (!cleared)
            {
                if (!session->isOpen())
                {
                    return unavailableFailure();
                }

                return failure(toStdString(cleared.error()));
            }

            return Application::ScheduleTestingLayoutClearResult::success();
        }
        catch (const std::exception&)
        {
            return failure("The testing layout could not be cleared.");
        }
        catch (...)
        {
            return failure("The testing layout could not be cleared.");
        }
    }

private:
    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Application::ScheduleTestingLayoutClearResult
        failure(std::string message)
    {
        if (message.empty())
        {
            message = "The testing layout could not be cleared.";
        }

        return Application::ScheduleTestingLayoutClearResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static Application::ScheduleTestingLayoutClearResult
        failure(
            const Domain::ErrorCode code,
            std::string message
            )
    {
        return Application::ScheduleTestingLayoutClearResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static Application::ScheduleTestingLayoutClearResult
        unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for testing assignments is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
