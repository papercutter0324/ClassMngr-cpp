#pragma once

#include "data/repositories/repository_error.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

class SettingsService;

enum class SignatureMode
{
    Image = 0,
    Type = 1
};

struct PersonalDetails
{
    QString name;
    QString campus;
    QString zoomLoginId;
    QString zoomPassword;
    bool zoomNotAvailable = true;
    QByteArray signatureImage;
    SignatureMode signatureMode = SignatureMode::Image;
    QString typedSignatureText;
    int typedSignatureFont = 0;
};

class PersonalDetailsRepository
{
public:
    explicit PersonalDetailsRepository(SettingsService* settingsService);

    [[nodiscard]] RepositoryResult<PersonalDetails> load() const;
    [[nodiscard]] RepositoryStatus save(const PersonalDetails& details) const;
    [[nodiscard]] RepositoryStatus saveCampus(const QString& campus) const;

    bool isAvailable() const;
    [[nodiscard]] RepositoryResult<QVariant> loadSetting(
        const QString& key,
        const QVariant& defaultValue
        ) const;
    [[nodiscard]] RepositoryStatus saveSetting(
        const QString& key,
        const QVariant& value
        ) const;

private:
    SettingsService* m_settingsService = nullptr;
};
