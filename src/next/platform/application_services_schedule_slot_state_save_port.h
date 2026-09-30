#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "next/application/schedule_slot_state_save.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Adapts the Qt-free slot-state command to the active session repository.
class ApplicationServicesScheduleSlotStateSavePort final
    : public Application::ScheduleSlotStateSavePort
{
public:
    explicit ApplicationServicesScheduleSlotStateSavePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesScheduleSlotStateSavePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesScheduleSlotStateSavePort(
        const ApplicationServicesScheduleSlotStateSavePort&
        ) = delete;
    ApplicationServicesScheduleSlotStateSavePort& operator=(
        const ApplicationServicesScheduleSlotStateSavePort&
        ) = delete;
    ApplicationServicesScheduleSlotStateSavePort(
        ApplicationServicesScheduleSlotStateSavePort&&
        ) = delete;
    ApplicationServicesScheduleSlotStateSavePort& operator=(
        ApplicationServicesScheduleSlotStateSavePort&&
        ) = delete;

    [[nodiscard]] bool isAvailable() const
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        return session
            && session->isOpen()
            && session->intensiveSlotStateRepository();
    }

    [[nodiscard]] Application::ScheduleSlotStateSaveResult saveSlotState(
        const Application::ScheduleSlotStateSaveRequest& request
        ) const override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return Application::ScheduleSlotStateSaveResult::failure(
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

            IntensiveSlotStateRepository* const repository =
                session->intensiveSlotStateRepository();
            if (!repository)
            {
                return unavailableFailure();
            }

            const Status saved = repository->saveIntensiveSlotState(
                legacyWeekday(request.weekday),
                legacyStartTime(request.startMinute),
                legacyState(request.selectedState),
                legacyState(request.defaultState)
                );
            if (!saved)
            {
                if (!session->isOpen())
                {
                    return unavailableFailure();
                }

                const QByteArray errorBytes = saved.error().toUtf8();
                return failure(
                    Domain::ErrorCode::Technical,
                    errorBytes.isEmpty()
                        ? "Schedule slot state could not be saved."
                        : errorBytes.toStdString()
                    );
            }

            return Application::ScheduleSlotStateSaveResult::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Schedule slot state could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Schedule slot state could not be saved."
                );
        }
    }

private:
    [[nodiscard]] static QString legacyWeekday(
        const Application::ScheduleWeekday weekday
        )
    {
        switch (weekday)
        {
        case Application::ScheduleWeekday::Monday:
            return QStringLiteral("Monday");
        case Application::ScheduleWeekday::Tuesday:
            return QStringLiteral("Tuesday");
        case Application::ScheduleWeekday::Wednesday:
            return QStringLiteral("Wednesday");
        case Application::ScheduleWeekday::Thursday:
            return QStringLiteral("Thursday");
        case Application::ScheduleWeekday::Friday:
            return QStringLiteral("Friday");
        case Application::ScheduleWeekday::Saturday:
            return QStringLiteral("Saturday");
        case Application::ScheduleWeekday::Sunday:
            return QStringLiteral("Sunday");
        }

        return {};
    }

    [[nodiscard]] static QString legacyStartTime(
        const int startMinute
        )
    {
        return QStringLiteral("%1:%2")
            .arg(startMinute / 60, 2, 10, QLatin1Char('0'))
            .arg(startMinute % 60, 2, 10, QLatin1Char('0'));
    }

    [[nodiscard]] static QString legacyState(
        const Application::ScheduleSlotState state
        )
    {
        switch (state)
        {
        case Application::ScheduleSlotState::Empty:
            return QStringLiteral("empty");
        case Application::ScheduleSlotState::Essay:
            return QStringLiteral("essay");
        case Application::ScheduleSlotState::Lunch:
            return QStringLiteral("lunch");
        }

        return {};
    }

    [[nodiscard]] static Application::ScheduleSlotStateSaveResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::ScheduleSlotStateSaveResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static Application::ScheduleSlotStateSaveResult
    unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for intensive slot states is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
