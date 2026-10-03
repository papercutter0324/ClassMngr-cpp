#pragma once

#include "next/application/roster_print_class_info_read_port.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Application
{

class RosterPrintClassInfoReadQuery final
{
public:
    explicit RosterPrintClassInfoReadQuery(
        const RosterPrintClassInfoReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] RosterPrintClassInfoReadResult execute(
        const Domain::ClassId& classId
        ) const
    {
        if (!isCanonicalPositiveInteger(classId.value()))
        {
            return RosterPrintClassInfoReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Roster print class ID must be a canonical positive integer.",
                .recoverable = false
            });
        }

        auto source = m_port.readRosterPrintClassInfo(classId);
        if (!source)
        {
            return RosterPrintClassInfoReadResult::failure(source.error());
        }

        auto snapshot = std::move(source.value());
        if (snapshot.classId != classId)
        {
            return RosterPrintClassInfoReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "A roster print class information read returned a different class identifier.",
                .recoverable = false
            });
        }

        return RosterPrintClassInfoReadResult::success(std::move(snapshot));
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

    const RosterPrintClassInfoReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
