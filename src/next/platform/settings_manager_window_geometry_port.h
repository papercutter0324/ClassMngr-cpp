#pragma once

#include "core/settingsmanager.h"
#include "next/application/window_geometry_port.h"

#include <QByteArray>

#include <cstddef>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the Next main-window geometry setting.
class SettingsManagerWindowGeometryPort final
    : public Application::WindowGeometryPort
{
public:
    [[nodiscard]] std::string read() const override
    {
        const QByteArray geometry =
            SettingsManager::instance().getWindowGeometry();
        if (geometry.isEmpty())
        {
            return {};
        }
        return std::string(
            geometry.constData(),
            static_cast<std::size_t>(geometry.size())
            );
    }

    void write(const std::string_view geometryPayload) const override
    {
        if (geometryPayload.empty())
        {
            SettingsManager::instance().setWindowGeometry(QByteArray{});
            return;
        }
        SettingsManager::instance().setWindowGeometry(QByteArray(
            geometryPayload.data(),
            static_cast<qsizetype>(geometryPayload.size())
            ));
    }
};

} // namespace ClassMngr::Next::Platform
