#include "next/platform/application_services_sub_prep_personal_zoom_preferences_port.h"

#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"

#include <QDebug>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

Application::SubPrepPersonalZoomPreferencesResult
ApplicationServicesSubPrepPersonalZoomPreferencesPort::load() const
{
    if (!m_session || !m_session->isOpen())
    {
        return Application::
            SubPrepPersonalZoomPreferencesResult::failure(
                unavailableError()
                );
    }

    SettingsRepository* const repository = m_session->settingsRepository();
    const auto loadOrDefault = [repository](const QString& key) {
        if (!repository)
        {
            qWarning()
                << "Failed to load setting"
                << key
                << ':'
                << "No Teacher Profile service is available.";
            return QVariant();
        }

        const auto stored = repository->loadSetting(key);
        if (!stored)
        {
            qWarning() << "Failed to load setting" << key << ':'
                       << stored.error();
            return QVariant();
        }

        return stored->isValid() ? *stored : QVariant();
    };

    const auto readWithLegacyFallback =
        [&loadOrDefault, repository](
            const QString& primaryKey,
            const QString& legacyKey
            ) {
            const QVariant primaryValue = loadOrDefault(primaryKey);
            if (primaryValue.isValid())
            {
                return primaryValue;
            }

            const QVariant legacyValue = loadOrDefault(legacyKey);
            if (legacyValue.isValid() && repository)
            {
                // Migration is best-effort; legacy data remains the successful
                // read result even if persisting the primary key fails.
                static_cast<void>(repository->saveSetting(
                    primaryKey,
                    legacyValue
                    ));
            }

            return legacyValue;
        };

    const QVariant loginId = readWithLegacyFallback(
        primaryLoginIdKey(),
        legacyLoginIdKey()
        );
    const QVariant password = readWithLegacyFallback(
        primaryPasswordKey(),
        legacyPasswordKey()
        );
    const QVariant unavailable = readWithLegacyFallback(
        primaryUnavailableKey(),
        legacyUnavailableKey()
        );

    return Application::
        SubPrepPersonalZoomPreferencesResult::success({
            .loginId = loginId.isValid()
                ? toUtf8(loginId.toString())
                : std::string("N/A"),
            .password = password.isValid()
                ? toUtf8(password.toString())
                : std::string("N/A"),
            .unavailable = unavailable.isValid()
                ? unavailable.toBool()
                : true
        });
}

} // namespace ClassMngr::Next::Platform
