#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_block_repository.h"
#include "next/application/schedule_testing_assignment_save.h"

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

class ApplicationServicesScheduleTestingAssignmentSavePort final
    : public Application::ScheduleTestingAssignmentSavePort
{
public:
    explicit ApplicationServicesScheduleTestingAssignmentSavePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesScheduleTestingAssignmentSavePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesScheduleTestingAssignmentSavePort(
        const ApplicationServicesScheduleTestingAssignmentSavePort&
        ) = delete;
    ApplicationServicesScheduleTestingAssignmentSavePort& operator=(
        const ApplicationServicesScheduleTestingAssignmentSavePort&
        ) = delete;
    ApplicationServicesScheduleTestingAssignmentSavePort(
        ApplicationServicesScheduleTestingAssignmentSavePort&&
        ) = delete;
    ApplicationServicesScheduleTestingAssignmentSavePort& operator=(
        ApplicationServicesScheduleTestingAssignmentSavePort&&
        ) = delete;

    [[nodiscard]] Application::ScheduleTestingAssignmentSaveResult
    saveTestingAssignment(
        const Application::ScheduleTestingAssignmentSaveRequest& request
        ) const override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return Application::ScheduleTestingAssignmentSaveResult::failure(
                validation.error()
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

            TestingBlockRepository* const repository =
                session->testingBlockRepository();
            if (!repository)
            {
                return unavailableFailure();
            }

            const QString day = QString::fromStdU16String(request.day);
            const QString startTime =
                QString::fromStdU16String(request.startTime);
            const QString room = QString::fromStdU16String(request.room);

            const Status saved = [&]() -> Status
            {
                switch (request.mutation)
                {
                case Application::ScheduleTestingAssignmentMutation::
                    RemoveAssignment:
                    return repository->deleteTestingAssignment(
                        day,
                        startTime
                        );

                case Application::ScheduleTestingAssignmentMutation::
                    AssignTestingClass:
                {
                    const std::optional<int> classId =
                        legacyClassId(*request.classId);
                    if (!classId)
                    {
                        return Status(std::unexpected(
                            QStringLiteral(
                                "A valid testing class is required."
                                )
                            ));
                    }

                    return repository->assignTestingClass(
                        day,
                        startTime,
                        *classId,
                        request.replaceExisting
                        );
                }

                case Application::ScheduleTestingAssignmentMutation::
                    SavePlainTesting:
                    return repository->saveTestingBlock(
                        day,
                        startTime,
                        room,
                        request.replaceExisting
                        );
                }

                return Status(std::unexpected(
                    QStringLiteral("Testing assignment action is invalid.")
                    ));
            }();

            if (!saved)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(saved.error())
                    );
            }

            return Application::ScheduleTestingAssignmentSaveResult::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing assignment could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing assignment could not be saved."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyClassId(
        const Domain::ClassId& id
        )
    {
        const std::string& value = id.value();
        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0)
        {
            return std::nullopt;
        }

        return parsed;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Application::ScheduleTestingAssignmentSaveResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Testing assignment could not be saved.";
        }

        return Application::ScheduleTestingAssignmentSaveResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static Application::ScheduleTestingAssignmentSaveResult
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
