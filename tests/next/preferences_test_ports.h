#pragma once

#include "next/application/preferences_model.h"

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
}
