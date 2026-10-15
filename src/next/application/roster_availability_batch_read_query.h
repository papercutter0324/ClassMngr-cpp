#pragma once

#include "next/application/roster_availability_batch_read_port.h"
#include "next/application/roster_row_availability.h"
#include "next/application/positive_decimal_id_validation.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class RosterAvailabilityBatchReadQuery final
{
public:
    explicit RosterAvailabilityBatchReadQuery(
        const RosterAvailabilityBatchReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] RosterAvailabilityBatchReadResult execute(
        const std::vector<Domain::ClassId>& classIds,
        const std::vector<std::u16string>& baseColumnNames
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
            return RosterAvailabilityBatchReadResult::success({});
        }

        RosterAvailabilityBatchReadResult source =
            m_port.readRosterAvailability(classIds, baseColumnNames);
        if (!source)
        {
            return RosterAvailabilityBatchReadResult::failure(source.error());
        }

        auto snapshots = std::move(source.value());
        if (snapshots.size() != classIds.size())
        {
            return validationFailure(
                "A roster availability batch returned a different number of records."
                );
        }

        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            if (snapshots[index].classId != classIds[index])
            {
                return validationFailure(
                    "A roster availability batch returned records in a different identifier order."
                    );
            }
            const Domain::Result<int>& firstEmptyRowResult =
                snapshots[index].firstEmptyRowResult;
            if (firstEmptyRowResult
                && (firstEmptyRowResult.value() < -1
                    || firstEmptyRowResult.value()
                        >= static_cast<int>(RosterModeledRowCount)))
            {
                return validationFailure(
                    "A roster availability batch returned an invalid first empty row."
                    );
            }
        }

        return RosterAvailabilityBatchReadResult::success(
            std::move(snapshots)
            );
    }

private:
    [[nodiscard]] static RosterAvailabilityBatchReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return RosterAvailabilityBatchReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static RosterAvailabilityBatchReadResult validationFailure(
        std::string message
        )
    {
        return failure(Domain::ErrorCode::Validation, std::move(message));
    }

    const RosterAvailabilityBatchReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
