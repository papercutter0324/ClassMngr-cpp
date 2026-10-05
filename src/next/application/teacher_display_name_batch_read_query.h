#pragma once

#include "next/application/teacher_display_name_batch_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class TeacherDisplayNameBatchReadQuery final
{
public:
    explicit TeacherDisplayNameBatchReadQuery(
        const TeacherDisplayNameBatchReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] TeacherDisplayNameBatchReadResult execute(
        const std::vector<Domain::TeacherId>& teacherIds
        ) const
    {
        std::unordered_map<std::string, std::size_t> requestedPositions;
        requestedPositions.reserve(teacherIds.size());
        for (std::size_t index = 0; index < teacherIds.size(); ++index)
        {
            const std::string& value = teacherIds[index].value();
            if (!isCanonicalPositiveId(value))
            {
                return invalidInput(
                    "Teacher IDs must be canonical positive integers."
                    );
            }

            if (!requestedPositions.emplace(value, index).second)
            {
                return invalidInput("Teacher IDs must be unique.");
            }
        }

        if (teacherIds.empty())
        {
            return TeacherDisplayNameBatchReadResult::success({});
        }

        TeacherDisplayNameBatchReadResult loaded =
            m_port.readTeacherDisplayNames(teacherIds);
        if (!loaded)
        {
            return TeacherDisplayNameBatchReadResult::failure(loaded.error());
        }

        std::unordered_set<std::string> seenTeacherIds;
        seenTeacherIds.reserve(loaded.value().size());
        std::size_t lastPosition = 0;
        bool hasPreviousPosition = false;
        for (const TeacherDisplayNameBatchReadSnapshot& snapshot :
             loaded.value())
        {
            const std::string& value = snapshot.teacherId.value();
            if (!isCanonicalPositiveId(value))
            {
                return validationFailure(
                    "A teacher display name batch returned a noncanonical teacher identifier."
                    );
            }

            const auto position = requestedPositions.find(value);
            if (position == requestedPositions.end())
            {
                return validationFailure(
                    "A teacher display name batch returned an unexpected teacher identifier."
                    );
            }

            if (!seenTeacherIds.insert(value).second)
            {
                return validationFailure(
                    "A teacher display name batch returned a duplicate teacher identifier."
                    );
            }

            if (hasPreviousPosition && position->second <= lastPosition)
            {
                return validationFailure(
                    "A teacher display name batch returned records in a different identifier order."
                    );
            }
            lastPosition = position->second;
            hasPreviousPosition = true;
        }

        // Teacher repositories use an inner join, so missing rows are valid
        // partial results. Returned rows still retain their requested order.
        return loaded;
    }

private:
    [[nodiscard]] static bool isCanonicalPositiveId(
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

    [[nodiscard]] static TeacherDisplayNameBatchReadResult invalidInput(
        std::string message
        )
    {
        return TeacherDisplayNameBatchReadResult::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static TeacherDisplayNameBatchReadResult validationFailure(
        std::string message
        )
    {
        return TeacherDisplayNameBatchReadResult::failure({
            .code = Domain::ErrorCode::Validation,
            .message = std::move(message),
            .recoverable = false
        });
    }

    const TeacherDisplayNameBatchReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
