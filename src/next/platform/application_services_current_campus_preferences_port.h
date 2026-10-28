#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/settings_repository.h"
#include "next/application/current_campus_preferences.h"

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the current-campus preference. The exact legacy key,
// QVariant/UTF-8 conversion, and repository failure mapping stay here;
// PersonalDetails remains the aggregate compatibility writer.
class ApplicationServicesCurrentCampusPreferencesPort final
    : public Application::CurrentCampusPreferencesPort
{
public:
    explicit ApplicationServicesCurrentCampusPreferencesPort(
        ApplicationServices& services
        ) noexcept
        : m_store(services.workspaceStore())
    {
    }

    explicit ApplicationServicesCurrentCampusPreferencesPort(
        ApplicationServices* services
        ) noexcept
        : m_store(services ? services->workspaceStore() : nullptr)
    {
    }

    explicit ApplicationServicesCurrentCampusPreferencesPort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    ApplicationServicesCurrentCampusPreferencesPort(
        const ApplicationServicesCurrentCampusPreferencesPort&
        ) = delete;
    ApplicationServicesCurrentCampusPreferencesPort& operator=(
        const ApplicationServicesCurrentCampusPreferencesPort&
        ) = delete;
    ApplicationServicesCurrentCampusPreferencesPort(
        ApplicationServicesCurrentCampusPreferencesPort&&
        ) = delete;
    ApplicationServicesCurrentCampusPreferencesPort& operator=(
        ApplicationServicesCurrentCampusPreferencesPort&&
        ) = delete;

    [[nodiscard]] bool isAvailable() const override
    {
        return m_store && m_store->isOpen();
    }

    [[nodiscard]] Domain::Result<Application::CurrentCampusPreferenceValue>
    read() const override
    {
        using Value = Application::CurrentCampusPreferenceValue;

        if (!isAvailable())
        {
            return Domain::Result<Value>::success(Value{});
        }

        SettingsRepository* const repository = m_store->settingsRepository();
        if (!repository)
        {
            return Domain::Result<Value>::failure({
                .code = Domain::ErrorCode::Technical,
                .message =
                    "The settings repository is unavailable while reading "
                    "the current campus preference.",
                .recoverable = true
            });
        }

        const auto stored = repository->loadSetting(key());
        if (!stored)
        {
            qWarning()
                << "Failed to load setting"
                << key()
                << ':'
                << toLegacyDiagnostic(stored.error());

            const QByteArray errorBytes = toLegacyDiagnostic(stored.error()).toUtf8();
            return Domain::Result<Value>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = errorBytes.isEmpty()
                    ? "The current campus preference could not be read."
                    : errorBytes.toStdString(),
                .recoverable = true
            });
        }

        const QVariant campus = stored->isValid()
            ? *stored
            : QVariant(QString());

        const QByteArray storedCampus = campus.toString().toUtf8();

        return Domain::Result<Value>::success(
            Value(std::string(
                storedCampus.constData(),
                static_cast<std::size_t>(storedCampus.size())
                ))
            );
    }

    [[nodiscard]] Domain::Result<void> write(
        const Application::CurrentCampusPreferenceValue& campus
        ) const override
    {
        if (!isAvailable())
        {
            return Domain::Result<void>::success();
        }

        SettingsRepository* const repository = m_store->settingsRepository();
        if (!repository)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Current campus could not be saved.",
                .recoverable = false
            });
        }

        const RepositoryStatus saved = repository->saveSetting(
            key(),
            QString::fromUtf8(
                campus.value().data(),
                static_cast<qsizetype>(campus.value().size())
                )
            );
        if (!saved)
        {
            const QByteArray errorBytes = toLegacyDiagnostic(saved.error()).toUtf8();
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = errorBytes.isEmpty()
                    ? "Current campus could not be saved."
                    : errorBytes.toStdString(),
                .recoverable = false
            });
        }

        return Domain::Result<void>::success();
    }

private:
    [[nodiscard]] static QString key()
    {
        return QStringLiteral("myInfo/campus");
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
