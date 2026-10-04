#pragma once

#include "next/application/roster_template_print_source_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace ClassMngr::Next::Application
{

class RosterTemplatePrintSourceReadQuery final
{
public:
    explicit RosterTemplatePrintSourceReadQuery(
        const RosterTemplatePrintSourceReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] RosterTemplatePrintSourceReadResult execute(
        const std::vector<Domain::ClassId>& classIds
        ) const
    {
        if (classIds.empty())
        {
            return RosterTemplatePrintSourceReadResult::success({});
        }

        std::unordered_set<std::string> seenIds;
        seenIds.reserve(classIds.size());
        for (const Domain::ClassId& classId : classIds)
        {
            if (!isCanonicalPositiveId(classId.value())
                || !seenIds.insert(classId.value()).second)
            {
                return RosterTemplatePrintSourceReadResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "Roster print class IDs must be canonical, positive, and unique.",
                    .recoverable = false
                });
            }
        }

        auto source = m_port.readRosterTemplatePrintSource({classIds});
        if (!source)
        {
            return RosterTemplatePrintSourceReadResult::failure(
                source.error()
                );
        }

        auto entries = std::move(source.value());
        if (entries.size() != classIds.size())
        {
            return RosterTemplatePrintSourceReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "Roster print batch returned a different number of classes.",
                .recoverable = false
            });
        }

        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            if (entries[index].classId != classIds[index]
                || entries[index].classInfo.classId != classIds[index])
            {
                return RosterTemplatePrintSourceReadResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Roster print batch returned a different class identity or order.",
                    .recoverable = false
                });
            }
        }

        return RosterTemplatePrintSourceReadResult::success(
            std::move(entries)
            );
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

    const RosterTemplatePrintSourceReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
