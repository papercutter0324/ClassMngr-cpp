#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/testing_class_create.h"
#include "next/platform/application_services_schedule_testing_assignment_save_port.h"

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
        const Domain::Result<void> validation = request.validate(
            ApplicationServicesScheduleTestingAssignmentSavePort::
                qtCaseInsensitiveWeekdayEquals
            );
        if (!validation)
        {
            return Application::TestingClassCreateResult::failure(
                validation.error()
                );
        }

        const std::optional<qint64> teacherId = request.teacherId
            ? positiveTeacherId(request.teacherId->value())
            : std::optional<qint64>{};
        if (request.teacherId && !teacherId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Testing class details contain an invalid teacher identifier."
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

            const RepositoryResult<int> created = repository->createTestingClass(
                testingClass,
                assignmentDay,
                assignmentStartTime
                );
            if (!created)
            {
                const RepositoryError& error = created.error();
                const bool recoverable =
                    error.recoverableOverride.value_or(
                        error.code != RepositoryErrorCode::Technical
                        );
                return failure(
                    mapRepositoryErrorCode(error.code),
                    toStdString(error.diagnostic),
                    recoverable
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
    [[nodiscard]] static std::optional<qint64> positiveTeacherId(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return std::nullopt;
        }

        qint64 parsed = 0;
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

    [[nodiscard]] static Application::TestingClassCreateResult failure(
        const Domain::ErrorCode code,
        std::string message,
        const std::optional<bool> recoverableOverride = std::nullopt
        )
    {
        if (message.empty())
        {
            message = "Testing class could not be created.";
        }

        return Application::TestingClassCreateResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = recoverableOverride.value_or(
                code != Domain::ErrorCode::Technical
                )
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
