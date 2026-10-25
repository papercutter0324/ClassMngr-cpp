#pragma once

#include "data/repositories/repository_error.h"

#include <QSqlDatabase>
#include <QString>
#include <QVariant>
#include <QVariantMap>

class SettingsRepository
{
public:
    explicit SettingsRepository(
        QSqlDatabase& database
        );

    [[nodiscard]] RepositoryStatus saveSetting(
        const QString& key,
        const QVariant& value
        );

    [[nodiscard]] RepositoryStatus saveSettings(
        const QVariantMap& values
        );

    [[nodiscard]] RepositoryResult<QVariant> loadSetting(
        const QString& key
        );

private:
    QSqlDatabase& m_database;
};
