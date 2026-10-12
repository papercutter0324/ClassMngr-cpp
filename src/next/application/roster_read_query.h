#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/application/roster_snapshot.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

struct RosterReadQuery final
{
    Domain::ClassId classId;

    friend bool operator==(
        const RosterReadQuery&,
        const RosterReadQuery&
        ) = default;
};

using RosterReadResult = Domain::Result<RosterSnapshot>;

class RosterReadPort
{
public:
    virtual ~RosterReadPort() = default;

    [[nodiscard]] virtual RosterReadResult readRoster(
        const RosterReadQuery& query
        ) const = 0;
};

class RosterReadUseCase final
{
public:
    [[nodiscard]] static RosterReadResult execute(
        const RosterReadQuery& query,
        const RosterReadPort& port
        )
    {
        if (!isCanonicalPositiveDecimalId(query.classId.value()))
        {
            return RosterReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        return port.readRoster(query);
    }
};

} // namespace ClassMngr::Next::Application
