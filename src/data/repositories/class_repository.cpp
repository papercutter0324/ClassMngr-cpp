#include "class_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QDebug>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>

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

QList<Classroom> ClassRepository::getClasses()
{
    QList<Classroom> classes;

    QSqlQuery query(m_database);

    if (!query.exec(R"(
        SELECT c.*
        FROM classes c
        LEFT JOIN testing_classes tc
        ON tc.class_id = c.id
        WHERE tc.class_id IS NULL
        ORDER BY c.name
    )"))
    {
        qWarning()
            << "Failed to load classes:"
            << query.lastError().text();

        return classes;
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

Classroom ClassRepository::getClassById(
    int classId
    )
{
    Classroom classroom;

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT *
        FROM classes
        WHERE id=?
    )");

    query.addBindValue(classId);

    if (!query.exec())
    {
        qWarning()
            << "Failed to load class:"
            << query.lastError().text();
        return classroom;
    }

    if (!query.next())
    {
        return classroom;
    }

    classroom.id =
        query.value("id").toInt();

    classroom.name =
        query.value("name").toString();

    return classroom;
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
