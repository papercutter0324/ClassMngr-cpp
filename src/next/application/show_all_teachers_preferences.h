#pragma once

namespace ClassMngr::Next::Application
{

class ShowAllTeachersPreferencesPort
{
public:
    virtual ~ShowAllTeachersPreferencesPort() = default;
    [[nodiscard]] virtual bool read() const = 0;
    // The legacy writer cannot report persistence failures.
    virtual void write(bool enabled) const = 0;
};

}
