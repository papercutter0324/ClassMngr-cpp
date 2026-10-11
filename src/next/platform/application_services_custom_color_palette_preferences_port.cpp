#include "next/platform/application_services_custom_color_palette_preferences_port.h"

#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"

#include <QByteArray>
#include <QDebug>

#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{
namespace
{
[[nodiscard]] std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {
        bytes.constData(),
        static_cast<std::size_t>(bytes.size())
    };
}

[[nodiscard]] Domain::Result<void> technicalFailure(
    std::string message
    )
{
    return Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Technical,
        .message = std::move(message),
        .recoverable = false
    });
}
}

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

Domain::Result<void>
ApplicationServicesCustomColorPalettePreferencesPort::write(
    const Application::CustomColorPalette& palette
    ) const
{
    if (!m_session || !m_session->isOpen())
    {
        return Domain::Result<void>::success();
    }

    SettingsRepository* const repository = m_session->settingsRepository();
    if (!repository)
    {
        constexpr char diagnostic[] =
            "No settings repository is available to save custom colors.";
        qWarning()
            << "Failed to save custom colors:"
            << diagnostic;
        return technicalFailure(
            "Failed to save custom colors: "
            "No settings repository is available to save custom colors."
            );
    }

    const QStringList colors = toQStringList(palette);
    if (const Status saved = repository->saveSetting(
            key(),
            serializeCustomColors(colors)
            ); !saved)
    {
        qWarning() << "Failed to save custom colors:" << saved.error();
        return technicalFailure(
            "Failed to save custom colors: " + utf8(saved.error())
            );
    }

    return Domain::Result<void>::success();
}

} // namespace ClassMngr::Next::Platform
