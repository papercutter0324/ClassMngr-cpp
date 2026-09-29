#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/testing_class_create.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesTestingClassCreatePort final
    : public Application::TestingClassCreatePort
{
public:
    explicit ApplicationServicesTestingClassCreatePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesTestingClassCreatePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesTestingClassCreatePort(
        const ApplicationServicesTestingClassCreatePort&
        ) = delete;
    ApplicationServicesTestingClassCreatePort& operator=(
        const ApplicationServicesTestingClassCreatePort&
        ) = delete;
    ApplicationServicesTestingClassCreatePort(
        ApplicationServicesTestingClassCreatePort&&
        ) = delete;
    ApplicationServicesTestingClassCreatePort& operator=(
        ApplicationServicesTestingClassCreatePort&&
        ) = delete;

    [[nodiscard]] Application::TestingClassCreateResult createTestingClass(
        const Application::TestingClassCreateRequest& request
        ) const override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return Application::TestingClassCreateResult::failure(
                validation.error()
                );
        }

        const std::optional<int> teacherId = request.teacherId
            ? legacyPositiveInteger(request.teacherId->value())
            : std::optional<int>{};
        if (request.teacherId && !teacherId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Testing class details contain an invalid teacher identifier."
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
                    "Testing class could not be created."
                    );
            }

            TestingClass testingClass;
            testingClass.name = legacyText(request.name);
            testingClass.grade = legacyText(request.grade);
            testingClass.level = legacyText(request.level);
            testingClass.room = legacyText(request.room);
            testingClass.teacherId = teacherId.value_or(-1);
            testingClass.classColor = legacyText(request.classColor);
            testingClass.fontColor = legacyText(request.fontColor);
            testingClass.notes = legacyText(request.notes);

            const QString assignmentDay = request.assignmentDay
                ? legacyText(*request.assignmentDay)
                : QString{};
            const QString assignmentStartTime = request.assignmentStartTime
                ? legacyText(*request.assignmentStartTime)
                : QString{};

            const Result<int> created = repository->createTestingClass(
                testingClass,
                assignmentDay,
                assignmentStartTime
                );
            if (!created)
            {
                const QString& message = created.error();
                const Domain::ErrorCode errorCode =
                    repositoryValidationError(message)
                        ? Domain::ErrorCode::Validation
                        : repositoryConflictError(message)
                            ? Domain::ErrorCode::Conflict
                            : Domain::ErrorCode::Technical;
                return failure(
                    errorCode,
                    toStdString(message)
                    );
            }

            if (*created <= 0)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "Creating the testing class did not return a valid ID."
                    );
            }

            const auto classId = Domain::ClassId::fromString(
                std::to_string(*created)
                );
            if (!classId)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "Creating the testing class did not return a valid ID."
                    );
            }

            return Application::TestingClassCreateResult::success(*classId);
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing class could not be created."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing class could not be created."
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

    [[nodiscard]] static QString legacyText(const std::u16string& value)
    {
        return QString::fromUtf16(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    [[nodiscard]] static bool repositoryValidationError(
        const QString& message
        )
    {
        return message == QObject::tr("Testing class name is required.")
            || message == QObject::tr("Testing class grade is required.")
            || message == QObject::tr("Testing class level is required.")
            || message == QObject::tr("Testing class room is required.")
            || message == QObject::tr(
                "A testing assignment requires a valid weekday and start time."
                );
    }

    [[nodiscard]] static bool repositoryConflictError(
        const QString& message
        )
    {
        return message.contains(
            QStringLiteral("constraint failed"),
            Qt::CaseInsensitive
            );
    }

    [[nodiscard]] static Application::TestingClassCreateResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Testing class could not be created.";
        }

        return Application::TestingClassCreateResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Technical
        });
    }

    [[nodiscard]] static Application::TestingClassCreateResult
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
