#pragma once

#include "core/result.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

class SettingsService;

struct PersonalDetails
{
    QString name;
    QString campus;
    QString zoomLoginId;
    QString zoomPassword;
    bool zoomNotAvailable = true;
    QByteArray signatureImage;
};

class PersonalDetailsRepository
{
public:
    explicit PersonalDetailsRepository(SettingsService* settingsService);

    PersonalDetails load() const;
    bool save(const PersonalDetails& details) const;
    [[nodiscard]] Status saveCampus(const QString& campus) const;

    bool isAvailable() const;
    QVariant loadSetting(const QString& key, const QVariant& defaultValue) const;
    [[nodiscard]] Status saveSetting(
        const QString& key,
        const QVariant& value
        ) const;

private:
    SettingsService* m_settingsService = nullptr;
};
