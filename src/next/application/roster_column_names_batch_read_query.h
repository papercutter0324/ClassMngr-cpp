#pragma once

#include "next/application/roster_column_names_batch_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class RosterColumnNamesBatchReadQuery final
{
public:
    explicit RosterColumnNamesBatchReadQuery(
        const RosterColumnNamesBatchReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] RosterColumnNamesBatchReadResult execute(
        const std::vector<Domain::ClassId>& classIds
        ) const
    {
        std::unordered_set<std::string> seenClassIds;
        seenClassIds.reserve(classIds.size());
        for (const Domain::ClassId& classId : classIds)
        {
            if (!isCanonicalPositiveClassId(classId.value()))
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Class IDs must be canonical positive integers."
                    );
            }

            if (!seenClassIds.insert(classId.value()).second)
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Class IDs must be unique."
                    );
            }
        }

        if (classIds.empty())
        {
            return RosterColumnNamesBatchReadResult::success({});
        }

        RosterColumnNamesBatchReadResult source =
            m_port.readRosterColumnNames(classIds);
        if (!source)
        {
            return RosterColumnNamesBatchReadResult::failure(source.error());
        }

        auto snapshots = std::move(source.value());
        if (snapshots.size() != classIds.size())
        {
            return validationFailure(
                "A roster column names batch returned a different number of records."
                );
        }

        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            if (snapshots[index].classId != classIds[index])
            {
                return validationFailure(
                    "A roster column names batch returned records in a different identifier order."
                    );
            }
        }

        return RosterColumnNamesBatchReadResult::success(
            std::move(snapshots)
            );
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

    [[nodiscard]] static RosterColumnNamesBatchReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return RosterColumnNamesBatchReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static RosterColumnNamesBatchReadResult validationFailure(
        std::string message
        )
    {
        return failure(Domain::ErrorCode::Validation, std::move(message));
    }

    const RosterColumnNamesBatchReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
