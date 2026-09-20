#pragma once

#include <string>

namespace ClassMngr::Next::Application
{

// The application boundary carries the custom AI website as UTF-8 text.
// Empty or unavailable persistence is represented by an empty string.
class AiCommentCustomWebsitePort
{
public:
    virtual ~AiCommentCustomWebsitePort() = default;

    [[nodiscard]] virtual std::string read() const = 0;

    virtual void write(
        const std::string& websiteUrl
        ) const = 0;

    virtual void clear() const = 0;
};

} // namespace ClassMngr::Next::Application
