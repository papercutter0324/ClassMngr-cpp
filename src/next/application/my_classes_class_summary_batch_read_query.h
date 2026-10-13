#pragma once

#include "next/application/my_classes_class_summary_batch_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class MyClassesClassSummaryBatchReadQuery final
{
public:
    explicit MyClassesClassSummaryBatchReadQuery(
        const MyClassesClassSummaryBatchReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] MyClassesClassSummaryBatchReadResult execute(
        const std::vector<Domain::ClassId>& classIds
        ) const
    {
        std::unordered_set<std::string> seenClassIds;
        seenClassIds.reserve(classIds.size());
        for (const Domain::ClassId& classId : classIds)
        {
            if (!isCanonicalPositiveDecimalId(classId.value()))
            {
                return MyClassesClassSummaryBatchReadResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "My Classes class IDs must be canonical positive integers.",
                    .recoverable = false
                });
            }

            if (!seenClassIds.insert(classId.value()).second)
            {
                return MyClassesClassSummaryBatchReadResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "My Classes class IDs must be unique.",
                    .recoverable = false
                });
            }
        }

        if (classIds.empty())
        {
            return MyClassesClassSummaryBatchReadResult::success({});
        }

        auto source = m_port.readMyClassesClassSummaries(classIds);
        if (!source)
        {
            return MyClassesClassSummaryBatchReadResult::failure(
                source.error()
                );
        }

        auto entries = std::move(source.value());
        if (entries.size() != classIds.size())
        {
            return validationFailure(
                "A My Classes class-summary batch returned a different number of entries."
                );
        }

        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            const MyClassesClassSummaryBatchReadEntry& entry = entries[index];
            if (entry.classId != classIds[index])
            {
                return validationFailure(
                    "A My Classes class-summary batch returned entries in a different identifier order."
                    );
            }

            if (entry.summary
                && entry.summary.value().teacherId
                && !isCanonicalPositiveDecimalId64(
                    entry.summary.value().teacherId->value()
                    ))
            {
                return validationFailure(
                    "A My Classes class-summary batch returned an invalid teacher identifier."
                    );
            }
        }

        return MyClassesClassSummaryBatchReadResult::success(
            std::move(entries)
            );
    }

private:
    [[nodiscard]] static MyClassesClassSummaryBatchReadResult
    validationFailure(std::string message)
    {
        return MyClassesClassSummaryBatchReadResult::failure({
            .code = Domain::ErrorCode::Validation,
            .message = std::move(message),
            .recoverable = false
        });
    }

    const MyClassesClassSummaryBatchReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
