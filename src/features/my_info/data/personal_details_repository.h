#pragma once

#include <QByteArray>
#include <QString>
#include <QVariant>

class DataService;
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
    explicit PersonalDetailsRepository(DataService* dataService);
    explicit PersonalDetailsRepository(SettingsService* settingsService);

    PersonalDetails load() const;
    bool save(const PersonalDetails& details) const;
    void saveCampus(const QString& campus) const;

    bool isAvailable() const;
    QVariant loadSetting(const QString& key, const QVariant& defaultValue) const;
    void saveSetting(const QString& key, const QVariant& value) const;

private:
    DataService* m_dataService = nullptr;
    SettingsService* m_settingsService = nullptr;

};
