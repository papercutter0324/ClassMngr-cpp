#pragma once

#include "next/application/selected_class_subtitle_batch_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class SelectedClassSubtitleBatchReadQuery final
{
public:
    explicit SelectedClassSubtitleBatchReadQuery(
        const SelectedClassSubtitleBatchReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] SelectedClassSubtitleBatchReadResult execute(
        const std::vector<Domain::ClassId>& classIds
        ) const
    {
        std::unordered_set<std::string> seenClassIds;
        seenClassIds.reserve(classIds.size());
        for (const Domain::ClassId& classId : classIds)
        {
            if (!isCanonicalPositiveDecimalId(classId.value()))
            {
                return SelectedClassSubtitleBatchReadResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "Selected class IDs must be canonical positive integers.",
                    .recoverable = false
                });
            }

            if (!seenClassIds.insert(classId.value()).second)
            {
                return SelectedClassSubtitleBatchReadResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "Selected class IDs must be unique.",
                    .recoverable = false
                });
            }
        }

        if (classIds.empty())
        {
            return SelectedClassSubtitleBatchReadResult::success({});
        }

        auto source = m_port.readSelectedClassSubtitles(classIds);
        if (!source)
        {
            return SelectedClassSubtitleBatchReadResult::failure(
                source.error()
                );
        }

        auto snapshots = std::move(source.value());
        if (snapshots.size() != classIds.size())
        {
            return validationFailure(
                "A selected class subtitle batch returned a different number of records."
                );
        }

        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            if (snapshots[index].classId != classIds[index])
            {
                return validationFailure(
                    "A selected class subtitle batch returned records in a different identifier order."
                    );
            }
        }

        return SelectedClassSubtitleBatchReadResult::success(
            std::move(snapshots)
            );
    }

private:
    [[nodiscard]] static SelectedClassSubtitleBatchReadResult
    validationFailure(std::string message)
    {
        return SelectedClassSubtitleBatchReadResult::failure({
            .code = Domain::ErrorCode::Validation,
            .message = std::move(message),
            .recoverable = false
        });
    }

    const SelectedClassSubtitleBatchReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
