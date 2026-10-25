#include "intensive_slot_state_repository.h"

#include "data/database/sql_query_utils.h"

#include <QDebug>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>

IntensiveSlotStateRepository::IntensiveSlotStateRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

RepositoryResult<QList<IntensiveSlotState>>
IntensiveSlotStateRepository::loadIntensiveSlotStates()
{
    QList<IntensiveSlotState> states;

    QSqlQuery query(m_database);
    const QString action =
        QObject::tr("Loading intensive slot states");

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
        SELECT
            day,
            start_time,
            state
        FROM intensive_slot_states
        ORDER BY day, start_time
    )"),
        action
        );
    if (!executed)
    {
        return std::unexpected(
            SqlQueryUtils::repositoryErrorFor(executed.error()));
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

    if (query.lastError().isValid())
    {
        return std::unexpected(
            SqlQueryUtils::repositoryErrorFor(
                SqlQueryUtils::errorFor(query, action)));
    }

    return states;
}

RepositoryStatus IntensiveSlotStateRepository::saveIntensiveSlotState(
    const QString& day,
    const QString& startTime,
    const QString& state,
    const QString& defaultState
    )
{
    QSqlQuery query(m_database);
    const QString identity = QObject::tr("intensive slot %1 at %2")
        .arg(day, startTime);

    if (state == defaultState)
    {
        query.prepare(R"(
            DELETE FROM intensive_slot_states
            WHERE day=?
            AND start_time=?
        )");

        query.addBindValue(day);
        query.addBindValue(startTime);

        const auto executed = SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Deleting intensive slot state"),
            identity
            );
        if (!executed)
        {
            return std::unexpected(
                SqlQueryUtils::repositoryErrorFor(executed.error()));
        }

        return {};
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

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Saving intensive slot state"),
        identity
        );
    if (!executed)
    {
        return std::unexpected(
            SqlQueryUtils::repositoryErrorFor(executed.error()));
    }

    return {};
}
