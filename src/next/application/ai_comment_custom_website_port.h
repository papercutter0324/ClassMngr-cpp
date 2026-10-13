#pragma once

#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries the custom AI website as UTF-8 text.
// Empty or unavailable persistence is represented by an empty string; save
// reports a typed result for the paired provider and URL preference update.
class AiCommentCustomWebsitePort
{
public:
    virtual ~AiCommentCustomWebsitePort() = default;

    [[nodiscard]] virtual std::string read() const = 0;

    [[nodiscard]] virtual Domain::Result<void> save(
        const std::string& websiteUrl
        ) const = 0;

    virtual void clear() const = 0;
};

} // namespace ClassMngr::Next::Application
