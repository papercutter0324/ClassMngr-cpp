#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/application/roster_read_query.h"
#include "next/application/roster_row_display_policy.h"
#include "next/application/roster_row_availability.h"

#include <utility>

namespace ClassMngr::Next::Application
{

struct RosterWindowReadQuery final
{
    Domain::ClassId classId;
    int rowLimit = static_cast<int>(RosterModeledRowCount);

    friend bool operator==(
        const RosterWindowReadQuery&,
        const RosterWindowReadQuery&
        ) = default;
};

struct RosterWindowReadSnapshot final
{
    RosterSnapshot roster;
    bool hasRowsOutsideWindow = false;

    friend bool operator==(
        const RosterWindowReadSnapshot&,
        const RosterWindowReadSnapshot&
        ) = default;
};

using RosterWindowReadResult = Domain::Result<RosterWindowReadSnapshot>;

class RosterWindowReadPort
{
public:
    virtual ~RosterWindowReadPort() = default;

    [[nodiscard]] virtual RosterWindowReadResult readRosterWindow(
        const RosterWindowReadQuery& query
        ) const = 0;
};

class RosterWindowReadUseCase final
{
public:
    [[nodiscard]] static RosterWindowReadResult execute(
        const RosterWindowReadQuery& query,
        const RosterWindowReadPort& port
        )
    {
        if (!isCanonicalPositiveDecimalId(query.classId.value()))
        {
            return RosterWindowReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        if (!isValidMaximumRosterRows(query.rowLimit))
        {
            return RosterWindowReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Roster row limit must be between 1 and 100.",
                .recoverable = true
            });
        }

        return port.readRosterWindow(query);
    }
};

// Adapts the bounded window for row transfer. The matching window save
// boundary preserves any target rows outside the returned window.
class RosterWindowAsRosterReadPort final : public RosterReadPort
{
public:
    explicit RosterWindowAsRosterReadPort(
        const RosterWindowReadPort& windowPort,
        const int rowLimit = DefaultMaximumRosterRows
        ) noexcept
        : m_windowPort(windowPort)
        , m_rowLimit(isValidMaximumRosterRows(rowLimit)
            ? rowLimit
            : DefaultMaximumRosterRows)
    {
    }

    [[nodiscard]] RosterReadResult readRoster(
        const RosterReadQuery& query
        ) const override
    {
        const RosterWindowReadResult loaded =
            RosterWindowReadUseCase::execute(
                {.classId = query.classId, .rowLimit = m_rowLimit},
                m_windowPort
                );
        if (!loaded)
        {
            return RosterReadResult::failure(loaded.error());
        }

        return RosterReadResult::success(
            std::move(loaded.value().roster)
            );
    }

private:
    const RosterWindowReadPort& m_windowPort;
    int m_rowLimit = DefaultMaximumRosterRows;
};

} // namespace ClassMngr::Next::Application
