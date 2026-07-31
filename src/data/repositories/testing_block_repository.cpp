#include "testing_block_repository.h"

#include "data/database/database_transaction.h"

#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTime>

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

QString canonicalStartTime(
    const QString& startTime
    )
{
    const QString normalized =
        startTime.trimmed();
    const QTime parsed =
        QTime::fromString(
            normalized,
            QStringLiteral("HH:mm")
            );
    if (
        !parsed.isValid()
        || parsed.toString(QStringLiteral("HH:mm")) != normalized
        )
    {
        return {};
    }
    return normalized;
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
        || canonicalStartTime(startTime).isEmpty()
        )
    {
        return std::unexpected(
            QObject::tr("A testing block requires a valid weekday and start time.")
            );
    }

    return {};
}

Result<int> existingClassId(
    QSqlDatabase& database,
    const QString& day,
    const QString& startTime
    )
{
    QSqlQuery query(database);
    query.prepare(R"(
        SELECT class_id
        FROM schedule_testing_blocks
        WHERE day=?
        AND start_time=?
    )");
    query.addBindValue(canonicalWeekday(day));
    query.addBindValue(canonicalStartTime(startTime));

    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(
                query,
                QObject::tr("Checking the testing assignment")
                )
            );
    }

    if (!query.next())
    {
        return -1;
    }

    if (query.value(QStringLiteral("class_id")).isNull())
    {
        return 0;
    }

    return query.value(QStringLiteral("class_id")).toInt();
}

Status validateTestingClass(
    QSqlDatabase& database,
    int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("A valid testing class is required.")
            );
    }

    QSqlQuery query(database);
    query.prepare(R"(
        SELECT
            c.name,
            ci.class_grade,
            ci.class_level,
            tc.room
        FROM testing_classes tc
        JOIN classes c
        ON c.id=tc.class_id
        JOIN class_info ci
        ON ci.class_id=tc.class_id
        WHERE tc.class_id=?
    )");
    query.addBindValue(classId);

    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(
                query,
                QObject::tr("Checking the testing class")
                )
            );
    }

    if (!query.next())
    {
        return std::unexpected(
            QObject::tr("The selected testing class no longer exists.")
            );
    }

    if (
        query.value(QStringLiteral("name"))
            .toString().trimmed().isEmpty()
        || query.value(QStringLiteral("class_grade"))
            .toString().trimmed().isEmpty()
        || query.value(QStringLiteral("class_level"))
            .toString().trimmed().isEmpty()
        || query.value(QStringLiteral("room"))
            .toString().trimmed().isEmpty()
        )
    {
        return std::unexpected(
            QObject::tr(
                "The selected testing class is missing required details."
                )
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
    const Result<QList<TestingAssignment>> assignments =
        loadTestingAssignments();

    if (!assignments)
    {
        return std::unexpected(assignments.error());
    }

    for (const TestingAssignment& assignment : *assignments)
    {
        if (assignment.kind != TestingAssignmentKind::PlainTesting)
        {
            continue;
        }

        blocks.append(
            {
                assignment.day,
                assignment.startTime,
                assignment.room
            }
            );
    }

    return blocks;
}

Result<QList<TestingAssignment>>
TestingBlockRepository::loadTestingAssignments()
{
    QList<TestingAssignment> assignments;
    QSqlQuery query(m_database);

    if (!query.exec(R"(
        SELECT
            day,
            start_time,
            room,
            class_id
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
        TestingAssignment assignment;
        assignment.day =
            query.value(QStringLiteral("day")).toString();
        assignment.startTime =
            query.value(QStringLiteral("start_time")).toString();
        assignment.room =
            query.value(QStringLiteral("room")).toString();
        assignment.classId =
            query.value(QStringLiteral("class_id")).isNull()
                ? -1
                : query.value(QStringLiteral("class_id")).toInt();
        assignment.kind =
            assignment.classId > 0
                ? TestingAssignmentKind::SpecialClass
                : TestingAssignmentKind::PlainTesting;
        assignments.append(assignment);
    }

    return assignments;
}

Status TestingBlockRepository::saveTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room,
    bool replaceExisting
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

    const Result<int> existing =
        existingClassId(
            m_database,
            day,
            startTime
            );

    if (!existing)
    {
        return std::unexpected(existing.error());
    }

    if (*existing > 0 && !replaceExisting)
    {
        return std::unexpected(
            QObject::tr("This slot is assigned to a testing class. Confirm replacement first.")
            );
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO schedule_testing_blocks (
            day,
            start_time,
            room,
            class_id
        )
        VALUES (?, ?, ?, NULL)

        ON CONFLICT(day, start_time)
        DO UPDATE SET
            room=excluded.room,
            class_id=NULL
    )");
    query.addBindValue(canonicalWeekday(day));
    query.addBindValue(canonicalStartTime(startTime));
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

Status TestingBlockRepository::assignTestingClass(
    const QString& day,
    const QString& startTime,
    int classId,
    bool replaceExisting
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

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr(
                "Could not start the testing assignment transaction."
                )
            );
    }

    const Status validClass =
        validateTestingClass(
            m_database,
            classId
            );

    if (!validClass)
    {
        return validClass;
    }

    const Result<int> existing =
        existingClassId(
            m_database,
            day,
            startTime
            );

    if (!existing)
    {
        return std::unexpected(existing.error());
    }

    if (
        *existing >= 0
        && *existing != classId
        && !replaceExisting
        )
    {
        return std::unexpected(
            QObject::tr("This slot already has a testing assignment. Confirm replacement first.")
            );
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO schedule_testing_blocks (
            day,
            start_time,
            room,
            class_id
        )
        VALUES (?, ?, '', ?)

        ON CONFLICT(day, start_time)
        DO UPDATE SET
            room='',
            class_id=excluded.class_id
    )");
    query.addBindValue(canonicalWeekday(day));
    query.addBindValue(canonicalStartTime(startTime));
    query.addBindValue(classId);

    if (!query.exec())
    {
        return std::unexpected(
            queryFailure(
                query,
                QObject::tr("Assigning the testing class")
                )
            );
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr(
                "Committing the testing assignment transaction failed: %1"
                )
                .arg(m_database.lastError().text())
            );
    }

    return {};
}

Status TestingBlockRepository::deleteTestingAssignment(
    const QString& day,
    const QString& startTime
    )
{
    return deleteTestingBlock(
        day,
        startTime
        );
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
    query.addBindValue(canonicalStartTime(startTime));

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
    return clearTestingAssignments();
}

Status TestingBlockRepository::clearTestingAssignments()
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
