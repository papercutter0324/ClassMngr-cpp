#pragma once

#include "core/result.h"

#include <QSqlDatabase>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class SettingsRepository
{
public:
    explicit SettingsRepository(const QString& databasePath);
    // Compatibility-only constructor for retained Qt SQL tests/adapters.
    explicit SettingsRepository(
        QSqlDatabase& database
        );
    ~SettingsRepository();

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
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation,
        const QString& settingContext = {}
        );

    QString m_databasePath;
    bool m_compatibilityDatabaseWasOpen = true;
    std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    QString m_engineDatabasePath;
};
