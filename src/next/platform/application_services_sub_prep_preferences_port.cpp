#include "next/platform/application_services_sub_prep_preferences_port.h"

#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"

#include <QDebug>

namespace ClassMngr::Next::Platform
{

Application::SubPrepPreferencesResult
ApplicationServicesSubPrepPreferencesPort::load() const
{
    if (!m_store || !m_store->isOpen())
    {
        return Application::SubPrepPreferencesResult::failure(
            unavailableError()
            );
    }

    SettingsRepository* const repository = m_store->settingsRepository();
    struct LoadedSetting final
    {
        QVariant value;
        std::optional<Domain::OperationError> error;
    };
    const auto loadOrDefault = [repository](
        const QString& key,
        const QVariant& defaultValue
        ) -> LoadedSetting {
        if (!repository)
        {
            qWarning()
                << "Failed to load setting"
                << key
                << ':'
                << "No Teacher Profile service is available.";
            return { defaultValue, std::nullopt };
        }

        const auto stored = repository->loadSetting(key);
        if (!stored)
        {
            qWarning()
                << "Failed to load setting"
                << key
                << ':'
                << stored.error();
            return {
                defaultValue,
                Domain::OperationError{
                    .code = Domain::ErrorCode::Technical,
                    .message = toUtf8(stored.error()),
                    .recoverable = false
                }
            };
        }

        return {
            stored->isValid() ? *stored : defaultValue,
            std::nullopt
        };
    };

    const LoadedSetting classMaterials = loadOrDefault(
        classMaterialsKey(),
        QString()
        );
    if (classMaterials.error)
    {
        return Application::SubPrepPreferencesResult::failure(
            *classMaterials.error
            );
    }

    const LoadedSetting bookReportGrading = loadOrDefault(
        bookReportGradingKey(),
        QVariant()
        );
    if (bookReportGrading.error)
    {
        return Application::SubPrepPreferencesResult::failure(
            *bookReportGrading.error
            );
    }

    const LoadedSetting bookReportSpecialInstructions = loadOrDefault(
        bookReportSpecialInstructionsKey(),
        QVariant()
        );
    if (bookReportSpecialInstructions.error)
    {
        return Application::SubPrepPreferencesResult::failure(
            *bookReportSpecialInstructions.error
            );
    }

    const LoadedSetting subComments = loadOrDefault(
        subCommentsKey(),
        QString()
        );
    if (subComments.error)
    {
        return Application::SubPrepPreferencesResult::failure(
            *subComments.error
            );
    }

    return Application::SubPrepPreferencesResult::success({
        .classMaterials = toUtf8(
            classMaterials.value.toString()
            ),
        .bookReportGrading = optionalUtf8(
            bookReportGrading.value
            ),
        .bookReportSpecialInstructions = optionalUtf8(
            bookReportSpecialInstructions.value
            ),
        .subComments = toUtf8(
            subComments.value.toString()
            )
    });
}

Application::SubPrepPreferencesSaveResult
ApplicationServicesSubPrepPreferencesPort::save(
    const Application::SubPrepPreferences& preferences
    ) const
{
    if (!m_store || !m_store->isOpen())
    {
        return Application::SubPrepPreferencesSaveResult::failure(
            unavailableError()
            );
    }

    SettingsRepository* const repository = m_store->settingsRepository();
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
