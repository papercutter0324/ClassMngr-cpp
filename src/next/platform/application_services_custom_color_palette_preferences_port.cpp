#include "next/platform/application_services_custom_color_palette_preferences_port.h"

#include "data/database/workspace_store.h"
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

[[nodiscard]] Domain::OperationError technicalError(
    std::string message
    )
{
    return {
        .code = Domain::ErrorCode::Technical,
        .message = std::move(message),
        .recoverable = false
    };
}

[[nodiscard]] Domain::Result<void> technicalFailure(
    std::string message
    )
{
    return Domain::Result<void>::failure(technicalError(std::move(message)));
}
}

Domain::Result<Application::CustomColorPalette>
ApplicationServicesCustomColorPalettePreferencesPort::read() const
{
    if (!m_store || !m_store->isOpen())
    {
        return Domain::Result<Application::CustomColorPalette>::success(
            Application::defaultCustomColorPalette()
            );
    }

    SettingsRepository* const repository = m_store->settingsRepository();
    if (!repository)
    {
        qWarning()
            << "Failed to load setting"
            << key()
            << ':'
            << "No settings repository is available.";
        return Domain::Result<Application::CustomColorPalette>::failure(
            technicalError(
                "Failed to load custom colors: "
                "No settings repository is available."
                )
            );
    }

    const auto stored = repository->loadSetting(key());
    if (!stored)
    {
        qWarning()
            << "Failed to load setting"
            << key()
            << ':'
            << toLegacyDiagnostic(stored.error());
        return Domain::Result<Application::CustomColorPalette>::failure(
            technicalError(
                "Failed to load custom colors: "
                    + utf8(toLegacyDiagnostic(stored.error()))
                )
            );
    }

    if (!stored->isValid())
    {
        return Domain::Result<Application::CustomColorPalette>::success(
            Application::defaultCustomColorPalette()
            );
    }

    return Domain::Result<Application::CustomColorPalette>::success(
        normalizeStoredValue(*stored)
        );
}

Domain::Result<void>
ApplicationServicesCustomColorPalettePreferencesPort::write(
    const Application::CustomColorPalette& palette
    ) const
{
    if (!m_store || !m_store->isOpen())
    {
        return Domain::Result<void>::success();
    }

    SettingsRepository* const repository = m_store->settingsRepository();
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
    if (const RepositoryStatus saved = repository->saveSetting(
            key(),
            serializeCustomColors(colors)
            ); !saved)
    {
        qWarning() << "Failed to save custom colors:"
                   << toLegacyDiagnostic(saved.error());
        return technicalFailure(
            "Failed to save custom colors: "
                + utf8(toLegacyDiagnostic(saved.error()))
            );
    }

    return Domain::Result<void>::success();
}

} // namespace ClassMngr::Next::Platform
