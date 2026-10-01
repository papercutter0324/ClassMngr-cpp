#include "next/platform/application_services_custom_color_palette_preferences_port.h"

#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"

#include <QDebug>

namespace ClassMngr::Next::Platform
{

Application::CustomColorPalette
ApplicationServicesCustomColorPalettePreferencesPort::read() const
{
    if (!m_session || !m_session->isOpen())
    {
        return Application::defaultCustomColorPalette();
    }

    SettingsRepository* const repository = m_session->settingsRepository();
    if (!repository)
    {
        qWarning()
            << "Failed to load setting"
            << key()
            << ':'
            << "No Teacher Profile service is available.";
        return Application::defaultCustomColorPalette();
    }

    const auto stored = repository->loadSetting(key());
    QVariant storedValue = QString();
    if (!stored)
    {
        qWarning()
            << "Failed to load setting"
            << key()
            << ':'
            << stored.error();
    }
    else if (stored->isValid())
    {
        storedValue = *stored;
    }

    return normalizeStoredValue(storedValue);
}

void ApplicationServicesCustomColorPalettePreferencesPort::write(
    const Application::CustomColorPalette& palette
    ) const
{
    if (!m_session || !m_session->isOpen())
    {
        return;
    }

    SettingsRepository* const repository = m_session->settingsRepository();
    if (!repository)
    {
        qWarning()
            << "Failed to save custom colors:"
            << "No Teacher Profile service is available.";
        return;
    }

    const QStringList colors = toQStringList(palette);
    if (const Status saved = repository->saveSetting(
            key(),
            serializeCustomColors(colors)
            ); !saved)
    {
        qWarning() << "Failed to save custom colors:" << saved.error();
    }
}

} // namespace ClassMngr::Next::Platform
