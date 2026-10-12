#pragma once

#include "core/settingsmanager.h"
#include "next/application/upcoming_birthday_dismissal_port.h"

#include <QDate>
#include <QString>

#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the upcoming-birthday dismissal date. The legacy
// SettingsManager singleton and QDate storage remain confined here.
class SettingsManagerUpcomingBirthdayDismissalPort final
    : public Application::UpcomingBirthdayDismissalPort
{
public:
    explicit SettingsManagerUpcomingBirthdayDismissalPort(
        SettingsManager::UpcomingBirthdayDismissalPreferenceSync
            synchronize = {}
        )
        : m_synchronize(std::move(synchronize))
    {
    }

    [[nodiscard]] Domain::Result<void> write(
        const Application::CalendarEventDate& date
        ) const override
    {
        const QDate legacyDate = toLegacyDate(date.value());
        if (!legacyDate.isValid())
        {
            return Domain::Result<void>::success();
        }

        const QSettings::Status status =
            SettingsManager::instance()
                .writeUpcomingBirthdayDismissalDatePreference(
                    legacyDate,
                    m_synchronize
                    );
        if (status == QSettings::NoError)
        {
            return Domain::Result<void>::success();
        }

        const char* statusName = status == QSettings::AccessError
            ? "AccessError (settings could not be written)"
            : "FormatError (settings contain invalid data)";
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = std::string(
                "The upcoming birthday dismissal date could not be saved: "
                "QSettings "
                ) + statusName + ".",
            .recoverable = true
        });
    }

private:
    [[nodiscard]] static QDate toLegacyDate(
        const std::string& value
        )
    {
        const QString serialized = QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
        const QDate parsed = QDate::fromString(
            serialized,
            Qt::ISODate
            );

        return parsed.isValid()
                && parsed.toString(Qt::ISODate) == serialized
            ? parsed
            : QDate();
    }

    SettingsManager::UpcomingBirthdayDismissalPreferenceSync m_synchronize;
};

} // namespace ClassMngr::Next::Platform
