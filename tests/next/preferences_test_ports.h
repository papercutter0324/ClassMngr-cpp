#pragma once

#include "next/application/preferences_model.h"
#include "next/application/window_geometry_port.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Test
{
class MemoryPreferences final : public Application::PreferencesStateReadPort,
                                public Application::PreferencesSnapshotPersistPort
{
public:
    mutable Application::UserPreferencesState stored;
    Application::PreferencesModel model{*this, *this};

    [[nodiscard]] Application::PreferencesStateReadResult read() const override
    {
        return Application::PreferencesStateReadResult::success(stored);
    }
    [[nodiscard]] Application::PreferencesPersistResult persist(
        const Application::UserPreferencesSnapshot&) const override
    {
        return Application::PreferencesPersistResult::success();
    }
};

class MemoryWindowGeometryPort final : public Application::WindowGeometryPort
{
public:
    mutable std::string storedGeometry;
    mutable int readCount = 0;
    mutable int writeCount = 0;

    [[nodiscard]] std::string read() const override
    {
        ++readCount;
        return storedGeometry;
    }

    void write(const std::string_view geometryPayload) const override
    {
        ++writeCount;
        if (geometryPayload.empty())
        {
            storedGeometry.clear();
            return;
        }
        storedGeometry.assign(geometryPayload.data(), geometryPayload.size());
    }
};

[[nodiscard]] inline std::unique_ptr<MemoryWindowGeometryPort>
makeMemoryWindowGeometryPort(std::string initialGeometry = {})
{
    auto port = std::make_unique<MemoryWindowGeometryPort>();
    port->storedGeometry = std::move(initialGeometry);
    return port;
}
}
