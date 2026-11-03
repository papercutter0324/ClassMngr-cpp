#pragma once

#include <string>
#include <string_view>

namespace ClassMngr::Next::Application
{

// The main-window geometry crosses the application boundary as an opaque,
// binary-safe payload. Platform storage details belong to an adapter.
class WindowGeometryPort
{
public:
    virtual ~WindowGeometryPort() = default;

    [[nodiscard]] virtual std::string read() const = 0;
    virtual void write(std::string_view geometryPayload) const = 0;
};

} // namespace ClassMngr::Next::Application
