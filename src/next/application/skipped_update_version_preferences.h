#pragma once

#include <optional>
#include <string>

namespace ClassMngr::Next::Application
{

using SkippedUpdateVersion = std::optional<std::string>;

// The skipped update version is an optional UTF-8 value. Empty or unavailable
// legacy storage is represented as an empty optional at this boundary.
struct SkippedUpdateVersionPreferences final
{
    SkippedUpdateVersion skippedVersion;

    friend bool operator==(
        const SkippedUpdateVersionPreferences&,
        const SkippedUpdateVersionPreferences&
        ) = default;
};

// Persistence is deliberately expressed as typed read/write/clear
// operations. Concrete adapters own the storage mechanism and legacy value
// conversion.
class SkippedUpdateVersionPreferencesPort
{
public:
    virtual ~SkippedUpdateVersionPreferencesPort() = default;

    [[nodiscard]] virtual SkippedUpdateVersionPreferences read()
        const = 0;

    virtual void write(
        const SkippedUpdateVersionPreferences& preferences
        ) const = 0;

    virtual void clear() const = 0;
};

} // namespace ClassMngr::Next::Application
