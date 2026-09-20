#pragma once

#include "core/settingsmanager.h"
#include "next/application/last_selected_campus_port.h"

#include <QString>
#include <QVariant>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the last-selected campus. The legacy
// SettingsManager singleton and QVariant conversion stay here; callers
// consume only the typed optional campus boundary.
class SettingsManagerLastSelectedCampusPort final
    : public Application::LastSelectedCampusPort
{
public:
    SettingsManagerLastSelectedCampusPort() = default;

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

    void write(
        const std::optional<Domain::CampusId>& campusId
        ) const override
    {
        SettingsManager::instance().set(
            key(),
            campusId.has_value()
                ? QString::fromStdString(campusId->value())
                : QString()
            );
    }

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            SettingsManager::Keys::LAST_CAMPUS_JSON_ID
            );
    }
};

} // namespace ClassMngr::Next::Platform
