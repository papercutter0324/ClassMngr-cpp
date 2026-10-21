#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/application/roster_read_query.h"
#include "next/application/roster_row_availability.h"

#include <utility>

namespace ClassMngr::Next::Application
{

struct RosterWindowReadQuery final
{
    Domain::ClassId classId;

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

        return port.readRosterWindow(query);
    }
};

// Adapts the bounded window for row transfer. Transfer refuses an overflowing
// target instead of passing a truncated projection to the replacement save.
class RosterWindowAsRosterReadPort final : public RosterReadPort
{
public:
    explicit RosterWindowAsRosterReadPort(
        const RosterWindowReadPort& windowPort
        ) noexcept
        : m_windowPort(windowPort)
    {
    }

    [[nodiscard]] RosterReadResult readRoster(
        const RosterReadQuery& query
        ) const override
    {
        const RosterWindowReadResult loaded =
            RosterWindowReadUseCase::execute(
                {.classId = query.classId},
                m_windowPort
                );
        if (!loaded)
        {
            return RosterReadResult::failure(loaded.error());
        }

        if (loaded.value().hasRowsOutsideWindow)
        {
            return RosterReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "The target roster has saved rows beyond row 25; transfer was blocked to preserve them.",
                .recoverable = false
            });
        }

        return RosterReadResult::success(
            std::move(loaded.value().roster)
            );
    }

private:
    const RosterWindowReadPort& m_windowPort;
};

} // namespace ClassMngr::Next::Application
