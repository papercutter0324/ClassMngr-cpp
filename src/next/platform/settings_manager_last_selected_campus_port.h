#pragma once

#include "core/settingsmanager.h"
#include "next/application/last_selected_campus_port.h"

#include <QString>
#include <QVariant>

#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the last-selected campus. The legacy
// SettingsManager singleton and QVariant conversion stay here; callers
// consume only the typed optional campus boundary.
class SettingsManagerLastSelectedCampusPort final
    : public Application::LastSelectedCampusPort
{
public:
    explicit SettingsManagerLastSelectedCampusPort(
        SettingsManager::LastCampusJsonIdPreferenceSync synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

    [[nodiscard]] std::optional<Domain::CampusId> read()
        const override
    {
        const QVariant storedValue =
            SettingsManager::instance().get(key());

        if (!storedValue.isValid())
        {
            return std::nullopt;
        }

        const QString storedText = storedValue.toString();
        if (storedText.trimmed().isEmpty())
        {
            return std::nullopt;
        }

        return Domain::CampusId::fromString(
            storedText.toStdString()
            );
    }

    [[nodiscard]] Domain::Result<void> write(
        const std::optional<Domain::CampusId>& campusId
        ) const override
    {
        const QSettings::Status status =
            SettingsManager::instance().writeLastCampusJsonIdPreference(
                campusId.has_value()
                    ? QString::fromStdString(campusId->value())
                    : QString(),
                m_synchronize
                );

        if (status == QSettings::NoError)
        {
            return Domain::Result<void>::success();
        }

        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message =
                "The last-selected campus preference could not be saved.",
            .recoverable = true
        });
    }

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            SettingsManager::Keys::LAST_CAMPUS_JSON_ID
            );
    }

    SettingsManager::LastCampusJsonIdPreferenceSync m_synchronize;
};

} // namespace ClassMngr::Next::Platform
