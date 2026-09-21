#pragma once

namespace ClassMngr::Next::Application
{

// The application boundary carries the typed save mode. Persistence and
// legacy integer conversion belong to the platform adapter.
enum class SaveMode
{
    Automatic = 0,
    Manual = 1
};

class SaveModePreferencesPort
{
public:
    virtual ~SaveModePreferencesPort() = default;

    [[nodiscard]] virtual SaveMode read() const = 0;
};

} // namespace ClassMngr::Next::Application
