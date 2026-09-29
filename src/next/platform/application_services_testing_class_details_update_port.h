#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/testing_class_details_update.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesTestingClassDetailsUpdatePort final
    : public Application::TestingClassDetailsUpdatePort
{
public:
    explicit ApplicationServicesTestingClassDetailsUpdatePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesTestingClassDetailsUpdatePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesTestingClassDetailsUpdatePort(
        const ApplicationServicesTestingClassDetailsUpdatePort&
        ) = delete;
    ApplicationServicesTestingClassDetailsUpdatePort& operator=(
        const ApplicationServicesTestingClassDetailsUpdatePort&
        ) = delete;
    ApplicationServicesTestingClassDetailsUpdatePort(
        ApplicationServicesTestingClassDetailsUpdatePort&&
        ) = delete;
    ApplicationServicesTestingClassDetailsUpdatePort& operator=(
        ApplicationServicesTestingClassDetailsUpdatePort&&
        ) = delete;

    [[nodiscard]] Application::TestingClassDetailsUpdateResult
    updateTestingClassDetails(
        const Application::TestingClassDetailsUpdateRequest& request
        ) const override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return Application::TestingClassDetailsUpdateResult::failure(
                validation.error()
                );
        }

        const std::optional<int> classId = legacyPositiveInteger(
            request.classId.value()
            );
        const std::optional<int> teacherId = request.teacherId
            ? legacyPositiveInteger(request.teacherId->value())
            : std::optional<int>{};
        if (!classId || (request.teacherId && !teacherId))
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Testing class details contain an invalid identifier."
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

            TestingClassRepository* const repository =
                session->testingClassRepository();
            if (!repository)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "Testing class details could not be saved."
                    );
            }

            TestingClass testingClass;
            testingClass.classId = *classId;
            testingClass.name = QString::fromStdU16String(request.name);
            testingClass.grade = QString::fromStdU16String(request.grade);
            testingClass.level = QString::fromStdU16String(request.level);
            testingClass.room = QString::fromStdU16String(request.room);
            testingClass.teacherId = teacherId.value_or(-1);
            testingClass.classColor = QString::fromStdU16String(
                request.classColor
                );
            testingClass.fontColor = QString::fromStdU16String(
                request.fontColor
                );
            testingClass.notes = QString::fromStdU16String(request.notes);

            const Status saved = repository->updateTestingClass(testingClass);
            if (!saved)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(saved.error())
                    );
            }

            return Application::TestingClassDetailsUpdateResult::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing class details could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing class details could not be saved."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyPositiveInteger(
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

    [[nodiscard]] static Application::TestingClassDetailsUpdateResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Testing class details could not be saved.";
        }

        return Application::TestingClassDetailsUpdateResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::InvalidInput
        });
    }

    [[nodiscard]] static Application::TestingClassDetailsUpdateResult
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
