#pragma once

#include <string>

namespace ClassMngr::Next::Application
{

// Platform-provided user locations used to seed feature output folders. Paths
// cross the application boundary as UTF-8; an unavailable location is empty.
struct DefaultOutputDirectories final
{
    std::string documents;
    std::string home;
};

class DefaultOutputDirectoriesPort
{
public:
    virtual ~DefaultOutputDirectoriesPort() = default;

    [[nodiscard]] virtual DefaultOutputDirectories read() const = 0;
};

} // namespace ClassMngr::Next::Application
