#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/class_details_validation_context_query.h"

#include <QByteArray>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassDetailsValidationContextPort final
    : public Application::ClassDetailsValidationContextPort
{
public:
    explicit ApplicationServicesClassDetailsValidationContextPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ClassDetailsValidationContextPortResult
    loadClassDetailsValidationContext(
        const Domain::ClassId& classId
        ) const override
    {
        const std::optional<int> legacyClassId = legacyId(classId.value());
        if (!legacyClassId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."
                );
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for class validation is unavailable."
                );
        }

        ClassInfoRepository* const repository =
            session->classInfoRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class validation context could not be loaded."
                );
        }

        try
        {
            // Keep this read on the active session repository. A successful
            // missing-row read retains ClassInfoRepository's default values.
            const Result<ClassInfo> loaded =
                repository->loadClassInfo(*legacyClassId);
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            const std::optional<Domain::ClassId> matchedClassId =
                Domain::ClassId::fromString(
                    std::to_string(loaded->classId)
                    );
            if (!matchedClassId)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "The class validation context has an invalid class identifier."
                    );
            }

            return Application::ClassDetailsValidationContextPortResult::success({
                .matchedClassId = *matchedClassId,
                .teacherId = loaded->teacherId,
                .notes = loaded->notes.toStdU16String(),
                .timeFillerActivities =
                    loaded->timeFillerActivities.toStdU16String()
            });
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class validation context could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class validation context could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyId(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return std::nullopt;
        }

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
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    [[nodiscard]] static Application::ClassDetailsValidationContextPortResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Class validation context could not be loaded.";
        }

        return Application::ClassDetailsValidationContextPortResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Validation
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
