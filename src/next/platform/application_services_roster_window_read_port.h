#pragma once

#include "next/platform/repository_error_code.h"

#include "core/application_services.h"
#include "data/repositories/roster_repository.h"
#include "data/database/workspace_store.h"
#include "next/application/roster_window_read_query.h"

#include <QByteArray>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesRosterWindowReadPort final
    : public Application::RosterWindowReadPort
{
public:
    explicit ApplicationServicesRosterWindowReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesRosterWindowReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::RosterWindowReadResult readRosterWindow(
        const Application::RosterWindowReadQuery& query
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

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The roster service is unavailable."
                );
        }

        RosterRepository* const repository = store->rosterRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster repository is unavailable."
                );
        }

        try
        {
            const RepositoryResult<RosterWindowReadOutcome> loaded =
                repository->loadRosterWindowDetailed(
                    *classId,
                    static_cast<int>(Application::RosterModeledRowCount)
                    );
            if (!loaded)
            {
                return failure(
                    repositoryDomainErrorCode(loaded.error().code),
                    toStdString(toLegacyDiagnostic(loaded.error()))
                    );
            }

            if (loaded->parentMissing)
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "Roster class was not found."
                    );
            }

            return Application::RosterWindowReadResult::success({
                .roster = applicationSnapshot(loaded->roster),
                .hasRowsOutsideWindow = loaded->hasRowsOutsideWindow
            });
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
        snapshot.columns.reserve(static_cast<std::size_t>(roster.columns.size()));
        for (const QString& column : roster.columns)
        {
            snapshot.columns.push_back(column.toStdU16String());
        }

        snapshot.columnWidths.reserve(
            static_cast<std::size_t>(roster.columnWidths.size())
            );
        for (const int width : roster.columnWidths)
        {
            snapshot.columnWidths.push_back(
                Application::RosterSnapshotColumnWidth{width}
                );
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

    [[nodiscard]] static Application::RosterWindowReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Roster could not be loaded.";
        }

        return Application::RosterWindowReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::InvalidInput
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
