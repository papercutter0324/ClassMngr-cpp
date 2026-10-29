#pragma once

#include "next/application/campus_directory_read_port.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace ClassMngr::Next::Application
{

class CampusDirectoryReadQuery final
{
public:
    [[nodiscard]] static CampusDirectoryReadResult execute(
        const CampusDirectoryReadPort& port
        )
    {
        CampusDirectoryReadResult result = port.loadCampuses();
        if (!result)
        {
            return result;
        }

        auto& campuses = result.value();
        if (campuses.size() > 4'096)
        {
            return CampusDirectoryReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "Campus directory exceeds its supported entry limit.",
                .recoverable = false
            });
        }

        for (const CampusDirectoryEntry& campus : campuses)
        {
            const bool blankId = campus.id.value().empty()
                || std::all_of(
                    campus.id.value().cbegin(),
                    campus.id.value().cend(),
                    [](const char character)
                    {
                        return std::isspace(
                            static_cast<unsigned char>(character)
                            ) != 0;
                    }
                    );
            const bool blankName = campus.displayName.empty()
                || std::all_of(
                    campus.displayName.cbegin(),
                    campus.displayName.cend(),
                    [](const char character)
                    {
                        return std::isspace(
                            static_cast<unsigned char>(character)
                            ) != 0;
                    }
                    );
            if (blankId || blankName
                || campus.id.value().size() > 256
                || campus.displayName.size() > 256)
            {
                return CampusDirectoryReadResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Campus directory contains an invalid entry.",
                    .recoverable = false
                });
            }
        }

        std::stable_sort(
            campuses.begin(),
            campuses.end(),
            [](const CampusDirectoryEntry& left,
               const CampusDirectoryEntry& right)
            {
                auto lower = [](const unsigned char character)
                {
                    return static_cast<unsigned char>(std::tolower(character));
                };
                return std::lexicographical_compare(
                    left.displayName.cbegin(),
                    left.displayName.cend(),
                    right.displayName.cbegin(),
                    right.displayName.cend(),
                    [&](const char lhs, const char rhs)
                    {
                        return lower(static_cast<unsigned char>(lhs))
                            < lower(static_cast<unsigned char>(rhs));
                    }
                    );
            }
            );

        std::set<std::string> ids;
        for (const CampusDirectoryEntry& campus : campuses)
        {
            if (!ids.insert(campus.id.value()).second)
            {
                return CampusDirectoryReadResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Campus directory contains duplicate identifiers.",
                    .recoverable = false
                });
            }
        }

        return result;
    }
};

} // namespace ClassMngr::Next::Application
