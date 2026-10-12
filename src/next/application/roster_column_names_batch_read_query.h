#pragma once

#include "next/application/roster_column_names_batch_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

#include <string>
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
            if (!isCanonicalPositiveDecimalId(classId.value()))
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
