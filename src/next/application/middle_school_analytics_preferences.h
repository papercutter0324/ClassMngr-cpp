#pragma once

namespace ClassMngr::Next::Application
{

// The application boundary carries only the typed middle-school analytics
// visibility preference. Persistence and legacy QVariant conversion belong to
// the outer adapter.
class MiddleSchoolAnalyticsPreferencesPort
{
public:
    virtual ~MiddleSchoolAnalyticsPreferencesPort() = default;

    [[nodiscard]] virtual bool load() const = 0;

    virtual void save(bool show) const = 0;
};

} // namespace ClassMngr::Next::Application
