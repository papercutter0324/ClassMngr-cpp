#include "intensive_slot_state_repository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

IntensiveSlotStateRepository::IntensiveSlotStateRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

QList<IntensiveSlotState> IntensiveSlotStateRepository::loadIntensiveSlotStates()
{
    QList<IntensiveSlotState> states;

    QSqlQuery query(m_database);

    if (!query.exec(R"(
        SELECT
            day,
            start_time,
            state
        FROM intensive_slot_states
        ORDER BY day, start_time
    )"))
    {
        qWarning()
            << "Failed to load intensive slot states:"
            << query.lastError().text();

        return states;
    }

    while (query.next())
    {
        IntensiveSlotState state;

        state.day =
            query.value("day").toString();

        state.startTime =
            query.value("start_time").toString();

        state.state =
            query.value("state").toString();

        states.append(state);
    }

    return states;
}

void IntensiveSlotStateRepository::saveIntensiveSlotState(
    const QString& day,
    const QString& startTime,
    const QString& state
    )
{
    QSqlQuery query(m_database);

    if (state == QStringLiteral("essay"))
    {
        query.prepare(R"(
            DELETE FROM intensive_slot_states
            WHERE day=?
            AND start_time=?
        )");

        query.addBindValue(day);
        query.addBindValue(startTime);

        if (!query.exec())
        {
            qWarning()
                << "Failed to delete intensive slot state:"
                << query.lastError().text();
        }

        return;
    }

    query.prepare(R"(
        INSERT INTO intensive_slot_states (
            day,
            start_time,
            state
        )
        VALUES (?, ?, ?)

        ON CONFLICT(day, start_time)
        DO UPDATE SET
            state=excluded.state
    )");

    query.addBindValue(day);
    query.addBindValue(startTime);
    query.addBindValue(state);

    if (!query.exec())
    {
        qWarning()
            << "Failed to save intensive slot state:"
            << query.lastError().text();
    }
}
