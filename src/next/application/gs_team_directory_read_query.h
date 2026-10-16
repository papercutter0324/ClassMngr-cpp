#pragma once

#include "next/application/gs_team_directory_read_port.h"

#include <string>
#include <unordered_set>
#include <utility>

namespace ClassMngr::Next::Application
{

class GsTeamDirectoryReadQuery final
{
public:
    explicit GsTeamDirectoryReadQuery(
        const GsTeamDirectoryReadPort& port
        ) noexcept
        : m_port(port)
    {
    }

    [[nodiscard]] GsTeamDirectoryReadResult execute() const
    {
        GsTeamDirectoryReadResult source = m_port.readGsTeamDirectory();
        if (!source)
        {
            return GsTeamDirectoryReadResult::failure(source.error());
        }

        std::unordered_set<int> seenIds;
        seenIds.reserve(source.value().size());
        for (const GsTeamDirectoryEntry& member : source.value())
        {
            const int id = member.id.value();
            if (id <= 0)
            {
                return validationFailure(
                    "The GS Team directory contains an invalid member identifier."
                    );
            }

            if (!seenIds.insert(id).second)
            {
                return validationFailure(
                    "The GS Team directory contains a duplicate member identifier."
                    );
            }
        }

        return source;
    }

private:
    [[nodiscard]] static GsTeamDirectoryReadResult validationFailure(
        std::string message
        )
    {
        return GsTeamDirectoryReadResult::failure({
            .code = Domain::ErrorCode::Validation,
            .message = std::move(message),
            .recoverable = false
        });
    }

    const GsTeamDirectoryReadPort& m_port;
};

} // namespace ClassMngr::Next::Application
