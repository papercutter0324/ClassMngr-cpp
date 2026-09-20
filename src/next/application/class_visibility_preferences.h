#pragma once

namespace ClassMngr::Next::Application
{

// The application boundary carries only the typed class visibility scope.
// Persistence and legacy value conversion belong to the outer adapter.
enum class ClassVisibilityScope
{
    ActiveSchedule,
    AllClasses
};

class ClassVisibilityPreferencesPort
{
public:
    virtual ~ClassVisibilityPreferencesPort() = default;

    [[nodiscard]] virtual ClassVisibilityScope load() const = 0;

    virtual void save(ClassVisibilityScope scope) const = 0;
};

} // namespace ClassMngr::Next::Application
