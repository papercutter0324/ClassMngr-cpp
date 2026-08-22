#pragma once

#include "core/result.h"

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

    [[nodiscard]] Status saveSetting(
        const QString& key,
        const QVariant& value
        );

    [[nodiscard]] Status saveSettings(
        const QVariantMap& values
        );

    [[nodiscard]] Result<QVariant> loadSetting(
        const QString& key
        );

private:
    QSqlDatabase& m_database;
};
