#pragma once

#include "core/settingsmanager.h"
#include "next/application/document_viewer_background_preferences.h"

#include <QString>
#include <QVariant>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the document-viewer background read. The legacy
// SettingsManager singleton and integer conversion stay here; ActionRegistry
// remains the compatibility writer and UI owner for this option.
class SettingsManagerDocumentViewerBackgroundPreferencesPort final
    : public Application::DocumentViewerBackgroundPreferencesPort
{
public:
    explicit SettingsManagerDocumentViewerBackgroundPreferencesPort(
        SettingsManager::DocumentViewerBackgroundPreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

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

    [[nodiscard]] Domain::Result<void> write(
        const Application::DocumentViewerBackground background
        ) const override
    {
        int storedBackground = 0;
        switch (background)
        {
        case Application::DocumentViewerBackground::Default:
            storedBackground = 0;
            break;

        case Application::DocumentViewerBackground::White:
            storedBackground = 1;
            break;

        case Application::DocumentViewerBackground::Black:
            storedBackground = 2;
            break;

        default:
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message =
                    "Document Viewer Background must be Default, White, or Black.",
                .recoverable = false
            });
        }

        const QSettings::Status status =
            SettingsManager::instance().
                writeDocumentViewerBackgroundPreference(
                    storedBackground,
                    m_synchronize
                    );
        if (status != QSettings::NoError)
        {
            const char* statusName = status == QSettings::AccessError
                ? "AccessError (settings could not be written)"
                : "FormatError (settings contain invalid data)";
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = std::string(
                    "Unable to save Document Viewer Background preference: "
                    "QSettings "
                    ) + statusName + ".",
                .recoverable = true
            });
        }

        return Domain::Result<void>::success();
    }

private:
    SettingsManager::DocumentViewerBackgroundPreferenceSync m_synchronize;

    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            SettingsManager::Keys::DOCUMENT_VIEWER_BACKGROUND_PREFERENCE
            );
    }
};

} // namespace ClassMngr::Next::Platform
