#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_block_repository.h"
#include "next/application/schedule_testing_assignment_read_query.h"

#include <QByteArray>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesScheduleTestingAssignmentReadPort final
    : public Application::ScheduleTestingAssignmentReadPort
{
public:
    explicit ApplicationServicesScheduleTestingAssignmentReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesScheduleTestingAssignmentReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ScheduleTestingAssignmentReadResult
    readTestingAssignments(
        const Application::ScheduleTestingAssignmentReadQuery&
        ) const override
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

        try
        {
            const Result<QList<TestingAssignmentDisplayRecord>> loaded =
                repository->loadTestingAssignmentDisplayRecords();
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            Application::ScheduleTestingAssignmentReadSnapshot snapshot;
            snapshot.rows.reserve(static_cast<std::size_t>(loaded->size()));
            for (const TestingAssignmentDisplayRecord& source : *loaded)
            {
                Application::ScheduleTestingAssignmentReadRow row{
                    .day = source.day.toStdU16String(),
                    .startTime = source.startTime.toStdU16String(),
                    .room = source.room.toStdU16String(),
                    .classId = source.classId
                };

                if (source.classId > 0 && source.hasSpecialClass)
                {
                    row.specialClass =
                        Application::ScheduleTestingAssignmentSpecialClass{
                            .name = source.className.toStdU16String(),
                            .teacherKoreanName =
                                source.teacherKoreanName.toStdU16String(),
                            .teacherEnglishName =
                                source.teacherEnglishName.toStdU16String(),
                            .teacherPreferredName =
                                source.teacherPreferredName.toStdU16String(),
                            .room = source.testingClassRoom.toStdU16String(),
                            .grade = source.grade.toStdU16String(),
                            .level = source.level.toStdU16String(),
                            .classColor = source.classColor.toStdU16String(),
                            .fontColor = source.fontColor.toStdU16String()
                        };
                }

                snapshot.rows.push_back(std::move(row));
            }

            return Application::ScheduleTestingAssignmentReadResult::success(
                std::move(snapshot)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing assignments could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing assignments could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Application::ScheduleTestingAssignmentReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Testing assignments could not be loaded.";
        }

        return Application::ScheduleTestingAssignmentReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Validation
        });
    }

    [[nodiscard]] static Application::ScheduleTestingAssignmentReadResult
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
