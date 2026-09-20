#pragma once

#include "core/settingsmanager.h"
#include "next/application/document_viewer_background_preferences.h"
#include "ui/shared/state/option_state_keys.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the document-viewer background read. The legacy
// SettingsManager singleton and integer conversion stay here; ActionRegistry
// remains the compatibility writer and UI owner for this option.
class SettingsManagerDocumentViewerBackgroundPreferencesPort final
    : public Application::DocumentViewerBackgroundPreferencesPort
{
public:
    SettingsManagerDocumentViewerBackgroundPreferencesPort() = default;

    [[nodiscard]] Application::DocumentViewerBackground read()
        const override
    {
        const QVariant storedValue =
            SettingsManager::instance().get(key());
        bool conversionSucceeded = false;
        const int storedBackground =
            storedValue.toInt(&conversionSucceeded);

        if (!conversionSucceeded)
        {
            return Application::DocumentViewerBackground::Default;
        }

        switch (storedBackground)
        {
        case static_cast<int>(
            Application::DocumentViewerBackground::Default
            ):
            return Application::DocumentViewerBackground::Default;
        case static_cast<int>(
            Application::DocumentViewerBackground::White
            ):
            return Application::DocumentViewerBackground::White;
        case static_cast<int>(
            Application::DocumentViewerBackground::Black
            ):
            return Application::DocumentViewerBackground::Black;
        default:
            return Application::DocumentViewerBackground::Default;
        }
    }

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            OptionKeys::DocumentViewerBackground
            );
    }
};

} // namespace ClassMngr::Next::Platform
