#include "settings_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QObject>
#include <QSqlError>
#include <QSqlQuery>

SettingsRepository::SettingsRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Status SettingsRepository::saveSetting(
    const QString& key,
    const QVariant& value
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        INSERT INTO app_settings (
            key,
            value
        )
        VALUES (?, ?)

        ON CONFLICT(key)
        DO UPDATE SET
            value=excluded.value
    )");

    query.addBindValue(key);
    query.addBindValue(value);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Saving application setting"),
        QObject::tr("setting key '%1'").arg(key)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    return {};
}

Status SettingsRepository::saveSettings(
    const QVariantMap& values
    )
{
    if (values.isEmpty())
    {
        return {};
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Starting application settings transaction failed: %1")
                .arg(m_database.lastError().text())
            );
    }

    for (auto setting = values.cbegin(); setting != values.cend(); ++setting)
    {
        const Status saved = saveSetting(setting.key(), setting.value());
        if (!saved)
        {
            return saved;
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing application settings failed: %1")
                .arg(m_database.lastError().text())
            );
    }

    return {};
}

QVariant SettingsRepository::loadSetting(
    const QString& key,
    const QVariant& defaultValue
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT value
        FROM app_settings
        WHERE key=?
    )");

    query.addBindValue(key);

    if (!query.exec())
    {
        return defaultValue;
    }

    if (!query.next())
    {
        return defaultValue;
    }

    return query.value(0);
}
