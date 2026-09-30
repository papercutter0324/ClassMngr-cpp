#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/roster_repository.h"
#include "domain/models/roster.h"
#include "next/application/roster_read_query.h"

#include <QByteArray>
#include <QStringList>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesRosterReadPort final
    : public Application::RosterReadPort
{
public:
    explicit ApplicationServicesRosterReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesRosterReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesRosterReadPort(
        const ApplicationServicesRosterReadPort&
        ) = delete;
    ApplicationServicesRosterReadPort& operator=(
        const ApplicationServicesRosterReadPort&
        ) = delete;
    ApplicationServicesRosterReadPort(
        ApplicationServicesRosterReadPort&&
        ) = delete;
    ApplicationServicesRosterReadPort& operator=(
        ApplicationServicesRosterReadPort&&
        ) = delete;

    [[nodiscard]] Application::RosterReadResult readRoster(
        const Application::RosterReadQuery& query
        ) const override
    {
        const std::optional<int> classId = legacyClassId(
            query.classId.value()
            );
        if (!classId)
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
            return unavailableFailure();
        }

        RosterRepository* const repository = session->rosterRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster repository is unavailable."
                );
        }

        try
        {
            const Result<Roster> loaded = repository->loadRoster(*classId);
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            return Application::RosterReadResult::success(
                applicationSnapshot(*loaded)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyClassId(
        const std::string& value
        )
    {
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

    [[nodiscard]] static Application::RosterSnapshot applicationSnapshot(
        const Roster& roster
        )
    {
        Application::RosterSnapshot snapshot;
        snapshot.columns.reserve(
            static_cast<std::size_t>(roster.columns.size())
            );
        for (const QString& column : roster.columns)
        {
            snapshot.columns.push_back(column.toStdU16String());
        }

        snapshot.columnWidths.reserve(
            static_cast<std::size_t>(roster.columnWidths.size())
            );
        for (const int width : roster.columnWidths)
        {
            snapshot.columnWidths.push_back(width);
        }

        snapshot.rows.reserve(static_cast<std::size_t>(roster.rows.size()));
        for (const QStringList& sourceRow : roster.rows)
        {
            std::vector<std::u16string> row;
            row.reserve(static_cast<std::size_t>(sourceRow.size()));
            for (const QString& cell : sourceRow)
            {
                row.push_back(cell.toStdU16String());
            }
            snapshot.rows.push_back(std::move(row));
        }

        return snapshot;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Application::RosterReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Roster could not be loaded.";
        }

        return Application::RosterReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::InvalidInput
        });
    }

    [[nodiscard]] static Application::RosterReadResult unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The roster service is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
