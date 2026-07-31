#include "testing_block_repository.h"

#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace
{
QString canonicalWeekday(
    const QString& day
    )
{
    const QString normalized =
        day.trimmed();
    const QStringList weekdays{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };

    for (const QString& weekday : weekdays)
    {
        if (
            normalized.compare(
                weekday,
                Qt::CaseInsensitive
                ) == 0
            )
        {
            return weekday;
        }
    }

    return {};
}

QString queryFailure(
    const QSqlQuery& query,
    const QString& action
    )
{
    return QObject::tr("%1 failed: %2")
        .arg(
            action,
            query.lastError().text()
            );
}

Status validateKey(
    const QString& day,
    const QString& startTime
    )
{
    if (
        canonicalWeekday(day).isEmpty()
        || startTime.trimmed().isEmpty()
        )
    {
        return std::unexpected(
            QObject::tr("A testing block requires a valid weekday and start time.")
            );
    }

    return {};
}
}

TestingBlockRepository::TestingBlockRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<QList<TestingBlock>>
TestingBlockRepository::loadTestingBlocks()
{
    QList<TestingBlock> blocks;
    QSqlQuery query(m_database);

    if (!query.exec(R"(
        SELECT
            day,
            start_time,
            room
        FROM schedule_testing_blocks
        ORDER BY day, start_time
    )"))
    {
        return std::unexpected(
            queryFailure(
                query,
                QObject::tr("Loading testing blocks")
                )
            );
    }

    while (query.next())
    {
        TestingBlock block;
        block.day =
            query.value(QStringLiteral("day")).toString();
        block.startTime =
            query.value(QStringLiteral("start_time")).toString();
        block.room =
            query.value(QStringLiteral("room")).toString();
        blocks.append(block);
    }

    return blocks;
}

Status TestingBlockRepository::saveTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room
    )
{
    const Status valid =
        validateKey(
            day,
            startTime
            );

    if (!valid)
    {
        return valid;
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO schedule_testing_blocks (
            day,
            start_time,
            room
        )
        VALUES (?, ?, ?)

        ON CONFLICT(day, start_time)
        DO UPDATE SET
            room=excluded.room
    )");
    query.addBindValue(canonicalWeekday(day));
    query.addBindValue(startTime.trimmed());
    const QString normalizedRoom =
        room.trimmed();
    query.addBindValue(
        normalizedRoom.isNull()
            ? QStringLiteral("")
            : normalizedRoom
        );

    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(
                query,
                QObject::tr("Saving the testing block")
                )
            );
    }

    return {};
}

Status TestingBlockRepository::deleteTestingBlock(
    const QString& day,
    const QString& startTime
    )
{
    const Status valid =
        validateKey(
            day,
            startTime
            );

    if (!valid)
    {
        return valid;
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        DELETE FROM schedule_testing_blocks
        WHERE day=?
        AND start_time=?
    )");
    query.addBindValue(canonicalWeekday(day));
    query.addBindValue(startTime.trimmed());

    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(
                query,
                QObject::tr("Removing the testing block")
                )
            );
    }

    return {};
}

Status TestingBlockRepository::clearTestingBlocks()
{
    QSqlQuery query(m_database);

    if (!query.exec(
            QStringLiteral(
                "DELETE FROM schedule_testing_blocks"
                )
            ))
    {
        return std::unexpected(
            queryFailure(
                query,
                QObject::tr("Clearing the testing layout")
                )
            );
    }

    return {};
}
