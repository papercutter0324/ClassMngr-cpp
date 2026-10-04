#include "class_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QDebug>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QStringList>

#include <utility>

namespace
{
Status statusFromExecution(
    const SqlQueryUtils::ExecutionResult& result
    )
{
    if (!result)
    {
        return std::unexpected(result.error().userMessage());
    }

    return {};
}

QString classIdentity(int classId)
{
    return QObject::tr("class id %1").arg(classId);
}
} // namespace

ClassRepository::ClassRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<int> ClassRepository::createClass(
    const QString& name
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        INSERT INTO classes (
            name
        )
        VALUES (?)
    )");
    query.addBindValue(name);

    const QString identity = QObject::tr("class name '%1'")
        .arg(name);
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Creating class"),
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    const int classId = query.lastInsertId().toInt();
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr(
                "Creating class for %1 failed: the database did not return "
                "a valid record id."
                ).arg(identity)
            );
    }

    return classId;
}

Result<QList<Classroom>> ClassRepository::getClasses()
{
    QList<Classroom> classes;

    QSqlQuery query(m_database);

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
        SELECT c.*
        FROM classes c
        LEFT JOIN testing_classes tc
        ON tc.class_id = c.id
        WHERE tc.class_id IS NULL
        ORDER BY c.name
    )"),
        QObject::tr("Loading classes")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        Classroom classroom;

        classroom.id =
            query.value("id").toInt();

        classroom.name =
            query.value("name").toString();

        classes.append(classroom);
    }

    return classes;
}

Result<Classroom> ClassRepository::getClassById(
    int classId
    )
{
    ++m_readMetrics.getClassByIdCallCount;
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading class failed: invalid class id %1.")
                .arg(classId)
            );
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT *
        FROM classes
        WHERE id=?
    )");

    query.addBindValue(classId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading class"),
        classIdentity(classId)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    if (!query.next())
    {
        return std::unexpected(
            QObject::tr(
                "Loading class failed for %1: no matching record exists."
                ).arg(classIdentity(classId))
            );
    }

    Classroom classroom;
    classroom.id =
        query.value("id").toInt();

    classroom.name =
        query.value("name").toString();

    return classroom;
}

Result<QList<Classroom>> ClassRepository::getClassesByIds(
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return QList<Classroom>{};
    }

    ++m_readMetrics.getClassesByIdsCallCount;
    m_readMetrics.requestedClassCount += classIds.size();

    QSet<int> seenClassIds;
    QStringList requestedValues;
    requestedValues.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds[index];
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return std::unexpected(QObject::tr(
                "Loading classes failed: class identifiers must be positive and unique."
                ));
        }

        seenClassIds.insert(classId);
        requestedValues.append(QStringLiteral("(%1, %2)")
            .arg(classId)
            .arg(index));
    }

    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    ++m_readMetrics.batchStatementCount;
    const auto loaded = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            WITH requested(class_id, ordinal) AS (VALUES %1)
            SELECT requested.class_id AS requested_class_id,
                   classes.id AS class_id,
                   classes.name AS class_name
            FROM requested
            LEFT JOIN classes ON classes.id = requested.class_id
            ORDER BY requested.ordinal
        )").arg(requestedValues.join(QStringLiteral(", "))),
        QObject::tr("Loading selected classes"),
        QObject::tr("class ids %1").arg(requestedValues.join(
            QStringLiteral(", ")
            ))
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error().userMessage());
    }

    QList<Classroom> classrooms;
    classrooms.reserve(classIds.size());
    while (query.next())
    {
        const int requestedClassId =
            query.value(QStringLiteral("requested_class_id")).toInt();
        const QVariant loadedClassId = query.value(QStringLiteral("class_id"));
        if (classrooms.size() >= classIds.size()
            || requestedClassId != classIds[classrooms.size()])
        {
            return std::unexpected(QObject::tr(
                "Loading selected classes failed: returned class order did not match the request."
                ));
        }
        if (loadedClassId.isNull())
        {
            return std::unexpected(
                QObject::tr(
                    "Loading class failed for class id %1: no matching record exists."
                    ).arg(requestedClassId)
                );
        }

        const int classId = loadedClassId.toInt();
        if (classId != requestedClassId)
        {
            return std::unexpected(QObject::tr(
                "Loading selected classes failed: returned class identity did not match the request."
                ));
        }

        classrooms.append(Classroom(
            query.value(QStringLiteral("class_name")).toString(),
            classId
            ));
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(query.lastError().text());
    }
    if (classrooms.size() != classIds.size())
    {
        return std::unexpected(QObject::tr(
            "Loading selected classes failed: the database returned an incomplete result."
            ));
    }

    return classrooms;
}

