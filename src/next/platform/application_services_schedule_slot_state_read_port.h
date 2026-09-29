#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "next/application/schedule_slot_state_read_query.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesScheduleSlotStateReadPort final
    : public Application::ScheduleSlotStateReadPort
{
public:
    explicit ApplicationServicesScheduleSlotStateReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesScheduleSlotStateReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ScheduleSlotStateReadResult readSlotStates(
        const Application::ScheduleSlotStateReadQuery&
        ) const override
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

        try
        {
            const Result<QList<IntensiveSlotState>> loaded =
                repository->loadIntensiveSlotStates();
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            Application::ScheduleSlotStateReadSnapshot snapshot;
            snapshot.rows.reserve(static_cast<std::size_t>(loaded->size()));
            for (const IntensiveSlotState& state : *loaded)
            {
                snapshot.rows.push_back({
                    .day = state.day.toStdU16String(),
                    .startTime = state.startTime.toStdU16String(),
                    .state = state.state.toStdU16String()
                });
            }

            return Application::ScheduleSlotStateReadResult::success(
                std::move(snapshot)
                );
        }
        catch (const std::exception& exception)
        {
            return failure(
                Domain::ErrorCode::Technical,
                exception.what()
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Intensive slot states could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(bytes.constData(),
                           static_cast<std::size_t>(bytes.size()));
    }

    [[nodiscard]] static Application::ScheduleSlotStateReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Intensive slot states could not be loaded.";
        }

        return Application::ScheduleSlotStateReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::NotFound
        });
    }

    [[nodiscard]] static Application::ScheduleSlotStateReadResult
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
