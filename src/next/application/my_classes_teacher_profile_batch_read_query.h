#pragma once

#include "next/application/my_classes_teacher_profile_batch_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class MyClassesTeacherProfileBatchReadQuery final
{
public:
    explicit MyClassesTeacherProfileBatchReadQuery(
        const MyClassesTeacherProfileBatchReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] MyClassesTeacherProfileBatchReadResult execute(
        const std::vector<Domain::TeacherId>& teacherIds
        ) const
    {
        std::unordered_set<std::string> seenTeacherIds;
        seenTeacherIds.reserve(teacherIds.size());
        for (const Domain::TeacherId& teacherId : teacherIds)
        {
            if (!isCanonicalPositiveInteger(teacherId.value()))
            {
                return MyClassesTeacherProfileBatchReadResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "My Classes teacher IDs must be canonical positive integers.",
                    .recoverable = false
                });
            }

            if (!seenTeacherIds.insert(teacherId.value()).second)
            {
                return MyClassesTeacherProfileBatchReadResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "My Classes teacher IDs must be unique.",
                    .recoverable = false
                });
            }
        }

        if (teacherIds.empty())
        {
            return MyClassesTeacherProfileBatchReadResult::success({});
        }

        auto source = m_port.readMyClassesTeacherProfiles(teacherIds);
        if (!source)
        {
            return MyClassesTeacherProfileBatchReadResult::failure(
                source.error()
                );
        }

        auto entries = std::move(source.value());
        if (entries.size() != teacherIds.size())
        {
            return validationFailure(
                "A My Classes teacher-profile batch returned a different number of entries."
                );
        }

        for (std::size_t index = 0; index < teacherIds.size(); ++index)
        {
            if (entries[index].teacherId != teacherIds[index])
            {
                return validationFailure(
                    "A My Classes teacher-profile batch returned entries in a different identifier order."
                    );
            }
        }

        return MyClassesTeacherProfileBatchReadResult::success(
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

    [[nodiscard]] static MyClassesTeacherProfileBatchReadResult
    validationFailure(std::string message)
    {
        return MyClassesTeacherProfileBatchReadResult::failure({
            .code = Domain::ErrorCode::Validation,
            .message = std::move(message),
            .recoverable = false
        });
    }

    const MyClassesTeacherProfileBatchReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
