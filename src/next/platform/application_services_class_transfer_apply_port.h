#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_transfer_repository.h"
#include "domain/validation/class_transfer_package_validator.h"
#include "next/application/class_transfer_apply.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassTransferApplyPort final
    : public Application::ClassTransferApplyPort
{
public:
    explicit ApplicationServicesClassTransferApplyPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesClassTransferApplyPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ClassTransferApplyResult apply(
        const Application::ClassTransferApplyCommand& command
        ) const override
    {
        try
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
                    "Classes could not be imported."
                    );
            }

            const auto normalized =
                ClassTransferPackageValidator::normalizedAndValidated(
                    command.package
                    );
            if (!normalized)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    toStdString(normalized.error()),
                    true
                    );
            }

            const auto imported = repository->importClasses(
                *normalized,
                command.choices
                );
            if (!imported)
            {
                const QByteArray message = imported.error().toUtf8();
                return failure(
                    Domain::ErrorCode::Technical,
                    std::string(message.constData(),
                        static_cast<std::size_t>(message.size())),
                    true
                    );
            }

            Application::ClassTransferApplySummary summary;
            summary.skippedClassCount = imported->skippedClassCount;
            for (const int id : imported->createdClassIds)
            {
                auto typedId = Domain::ClassId::fromString(std::to_string(id));
                if (id <= 0 || !typedId)
                {
                    return failure(
                        Domain::ErrorCode::Technical,
                        "The import repository returned an invalid class ID."
                        );
                }
                summary.createdClassIds.push_back(std::move(*typedId));
            }
            for (const int id : imported->replacedClassIds)
            {
                auto typedId = Domain::ClassId::fromString(std::to_string(id));
                if (id <= 0 || !typedId)
                {
                    return failure(
                        Domain::ErrorCode::Technical,
                        "The import repository returned an invalid class ID."
                        );
                }
                summary.replacedClassIds.push_back(std::move(*typedId));
            }
            return Application::ClassTransferApplyResult::success(
                std::move(summary)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Classes could not be imported."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Classes could not be imported."
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

    [[nodiscard]] static Application::ClassTransferApplyResult failure(
        const Domain::ErrorCode code,
        std::string message,
        const bool recoverable = false
        )
    {
        if (message.empty())
        {
            message = "Classes could not be imported.";
        }
        return Application::ClassTransferApplyResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = recoverable
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
