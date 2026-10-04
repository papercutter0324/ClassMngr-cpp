#pragma once

#include "next/application/my_classes_class_information_batch_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class MyClassesClassInformationBatchReadQuery final
{
public:
    explicit MyClassesClassInformationBatchReadQuery(
        const MyClassesClassInformationBatchReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] MyClassesClassInformationBatchReadResult execute(
        const std::vector<Domain::ClassId>& classIds
        ) const
    {
        std::unordered_set<std::string> seenClassIds;
        seenClassIds.reserve(classIds.size());
        for (const Domain::ClassId& classId : classIds)
        {
            if (!isCanonicalPositiveInteger(classId.value()))
            {
                return MyClassesClassInformationBatchReadResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "My Classes class IDs must be canonical positive integers.",
                    .recoverable = false
                });
            }

            if (!seenClassIds.insert(classId.value()).second)
            {
                return MyClassesClassInformationBatchReadResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "My Classes class IDs must be unique.",
                    .recoverable = false
                });
            }
        }

        if (classIds.empty())
        {
            return MyClassesClassInformationBatchReadResult::success({});
        }

        auto source = m_port.readMyClassesClassInformationBatch(classIds);
        if (!source)
        {
            return MyClassesClassInformationBatchReadResult::failure(
                source.error()
                );
        }

        auto entries = std::move(source.value());
        if (entries.size() != classIds.size())
        {
            return validationFailure(
                "A My Classes class-information batch returned a different number of entries."
                );
        }

        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            const MyClassesClassInformationBatchReadEntry& entry =
                entries[index];
            if (entry.classId != classIds[index])
            {
                return validationFailure(
                    "A My Classes class-information batch returned entries in a different identifier order."
                    );
            }

            if (entry.information
                && entry.information.value().teacherId
                && !isCanonicalPositiveInteger(
                    entry.information.value().teacherId->value()
                    ))
            {
                return validationFailure(
                    "A My Classes class-information batch returned an invalid teacher identifier."
                    );
            }
        }

        return MyClassesClassInformationBatchReadResult::success(
            std::move(entries)
            );
    }

private:
    [[nodiscard]] static bool isCanonicalPositiveInteger(
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

    [[nodiscard]] static MyClassesClassInformationBatchReadResult
    validationFailure(std::string message)
    {
        return MyClassesClassInformationBatchReadResult::failure({
            .code = Domain::ErrorCode::Validation,
            .message = std::move(message),
            .recoverable = false
        });
    }

    const MyClassesClassInformationBatchReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
