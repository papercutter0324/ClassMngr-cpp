#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/roster_row_transfer_use_case.h"

#include <QString>

#include <charconv>
#include <exception>
#include <optional>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesRosterRowTransferSavePort final
    : public Application::RosterRowTransferSavePort
{
public:
    explicit ApplicationServicesRosterRowTransferSavePort(
        ApplicationServices* services) noexcept : m_services(services) {}

    [[nodiscard]] Domain::Result<void> saveTransfer(
        const Application::RosterRowTransferSaveRequest& request
        ) const override
    {
        const auto sourceId = legacyId(request.sourceClassId);
        const auto targetId = legacyId(request.targetClassId);
        if (!sourceId || !targetId || *sourceId == *targetId)
            return failure(Domain::ErrorCode::InvalidInput,
                "Transfer requires distinct canonical positive class IDs.");
        if (!m_services || !m_services->hasOpenDatabase()
            || !m_services->rosterService())
            return failure(Domain::ErrorCode::NotFound,
                "The roster service is unavailable.");
        try
        {
            const Status saved = m_services->rosterService()->saveRosters({
                {*sourceId, legacyRoster(request.sourceRoster)},
                {*targetId, legacyRoster(request.targetRoster)}
            });
            if (!saved)
                return failure(Domain::ErrorCode::Technical,
                    saved.error().toUtf8().toStdString());
            return Domain::Result<void>::success();
        }
        catch (...)
        {
            return failure(Domain::ErrorCode::Technical,
                "Roster transfer could not be saved.");
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyId(const Domain::ClassId& id)
    {
        const auto& value = id.value();
        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(), value.data() + value.size(), parsed);
        if (error != std::errc{} || end != value.data() + value.size()
            || parsed <= 0 || std::to_string(parsed) != value)
            return std::nullopt;
        return parsed;
    }

    [[nodiscard]] static QString text(const std::u16string& value)
    {
        // Snapshots here contain logical text, so copy units without BOM decoding.
        QString result;
        result.reserve(static_cast<qsizetype>(value.size()));
        for (char16_t unit : value)
            result.append(QChar(unit));
        return result;
    }

    [[nodiscard]] static Roster legacyRoster(const Application::RosterSnapshot& snapshot)
    {
        Roster roster;
        for (const auto& column : snapshot.columns)
            roster.columns.append(text(column));
        for (int width : snapshot.columnWidths)
            roster.columnWidths.append(width);
        for (const auto& sourceRow : snapshot.rows)
        {
            QStringList row;
            for (const auto& cell : sourceRow)
                row.append(text(cell));
            roster.rows.append(std::move(row));
        }
        return roster;
    }

    [[nodiscard]] static Domain::Result<void> failure(
        Domain::ErrorCode code, std::string message)
    {
        return Domain::Result<void>::failure({
            .code = code, .message = std::move(message), .recoverable = false});
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
