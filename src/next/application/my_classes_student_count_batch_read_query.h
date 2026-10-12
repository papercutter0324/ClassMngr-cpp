#pragma once

#include "next/application/my_classes_student_count_batch_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class MyClassesStudentCountBatchReadQuery final
{
public:
    explicit MyClassesStudentCountBatchReadQuery(
        const MyClassesStudentCountBatchReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] MyClassesStudentCountBatchReadResult execute(
        const std::vector<Domain::ClassId>& classIds
        ) const
    {
        std::unordered_set<std::string> seenClassIds;
        seenClassIds.reserve(classIds.size());
        for (const Domain::ClassId& classId : classIds)
        {
            if (!isCanonicalPositiveDecimalId(classId.value()))
            {
                return MyClassesStudentCountBatchReadResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "My Classes class IDs must be canonical positive integers.",
                    .recoverable = false
                });
            }

            if (!seenClassIds.insert(classId.value()).second)
            {
                return MyClassesStudentCountBatchReadResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "My Classes class IDs must be unique.",
                    .recoverable = false
                });
            }
        }

        if (classIds.empty())
        {
            return MyClassesStudentCountBatchReadResult::success({});
        }

        auto source = m_port.readMyClassesStudentCounts(classIds);
        if (!source)
        {
            return MyClassesStudentCountBatchReadResult::failure(
                source.error()
                );
        }

        auto entries = std::move(source.value());
        if (entries.size() != classIds.size())
        {
            return validationFailure(
                "A My Classes student-count batch returned a different number of entries."
                );
        }

        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            const MyClassesStudentCountBatchReadEntry& entry = entries[index];
            if (entry.classId != classIds[index])
            {
                return validationFailure(
                    "A My Classes student-count batch returned entries in a different identifier order."
                    );
            }

            if (entry.studentCount && entry.studentCount.value() < 0)
            {
                return validationFailure(
                    "A My Classes student-count batch returned a negative count."
                    );
            }
        }

        return MyClassesStudentCountBatchReadResult::success(
            std::move(entries)
            );
    }

private:
    [[nodiscard]] static MyClassesStudentCountBatchReadResult
    validationFailure(std::string message)
    {
        return MyClassesStudentCountBatchReadResult::failure({
            .code = Domain::ErrorCode::Validation,
            .message = std::move(message),
            .recoverable = false
        });
    }

    const MyClassesStudentCountBatchReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
