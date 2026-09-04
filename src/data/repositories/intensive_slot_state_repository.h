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
    explicit IntensiveSlotStateRepository(const QString& databasePath);
    // Compatibility-only constructor for retained Qt SQL tests/adapters.
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

    QString m_databasePath;
    bool m_compatibilityDatabaseWasOpen = true;
    mutable std::unique_ptr<classmngr::engine::SqliteDatabase> m_engineDatabase;
    mutable QString m_engineDatabasePath;
};
