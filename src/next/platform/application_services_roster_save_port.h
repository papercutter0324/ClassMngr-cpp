#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/roster_repository.h"
#include "domain/models/roster.h"
#include "next/application/roster_save_use_case.h"

#include <QByteArray>
#include <QStringList>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] inline bool rosterSaveCaseInsensitiveEquals(
    const std::u16string_view left, const std::u16string_view right)
{
    return QStringView(left.data(), static_cast<qsizetype>(left.size())).compare(
        QStringView(right.data(), static_cast<qsizetype>(right.size())),
        Qt::CaseInsensitive) == 0;
}

class ApplicationServicesRosterSavePort final
    : public Application::RosterSavePort
{
public:
    explicit ApplicationServicesRosterSavePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesRosterSavePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesRosterSavePort(
        const ApplicationServicesRosterSavePort&
        ) = delete;
    ApplicationServicesRosterSavePort& operator=(
        const ApplicationServicesRosterSavePort&
        ) = delete;
    ApplicationServicesRosterSavePort(
        ApplicationServicesRosterSavePort&&
        ) = delete;
    ApplicationServicesRosterSavePort& operator=(
        ApplicationServicesRosterSavePort&&
        ) = delete;

    [[nodiscard]] Domain::Result<void> saveRoster(
        const Application::PreparedRosterSaveRequest& request
        ) const override
    {
        const std::optional<int> classId = legacyClassId(
            request.classId.value()
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
            const Status saved = repository->saveRoster(
                *classId,
                legacyRoster(request.roster)
                );
            if (!saved)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(saved.error())
                    );
            }

            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster could not be saved."
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

    [[nodiscard]] static QString legacyText(const std::u16string& value)
    {
        QString result;
        result.reserve(static_cast<qsizetype>(value.size()));
        for (const char16_t unit : value)
            result.append(QChar(unit));
        return result;
    }

    [[nodiscard]] static Roster legacyRoster(
        const Application::RosterSnapshot& snapshot
        )
    {
        Roster roster;
        roster.columns.reserve(static_cast<qsizetype>(snapshot.columns.size()));
        for (const std::u16string& column : snapshot.columns)
        {
            roster.columns.append(legacyText(column));
        }

        roster.columnWidths.reserve(
            static_cast<qsizetype>(snapshot.columnWidths.size())
            );
        for (const int width : snapshot.columnWidths)
        {
            roster.columnWidths.append(width);
        }

        roster.rows.reserve(static_cast<qsizetype>(snapshot.rows.size()));
        for (const std::vector<std::u16string>& sourceRow : snapshot.rows)
        {
            QStringList row;
            row.reserve(static_cast<qsizetype>(sourceRow.size()));
            for (const std::u16string& cell : sourceRow)
            {
                row.append(legacyText(cell));
            }
            roster.rows.append(std::move(row));
        }

        return roster;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Domain::Result<void> failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Roster could not be saved.";
        }

        return Domain::Result<void>::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static Domain::Result<void> unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The roster service is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
