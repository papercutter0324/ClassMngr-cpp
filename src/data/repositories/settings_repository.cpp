#include "settings_repository.h"

#include <QSqlQuery>

SettingsRepository::SettingsRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

void SettingsRepository::saveSetting(
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

    query.exec();
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
