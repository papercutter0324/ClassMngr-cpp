#pragma once

#include "core/result.h"
#include "domain/models/intensive_slot_state.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>

#include <memory>

namespace classmngr::engine
{
class SqliteDatabase;
}

class IntensiveSlotStateRepository
{
public:
    explicit IntensiveSlotStateRepository(
        QSqlDatabase& database
        );
    ~IntensiveSlotStateRepository();

    [[nodiscard]] Result<QList<IntensiveSlotState>> loadIntensiveSlotStates();

    [[nodiscard]] Status saveIntensiveSlotState(
        const QString& day,
        const QString& startTime,
        const QString& state,
        const QString& defaultState = QStringLiteral("essay")
        );

private:
    [[nodiscard]] Status ensureEngineDatabase(
        const QString& operation
        ) const;

    QSqlDatabase& m_database;
    mutable std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    mutable QString m_engineDatabasePath;
};
