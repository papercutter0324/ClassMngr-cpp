#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_transfer_repository.h"
#include "next/application/class_transfer_preview_query.h"

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassTransferPreviewReadPort final
    : public Application::ClassTransferPreviewReadPort
{
public:
    explicit ApplicationServicesClassTransferPreviewReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesClassTransferPreviewReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ClassTransferPreviewReadResult readSnapshot(
        const Application::ClassTransferPreviewRequest& request
        ) const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for class transfer is unavailable.",
                true
                );
        }

        ClassTransferRepository* const repository =
            session->classTransferRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class transfer preview candidates could not be loaded."
                );
        }

        try
        {
            return repository->readSnapshot(request);
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class transfer preview candidates could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class transfer preview candidates could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static Application::ClassTransferPreviewReadResult failure(
        const Domain::ErrorCode code,
        std::string message,
        const bool recoverable = false
        )
    {
        return Application::ClassTransferPreviewReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = recoverable
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
