#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "next/application/schedule_display_mode_preferences.h"

#include <QDebug>
#include <QString>
#include <QVariant>

#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the persisted ScheduleWidget display mode. The
// canonical string setting and legacy boolean migration remain here; callers
// receive a typed mode result that preserves repository read failures.
class ApplicationServicesScheduleDisplayModePreferencesPort final
    : public Application::ScheduleDisplayModePreferencesPort
{
public:
    explicit ApplicationServicesScheduleDisplayModePreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesScheduleDisplayModePreferencesPort(
        const ApplicationServicesScheduleDisplayModePreferencesPort&
        ) = delete;
    ApplicationServicesScheduleDisplayModePreferencesPort& operator=(
        const ApplicationServicesScheduleDisplayModePreferencesPort&
        ) = delete;
    ApplicationServicesScheduleDisplayModePreferencesPort(
        ApplicationServicesScheduleDisplayModePreferencesPort&&
        ) = delete;
    ApplicationServicesScheduleDisplayModePreferencesPort& operator=(
        ApplicationServicesScheduleDisplayModePreferencesPort&&
        ) = delete;

    [[nodiscard]] Application::ScheduleDisplayModePreferencesLoadResult load()
        const override
    {
        WorkspaceStore* const store = m_services.workspaceStore();
        if (!store || !store->isOpen())
        {
            return Application::
                ScheduleDisplayModePreferencesLoadResult::success(
                    Application::ScheduleDisplayMode::Regular
                    );
        }

        SettingsRepository* const repository = store->settingsRepository();
        if (!repository)
        {
            return Application::
                ScheduleDisplayModePreferencesLoadResult::success(
                    Application::ScheduleDisplayMode::Regular
                    );
        }

        const auto storedModeResult = repository->loadSetting(canonicalKey());
        if (!storedModeResult)
        {
            return loadFailure(
                canonicalKey(),
                storedModeResult.error()
                );
        }

        const QVariant storedMode = *storedModeResult;
        const auto parsedMode = modeFromCanonicalSetting(storedMode);
        if (parsedMode)
        {
            return Application::
                ScheduleDisplayModePreferencesLoadResult::success(*parsedMode);
        }

        const auto legacyModeResult = repository->loadSetting(legacyKey());
        if (!legacyModeResult)
        {
            return loadFailure(
                legacyKey(),
                legacyModeResult.error()
                );
        }

        const bool legacyIntensive = settingToBool(
            legacyModeResult->isValid()
                ? *legacyModeResult
                : QVariant()
            );
        const Application::ScheduleDisplayMode mode = legacyIntensive
            ? Application::ScheduleDisplayMode::Intensive
            : Application::ScheduleDisplayMode::Regular;

        if (!storedMode.isValid())
        {
            const auto saved = saveStoredMode(*repository, mode);
            if (!saved)
            {
                qWarning()
                    << "Failed to save schedule display mode:"
                    << QString::fromStdString(saved.error().message);
            }
        }

        return Application::
            ScheduleDisplayModePreferencesLoadResult::success(mode);
    }

    [[nodiscard]] Application::ScheduleDisplayModePreferencesSaveResult save(
        const Application::ScheduleDisplayMode mode
        ) const override
    {
        if (!isValidMode(mode))
        {
            return Application::
                ScheduleDisplayModePreferencesSaveResult::failure({
                    .code = Domain::ErrorCode::InvalidInput,
                    .message = "Schedule display mode is invalid.",
                    .recoverable = false
                });
        }

        WorkspaceStore* const store = m_services.workspaceStore();
        if (!store || !store->isOpen())
        {
            return failure(
                "Schedule display mode cannot be saved because no database session is open."
                );
        }

        SettingsRepository* const repository = store->settingsRepository();
        if (!repository)
        {
            return failure(
                "Schedule display mode cannot be saved because the settings repository is unavailable."
                );
        }

        return saveStoredMode(*repository, mode);
    }

private:
    [[nodiscard]] static Application::
        ScheduleDisplayModePreferencesLoadResult loadFailure(
            const QString& key,
            const QString& message
            )
    {
        qWarning()
            << "Failed to load setting"
            << key
            << ':'
            << message;
        return Application::
            ScheduleDisplayModePreferencesLoadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = message.isEmpty()
                    ? "Schedule display mode could not be loaded."
                    : message.toStdString(),
                .recoverable = false
            });
    }

    [[nodiscard]] static Application::
        ScheduleDisplayModePreferencesSaveResult failure(
            std::string message
            )
    {
        return Application::ScheduleDisplayModePreferencesSaveResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static QString canonicalKey()
    {
        return QStringLiteral("schedule_display_mode");
    }

    [[nodiscard]] static QString legacyKey()
    {
        return QStringLiteral("schedule_show_intensive");
    }

    [[nodiscard]] static bool isValidMode(
        const Application::ScheduleDisplayMode mode
        )
    {
        switch (mode)
        {
        case Application::ScheduleDisplayMode::Regular:
        case Application::ScheduleDisplayMode::Intensive:
        case Application::ScheduleDisplayMode::Testing:
            return true;

        default:
            return false;
        }
    }

    [[nodiscard]] static QString storedModeValue(
        const Application::ScheduleDisplayMode mode
        )
    {
        switch (mode)
        {
        case Application::ScheduleDisplayMode::Intensive:
            return QStringLiteral("intensive");

        case Application::ScheduleDisplayMode::Testing:
            return QStringLiteral("testing");

        case Application::ScheduleDisplayMode::Regular:
        default:
            return QStringLiteral("regular");
        }
    }

    [[nodiscard]] static std::optional<Application::ScheduleDisplayMode>
        modeFromCanonicalSetting(
        const QVariant& value
        )
    {
        const QString normalized =
            value.toString().trimmed().toLower();

        if (normalized == QStringLiteral("intensive"))
        {
            return Application::ScheduleDisplayMode::Intensive;
        }

        if (normalized == QStringLiteral("testing"))
        {
            return Application::ScheduleDisplayMode::Testing;
        }

        if (normalized == QStringLiteral("regular"))
        {
            return Application::ScheduleDisplayMode::Regular;
        }

        return std::nullopt;
    }

    [[nodiscard]] static bool settingToBool(
        const QVariant& value
        )
    {
        if (!value.isValid())
        {
            return false;
        }

        const QString text =
            value.toString().trimmed().toLower();

        if (text == QStringLiteral("true") || text == QStringLiteral("1"))
        {
            return true;
        }

        if (text == QStringLiteral("false") || text == QStringLiteral("0"))
        {
            return false;
        }

        return value.toBool();
    }

    [[nodiscard]] static Application::
        ScheduleDisplayModePreferencesSaveResult saveStoredMode(
        SettingsRepository& repository,
        const Application::ScheduleDisplayMode mode
        )
    {
        const Status saved = repository.saveSetting(
            canonicalKey(),
            storedModeValue(mode)
            );
        if (!saved)
        {
            const QByteArray errorBytes = saved.error().toUtf8();
            return failure(
                errorBytes.isEmpty()
                    ? "Schedule display mode could not be saved."
                    : errorBytes.toStdString()
                );
        }

        return Application::
            ScheduleDisplayModePreferencesSaveResult::success();
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
