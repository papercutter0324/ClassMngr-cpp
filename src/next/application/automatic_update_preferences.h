#pragma once

namespace ClassMngr::Next::Application
{

// The automatic-update preference is kept as a typed value so the application
// boundary exposes neither Qt storage types nor the legacy settings singleton.
struct AutomaticUpdatePreferences final
{
    bool automaticChecksEnabled = true;

    friend bool operator==(
        const AutomaticUpdatePreferences&,
        const AutomaticUpdatePreferences&
        ) = default;
};

// Persistence is deliberately expressed as read/write operations. Concrete
// adapters own the storage mechanism and legacy value conversion.
class AutomaticUpdatePreferencesPort
{
public:
    virtual ~AutomaticUpdatePreferencesPort() = default;

    [[nodiscard]] virtual AutomaticUpdatePreferences read() const = 0;

    virtual void write(
        const AutomaticUpdatePreferences& preferences
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
