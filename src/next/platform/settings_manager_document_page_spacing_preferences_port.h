#pragma once

#include "core/settingsmanager.h"
#include "next/application/document_page_spacing_preferences.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the document-page spacing read. The legacy
// SettingsManager default and QVariant integer conversion stay here;
// ActionRegistry remains the compatibility bridge and UI owner.
class SettingsManagerDocumentPageSpacingPreferencesPort final
    : public Application::DocumentPageSpacingPreferencesPort
{
public:
    SettingsManagerDocumentPageSpacingPreferencesPort() = default;

    [[nodiscard]] Application::DocumentPageSpacing read()
        const override
    {
        const QVariant storedValue =
            SettingsManager::instance().get(
                key(),
                static_cast<int>(Application::DocumentPageSpacing::Small)
                );
        if (!storedValue.isValid())
        {
            return Application::DocumentPageSpacing::Small;
        }

        // Deliberately keep QVariant's unchecked conversion behavior: valid
        // malformed text becomes zero and therefore maps to None.
        const int storedSpacing = storedValue.toInt();

        switch (storedSpacing)
        {
        case static_cast<int>(Application::DocumentPageSpacing::None):
            return Application::DocumentPageSpacing::None;
        case static_cast<int>(Application::DocumentPageSpacing::Small):
            return Application::DocumentPageSpacing::Small;
        case static_cast<int>(Application::DocumentPageSpacing::Medium):
            return Application::DocumentPageSpacing::Medium;
        case static_cast<int>(Application::DocumentPageSpacing::Large):
            return Application::DocumentPageSpacing::Large;
        default:
            return Application::DocumentPageSpacing::Small;
        }
    }

    void write(
        const Application::DocumentPageSpacing spacing
        ) const override
    {
        int storedSpacing = 0;
        switch (spacing)
        {
        case Application::DocumentPageSpacing::None:
            storedSpacing = 0;
            break;

        case Application::DocumentPageSpacing::Small:
            storedSpacing = 1;
            break;

        case Application::DocumentPageSpacing::Medium:
            storedSpacing = 2;
            break;

        case Application::DocumentPageSpacing::Large:
            storedSpacing = 3;
            break;

        default:
            return;
        }

        SettingsManager::instance().set(
            key(),
            storedSpacing
            );
    }

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::DocumentPageSpacing
            );
    }
};

} // namespace ClassMngr::Next::Platform
