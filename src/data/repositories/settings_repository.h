#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVariant>

class SettingsRepository
{
public:
    explicit SettingsRepository(
        QSqlDatabase& database
        );

    void saveSetting(
        const QString& key,
        const QVariant& value
        );

    QVariant loadSetting(
        const QString& key,
        const QVariant& defaultValue = QVariant()
        );

private:
    QSqlDatabase& m_database;
};
