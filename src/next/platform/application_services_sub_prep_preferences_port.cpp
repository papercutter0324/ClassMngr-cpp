#include "next/platform/application_services_sub_prep_preferences_port.h"

#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"

#include <QDebug>

namespace ClassMngr::Next::Platform
{

Application::SubPrepPreferencesResult
ApplicationServicesSubPrepPreferencesPort::load() const
{
    if (!m_session || !m_session->isOpen())
    {
        return Application::SubPrepPreferencesResult::failure(
            unavailableError()
            );
    }

    SettingsRepository* const repository = m_session->settingsRepository();
    const auto loadOrDefault = [repository](
        const QString& key,
        const QVariant& defaultValue
        ) -> QVariant {
        if (!repository)
        {
            qWarning()
                << "Failed to load setting"
                << key
                << ':'
                << "No Teacher Profile service is available.";
            return defaultValue;
        }

        const auto stored = repository->loadSetting(key);
        if (!stored)
        {
            qWarning()
                << "Failed to load setting"
                << key
                << ':'
                << stored.error();
            return defaultValue;
        }

        return stored->isValid() ? *stored : defaultValue;
    };

    return Application::SubPrepPreferencesResult::success({
        .classMaterials = toUtf8(
            loadOrDefault(classMaterialsKey(), QString()).toString()
            ),
        .bookReportGrading = optionalUtf8(
            loadOrDefault(bookReportGradingKey(), QVariant())
            ),
        .bookReportSpecialInstructions = optionalUtf8(
            loadOrDefault(bookReportSpecialInstructionsKey(), QVariant())
            ),
        .subComments = toUtf8(
            loadOrDefault(subCommentsKey(), QString()).toString()
            )
    });
}

Application::SubPrepPreferencesSaveResult
ApplicationServicesSubPrepPreferencesPort::save(
    const Application::SubPrepPreferences& preferences
    ) const
{
    if (!m_session || !m_session->isOpen())
    {
        return Application::SubPrepPreferencesSaveResult::failure(
            unavailableError()
            );
    }

    SettingsRepository* const repository = m_session->settingsRepository();
    if (!repository)
    {
        return Application::SubPrepPreferencesSaveResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "No Teacher Profile service is available.",
            .recoverable = false
        });
    }

    const Status saved = repository->saveSettings({
        {
            classMaterialsKey(),
            fromUtf8(preferences.classMaterials)
        },
        {
            bookReportGradingKey(),
            preferences.bookReportGrading
                ? QVariant(fromUtf8(*preferences.bookReportGrading))
                : QVariant()
        },
        {
            bookReportSpecialInstructionsKey(),
            preferences.bookReportSpecialInstructions
                ? QVariant(fromUtf8(*preferences.bookReportSpecialInstructions))
                : QVariant()
        },
        {
            subCommentsKey(),
            fromUtf8(preferences.subComments)
        }
    });
    if (!saved)
    {
        const QByteArray errorBytes = saved.error().toUtf8();
        return Application::SubPrepPreferencesSaveResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = errorBytes.isEmpty()
                ? "Sub Prep preferences could not be saved."
                : errorBytes.toStdString(),
            .recoverable = false
        });
    }

    return Application::SubPrepPreferencesSaveResult::success();
}

} // namespace ClassMngr::Next::Platform
