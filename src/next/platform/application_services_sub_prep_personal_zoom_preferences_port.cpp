#include "next/platform/application_services_sub_prep_personal_zoom_preferences_port.h"

#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"

#include <QDebug>
#include <QVariant>

#include <utility>

namespace
{
struct ResolvedZoomSetting final
{
    QVariant value;
    bool migrateToPrimary = false;
};

ClassMngr::Next::Domain::OperationError settingReadError(
    const QString& key,
    const QString& diagnostic
    )
{
    qWarning()
        << "Failed to load setting"
        << key
        << ':'
        << diagnostic;

    std::string message = "Personal Zoom preference '";
    message.append(key.toStdString());
    message.append("' could not be loaded");
    if (!diagnostic.isEmpty())
    {
        message.append(": ");
        const QByteArray diagnosticBytes = diagnostic.toUtf8();
        message.append(
            diagnosticBytes.constData(),
            static_cast<std::size_t>(diagnosticBytes.size())
            );
    }
    message.push_back('.');

    return {
        .code = ClassMngr::Next::Domain::ErrorCode::Technical,
        .message = std::move(message),
        .recoverable = false
    };
}
}

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
    if (!repository)
    {
        return Application::SubPrepPersonalZoomPreferencesResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message =
                "The settings repository is unavailable while reading "
                "personal Zoom preferences.",
            .recoverable = false
        });
    }

    const auto readWithLegacyFallback =
        [repository](
            const QString& primaryKey,
            const QString& legacyKey
            ) -> Domain::Result<ResolvedZoomSetting> {
            const auto primary = repository->loadSetting(primaryKey);
            if (!primary)
            {
                return Domain::Result<ResolvedZoomSetting>::failure(
                    settingReadError(primaryKey, primary.error())
                    );
            }

            if (primary->isValid())
            {
                return Domain::Result<ResolvedZoomSetting>::success({
                    .value = *primary,
                    .migrateToPrimary = false
                });
            }

            const auto legacy = repository->loadSetting(legacyKey);
            if (!legacy)
            {
                return Domain::Result<ResolvedZoomSetting>::failure(
                    settingReadError(legacyKey, legacy.error())
                    );
            }

            return Domain::Result<ResolvedZoomSetting>::success({
                .value = legacy->isValid() ? *legacy : QVariant(),
                .migrateToPrimary = legacy->isValid()
            });
        };

    const auto loginId = readWithLegacyFallback(
        primaryLoginIdKey(),
        legacyLoginIdKey()
        );
    if (!loginId)
    {
        return Application::SubPrepPersonalZoomPreferencesResult::failure(
            loginId.error()
            );
    }

    const auto password = readWithLegacyFallback(
        primaryPasswordKey(),
        legacyPasswordKey()
        );
    if (!password)
    {
        return Application::SubPrepPersonalZoomPreferencesResult::failure(
            password.error()
            );
    }

    const auto unavailable = readWithLegacyFallback(
        primaryUnavailableKey(),
        legacyUnavailableKey()
        );
    if (!unavailable)
    {
        return Application::SubPrepPersonalZoomPreferencesResult::failure(
            unavailable.error()
            );
    }

    // Migration is best-effort, but must wait until the whole preference
    // bundle has been read successfully. A failed read above leaves all
    // primary keys untouched.
    const auto migrateIfNeeded =
        [repository](
            const QString& primaryKey,
            const ResolvedZoomSetting& value
            ) {
            if (value.migrateToPrimary)
            {
                static_cast<void>(repository->saveSetting(
                    primaryKey,
                    value.value
                    ));
            }
        };
    migrateIfNeeded(primaryLoginIdKey(), loginId.value());
    migrateIfNeeded(primaryPasswordKey(), password.value());
    migrateIfNeeded(primaryUnavailableKey(), unavailable.value());

    const ResolvedZoomSetting& loginIdValue = loginId.value();
    const ResolvedZoomSetting& passwordValue = password.value();
    const ResolvedZoomSetting& unavailableValue = unavailable.value();

    return Application::
        SubPrepPersonalZoomPreferencesResult::success({
            .loginId = Application::SubPrepPersonalZoomLoginId(
                loginIdValue.value.isValid()
                    ? toUtf8(loginIdValue.value.toString())
                    : std::string("N/A")
                ),
            .password = Application::SubPrepPersonalZoomPassword(
                passwordValue.value.isValid()
                    ? toUtf8(passwordValue.value.toString())
                    : std::string("N/A")
                ),
            .unavailable = unavailableValue.value.isValid()
                ? unavailableValue.value.toBool()
                : true
        });
}

} // namespace ClassMngr::Next::Platform
