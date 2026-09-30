#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/schedule_display_mode_preferences.h"

#include <QDebug>
#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the persisted ScheduleWidget display mode. The
// canonical string setting and legacy boolean migration remain here; callers
// consume only the typed application mode.
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

    [[nodiscard]] Application::ScheduleDisplayMode load()
        const override
    {
        DatabaseSession* const session = m_services.databaseSession();
        if (!session || !session->isOpen())
        {
            return Application::ScheduleDisplayMode::Regular;
        }

        SettingsRepository* const repository = session->settingsRepository();
        if (!repository)
        {
            return Application::ScheduleDisplayMode::Regular;
        }

        const auto storedModeResult = repository->loadSetting(canonicalKey());
        const QVariant storedMode = storedModeResult.value_or(QVariant());
        const Application::ScheduleDisplayMode mode = modeFromSetting(
            storedMode,
            settingToBool(
                loadOrDefault(*repository, legacyKey(), false)
                )
            );

        if (!storedMode.isValid())
        {
            saveStoredMode(*repository, mode);
        }

        return mode;
    }

    void save(
        const Application::ScheduleDisplayMode mode
        ) const override
    {
        DatabaseSession* const session = m_services.databaseSession();
        if (!session || !session->isOpen())
        {
            return;
        }

        SettingsRepository* const repository = session->settingsRepository();
        if (!repository)
        {
            return;
        }

        saveStoredMode(*repository, mode);
    }

private:
    [[nodiscard]] static QString canonicalKey()
    {
        return QStringLiteral("schedule_display_mode");
    }

    [[nodiscard]] static QString legacyKey()
    {
        return QStringLiteral("schedule_show_intensive");
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

    [[nodiscard]] static Application::ScheduleDisplayMode modeFromSetting(
        const QVariant& value,
        const bool legacyIntensive
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

        return legacyIntensive
            ? Application::ScheduleDisplayMode::Intensive
            : Application::ScheduleDisplayMode::Regular;
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

    [[nodiscard]] static QVariant loadOrDefault(
        SettingsRepository& repository,
        const QString& key,
        const QVariant& defaultValue
        )
    {
        const auto value = repository.loadSetting(key);
        if (!value)
        {
            qWarning()
                << "Failed to load setting"
                << key
                << ':'
                << value.error();
            return defaultValue;
        }

        return value->isValid() ? *value : defaultValue;
    }

    static void saveStoredMode(
        SettingsRepository& repository,
        const Application::ScheduleDisplayMode mode
        )
    {
        if (const Status saved = repository.saveSetting(
                canonicalKey(),
                storedModeValue(mode)
                );
            !saved)
        {
            qWarning()
                << "Failed to save schedule display mode:"
                << saved.error();
        }
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
