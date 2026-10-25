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

RepositoryStatus SettingsRepository::saveSetting(
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
        return std::unexpected(
            SqlQueryUtils::repositoryErrorFor(executed.error()));
    }

    return {};
}

RepositoryStatus SettingsRepository::saveSettings(
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
        return std::unexpected(RepositoryError{
            QObject::tr("Starting application settings transaction failed: %1")
                .arg(m_database.lastError().text())
            });
    }

    for (auto setting = values.cbegin(); setting != values.cend(); ++setting)
    {
        const RepositoryStatus saved = saveSetting(
            setting.key(),
            setting.value());
        if (!saved)
        {
            return saved;
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(RepositoryError{
            QObject::tr("Committing application settings failed: %1")
                .arg(m_database.lastError().text())
            });
    }

    return {};
}

RepositoryResult<QVariant> SettingsRepository::loadSetting(
    const QString& key
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT value
        FROM app_settings
        WHERE key=?
    )");

    query.addBindValue(key);

    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading application setting"),
        QObject::tr("setting key '%1'").arg(key)
        );
    if (!loaded)
    {
        return std::unexpected(
            SqlQueryUtils::repositoryErrorFor(loaded.error()));
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return std::unexpected(SqlQueryUtils::repositoryErrorFor(
                SqlQueryUtils::errorFor(
                    query,
                    QObject::tr("Loading application setting"),
                    {},
                    QObject::tr("setting key '%1'").arg(key))));
        }
        return QVariant();
    }

    return query.value(0);
}