const ClassRepository::ReadMetrics& ClassRepository::readMetrics() const noexcept
{
    return m_readMetrics;
}

Status ClassRepository::updateClassName(
    int classId,
    const QString& name
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        UPDATE classes
        SET name=?
        WHERE id=?
    )");

    query.addBindValue(name);
    query.addBindValue(classId);

    return statusFromExecution(
        SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Renaming class"),
            classIdentity(classId)
            )
        );
}

Status ClassRepository::deleteClass(
    int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Deleting class failed: invalid class id %1.")
                .arg(classId)
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Starting class deletion transaction failed for %1: %2")
                .arg(classIdentity(classId), m_database.lastError().text())
            );
    }

    const QString identity = classIdentity(classId);
    QSqlQuery query(m_database);
    const auto executeClassStatement =
        [&](const QString& sql, const QString& action) -> Status
        {
            query.prepare(sql);
            query.addBindValue(classId);
            return statusFromExecution(
                SqlQueryUtils::executePrepared(query, action, identity)
                );
        };

    for (const auto& [sql, action] : {
             std::pair{
                 QStringLiteral("DELETE FROM roster_columns WHERE class_id=?"),
                 QObject::tr("Deleting class roster columns")},
             std::pair{
                 QStringLiteral("DELETE FROM roster_data WHERE class_id=?"),
                 QObject::tr("Deleting class roster data")},
             std::pair{
                 QStringLiteral("DELETE FROM class_info WHERE class_id=?"),
                 QObject::tr("Deleting class information")},
             std::pair{
                 QStringLiteral("DELETE FROM class_times WHERE class_id=?"),
                 QObject::tr("Deleting class times")},
             std::pair{
                 QStringLiteral("DELETE FROM class_intensive_times WHERE class_id=?"),
                 QObject::tr("Deleting intensive class times")}
             })
    {
        const Status deleted = executeClassStatement(sql, action);
        if (!deleted)
        {
            return deleted;
        }
    }

    query.prepare(QStringLiteral(
        "SELECT id FROM speaking_evaluations WHERE class_id=?"
        ));
    query.addBindValue(classId);
    const Status evaluationsLoaded = statusFromExecution(
        SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Loading class speaking evaluations for deletion"),
            identity
            )
        );
    if (!evaluationsLoaded)
    {
        return evaluationsLoaded;
    }

    QList<int> evaluationIds;
    while (query.next())
    {
        evaluationIds.append(query.value(QStringLiteral("id")).toInt());
    }

    for (int evaluationId : evaluationIds)
    {
        query.prepare(QStringLiteral(
            "DELETE FROM speaking_eval_data WHERE evaluation_id=?"
            ));
        query.addBindValue(evaluationId);
        const Status evaluationDataDeleted = statusFromExecution(
            SqlQueryUtils::executePrepared(
                query,
                QObject::tr("Deleting speaking evaluation data"),
                QObject::tr("evaluation id %1 for %2")
                    .arg(evaluationId)
                    .arg(identity)
                )
            );
        if (!evaluationDataDeleted)
        {
            return evaluationDataDeleted;
        }
    }

    for (const auto& [sql, action] : {
             std::pair{
                 QStringLiteral(
                     "DELETE FROM speaking_evaluations WHERE class_id=?"
                     ),
                 QObject::tr("Deleting class speaking evaluations")},
             std::pair{
                 QStringLiteral("DELETE FROM classes WHERE id=?"),
                 QObject::tr("Deleting class")}
             })
    {
        const Status deleted = executeClassStatement(sql, action);
        if (!deleted)
        {
            return deleted;
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing class deletion failed for %1: %2")
                .arg(identity, m_database.lastError().text())
            );
    }

    return {};
}
