#pragma once

#include "core/settingsmanager.h"
#include "next/application/document_page_spacing_preferences.h"

#include <QString>
#include <QVariant>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for document-page spacing reads and writes. The legacy
// SettingsManager default and QVariant integer conversion stay here.
class SettingsManagerDocumentPageSpacingPreferencesPort final
    : public Application::DocumentPageSpacingPreferencesPort
{
public:
    explicit SettingsManagerDocumentPageSpacingPreferencesPort(
        SettingsManager::DocumentPageSpacingPreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

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

    [[nodiscard]] Domain::Result<void> write(
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
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message =
                    "Document Page Spacing must be None, Small, Medium, or Large.",
                .recoverable = false
            });
        }

        const QSettings::Status status =
            SettingsManager::instance().writeDocumentPageSpacingPreference(
                storedSpacing,
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
                    "Unable to save Document Page Spacing preference: QSettings "
                    ) + statusName + ".",
                .recoverable = true
            });
        }

        return Domain::Result<void>::success();
    }

private:
    SettingsManager::DocumentPageSpacingPreferenceSync m_synchronize;

    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            SettingsManager::Keys::DOCUMENT_PAGE_SPACING_PREFERENCE
            );
    }
};

} // namespace ClassMngr::Next::Platform
