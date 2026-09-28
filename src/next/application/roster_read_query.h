#pragma once

#include "next/application/roster_snapshot.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <string>
#include <system_error>

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
        if (!isCanonicalPositiveClassId(query.classId.value()))
        {
            return RosterReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        return port.readRoster(query);
    }

private:
    [[nodiscard]] static bool isCanonicalPositiveClassId(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return false;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        return error == std::errc{}
            && end == value.data() + value.size()
            && parsed > 0
            && std::to_string(parsed) == value;
    }
};

} // namespace ClassMngr::Next::Application
