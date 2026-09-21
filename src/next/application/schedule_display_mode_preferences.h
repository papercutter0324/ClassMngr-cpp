#pragma once

namespace ClassMngr::Next::Application
{

// The application boundary carries only the typed schedule display mode.
// Persistence and legacy string conversion belong to the outer adapter.
enum class ScheduleDisplayMode
{
    Regular,
    Intensive,
    Testing
};

class ScheduleDisplayModePreferencesPort
{
public:
    virtual ~ScheduleDisplayModePreferencesPort() = default;

    [[nodiscard]] virtual ScheduleDisplayMode load() const = 0;

    virtual void save(ScheduleDisplayMode mode) const = 0;
};

} // namespace ClassMngr::Next::Application
