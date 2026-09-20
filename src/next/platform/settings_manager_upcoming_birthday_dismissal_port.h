#pragma once

#include "core/settingsmanager.h"
#include "next/application/upcoming_birthday_dismissal_port.h"

#include <QDate>
#include <QString>

#include <string>

namespace ClassMngr::Next::Platform
{

// Qt-boundary adapter for the upcoming-birthday dismissal date. The legacy
// SettingsManager singleton and QDate storage remain confined here.
class SettingsManagerUpcomingBirthdayDismissalPort final
    : public Application::UpcomingBirthdayDismissalPort
{
public:
    SettingsManagerUpcomingBirthdayDismissalPort() = default;

    void write(
        const Application::CalendarEventDate& date
        ) const override
    {
        const QDate legacyDate = toLegacyDate(date.value());
        if (!legacyDate.isValid())
        {
            return;
        }

        SettingsManager::instance().set(
            key(),
            legacyDate
            );
    }

private:
    [[nodiscard]] static QString key()
    {
        return QString::fromUtf8(
            SettingsManager::Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE
            );
    }

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
};

} // namespace ClassMngr::Next::Platform
