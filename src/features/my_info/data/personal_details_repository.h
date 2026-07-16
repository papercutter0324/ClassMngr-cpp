#pragma once

#include <QByteArray>
#include <QString>

class DataService;

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

    PersonalDetails load() const;
    bool save(const PersonalDetails& details) const;
    void saveCampus(const QString& campus) const;

private:
    DataService* m_dataService = nullptr;
};
