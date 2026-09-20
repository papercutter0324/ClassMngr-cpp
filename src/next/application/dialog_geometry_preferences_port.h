#pragma once

#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

// The application boundary carries opaque dialog geometry as a binary-safe
// string and receives a stable dialog key. Qt settings details belong to the
// platform adapter.
class DialogGeometryPreferencesPort
{
public:
    virtual ~DialogGeometryPreferencesPort() = default;

    [[nodiscard]] virtual std::string read(
        std::string_view stableDialogKey
        ) const = 0;

    virtual void write(
        std::string_view stableDialogKey,
        std::string_view geometryPayload
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
