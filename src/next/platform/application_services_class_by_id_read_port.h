#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "next/application/class_by_id_read_port.h"

#include <QByteArray>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassByIdReadPort final
    : public Application::ClassByIdReadPort
{
public:
    explicit ApplicationServicesClassByIdReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesClassByIdReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesClassByIdReadPort(
        const ApplicationServicesClassByIdReadPort&
        ) = delete;
    ApplicationServicesClassByIdReadPort& operator=(
        const ApplicationServicesClassByIdReadPort&
        ) = delete;
    ApplicationServicesClassByIdReadPort(
        ApplicationServicesClassByIdReadPort&&
        ) = delete;
    ApplicationServicesClassByIdReadPort& operator=(
        ApplicationServicesClassByIdReadPort&&
        ) = delete;

    [[nodiscard]] Application::ClassByIdReadResult readClassById(
        const Domain::ClassId& classId
        ) const override
    {
        const std::optional<int> legacyClassId = toLegacyClassId(classId);
        if (!legacyClassId)
        {
            return Application::ClassByIdReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Application::ClassByIdReadResult::failure(
                unavailableError());
        }

        ClassRepository* const repository = session->classRepository();
        if (!repository)
        {
            return Application::ClassByIdReadResult::failure(
                unavailableError());
        }

        try
        {
            const Result<Classroom> loaded =
                repository->getClassById(*legacyClassId);
            if (!loaded)
            {
                return Application::ClassByIdReadResult::failure(
                    repositoryError(loaded.error()));
            }
            if (loaded->id != *legacyClassId)
            {
                return Application::ClassByIdReadResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Class repository returned a different class identifier.",
                    .recoverable = false
                });
            }

            return Application::ClassByIdReadResult::success({
                .classId = classId,
                .className = loaded->name.toStdU16String()
            });
        }
        catch (const std::exception&)
        {
            return Application::ClassByIdReadResult::failure(
                technicalError("The selected class could not be loaded."));
        }
        catch (...)
        {
            return Application::ClassByIdReadResult::failure(
                technicalError("The selected class could not be loaded."));
        }
    }

private:
    [[nodiscard]] static std::optional<int> toLegacyClassId(
        const Domain::ClassId& classId
        )
    {
        const std::string& value = classId.value();
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
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = "The active database session for classes is unavailable.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError repositoryError(
        const QString& message
        )
    {
        const std::string detail = toStdString(message);
        return {
            .code = Domain::ErrorCode::Technical,
            .message = detail.empty()
                ? "The selected class could not be loaded."
                : detail,
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError technicalError(
        std::string message
        )
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        };
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
