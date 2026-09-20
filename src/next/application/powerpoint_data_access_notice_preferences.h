#pragma once

namespace ClassMngr::Next::Application
{

// The PowerPoint data-access notice preference is kept as a typed value so
// the application boundary exposes neither Qt storage types nor the legacy
// settings singleton.
struct PowerPointDataAccessNoticePreferences final
{
    bool showPowerPointDataAccessNotice = true;

    friend bool operator==(
        const PowerPointDataAccessNoticePreferences&,
        const PowerPointDataAccessNoticePreferences&
        ) = default;
};

// Persistence is deliberately expressed as read/write operations. Concrete
// adapters own the storage mechanism and legacy value conversion.
class PowerPointDataAccessNoticePreferencesPort
{
public:
    virtual ~PowerPointDataAccessNoticePreferencesPort() = default;

    [[nodiscard]] virtual PowerPointDataAccessNoticePreferences read()
        const = 0;

    virtual void write(
        const PowerPointDataAccessNoticePreferences& preferences
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
