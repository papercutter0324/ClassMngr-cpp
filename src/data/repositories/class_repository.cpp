#include "class_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QDebug>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QStringList>

#include <limits>
#include <utility>

namespace
{
RepositoryStatus statusFromExecution(
    const SqlQueryUtils::ExecutionResult& result
    )
{
    if (!result)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            result.error().userMessage()
        });
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

RepositoryResult<int> ClassRepository::createClass(
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
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            executed.error().userMessage()
        });
    }

    bool idOk = false;
    const qint64 generatedClassId = query.lastInsertId().toLongLong(&idOk);
    if (!idOk || generatedClassId <= 0)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr(
                "Creating class for %1 failed: the database did not return "
                "a valid record id."
                ).arg(identity)
        });
    }

    if (generatedClassId > std::numeric_limits<int>::max())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr(
                "Creating class for %1 failed: generated record id %2 is "
                "outside the supported integer range."
                ).arg(identity, QString::number(generatedClassId))
        });
    }

    return static_cast<int>(generatedClassId);
}

RepositoryResult<QList<Classroom>> ClassRepository::getClasses()
{
    ++m_readMetrics.getClassesCallCount;

    QList<Classroom> classes;

    QSqlQuery query(m_database);

    ++m_readMetrics.getClassesStatementAttemptCount;
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
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            executed.error().userMessage()
        });
    }

    while (query.next())
    {
        Classroom classroom;

        bool classIdOk = false;
        const qlonglong storedClassId = query.value("id")
            .toLongLong(&classIdOk);
        if (!classIdOk
            || storedClassId <= 0
            || storedClassId > std::numeric_limits<int>::max())
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Loading classes failed: stored class id %1 is outside "
                    "the supported positive integer range."
                    ).arg(query.value("id").toString())
            });
        }

        classroom.id = static_cast<int>(storedClassId);

        classroom.name =
            query.value("name").toString();

        classes.append(classroom);
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            query.lastError().text()
        });
    }

    m_readMetrics.getClassesReturnedRowCount += classes.size();
    return classes;
}

RepositoryResult<QList<Classroom>>
ClassRepository::getBoundedClassesForNavigation(
    const int maximumClassCount,
    const int maximumNameLength
    )
{
    if (maximumClassCount <= 0
        || maximumClassCount == std::numeric_limits<int>::max()
        || maximumNameLength < 0)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::InvalidInput,
            QObject::tr("Loading classes navigation failed: invalid read bounds.")
        });
    }

    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    query.prepare(QStringLiteral(R"(
        SELECT c.id, c.name
        FROM classes c
        LEFT JOIN testing_classes tc
        ON tc.class_id = c.id
        WHERE tc.class_id IS NULL
        ORDER BY c.name, c.id
        LIMIT ?
    )"));
    query.addBindValue(maximumClassCount + 1);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading bounded classes navigation")
        );
    if (!executed)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            executed.error().userMessage()
        });
    }

    QList<Classroom> classes;
    classes.reserve(maximumClassCount);
    while (query.next())
    {
        if (classes.size() >= maximumClassCount)
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Loading classes navigation failed: the class list exceeds its bounded row limit."
                    )
            });
        }

        bool classIdOk = false;
        const qlonglong storedClassId = query.value(0).toLongLong(&classIdOk);
        if (!classIdOk
            || storedClassId <= 0
            || storedClassId > std::numeric_limits<int>::max())
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Loading classes navigation failed: stored class id %1 is outside the supported positive integer range."
                    ).arg(query.value(0).toString())
            });
        }

        const QString className = query.value(1).toString();
        if (className.size() > maximumNameLength)
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Loading classes navigation failed: a stored class name exceeds its bounded text limit."
                    )
            });
        }

        classes.append(Classroom{className, static_cast<int>(storedClassId)});
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            query.lastError().text()
        });
    }

    return classes;
}

RepositoryResult<Classroom> ClassRepository::getClassById(
    int classId
    )
{
    return lookupClassById(classId);
}

RepositoryResult<Classroom> ClassRepository::lookupClassById(
    int classId
    )
{
    ++m_readMetrics.getClassByIdCallCount;
    if (classId <= 0)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::InvalidInput,
            QObject::tr(
                "Loading class failed: invalid class id %1."
                ).arg(classId)
        });
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
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            executed.error().userMessage()
        });
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Technical,
                SqlQueryUtils::errorFor(
                    query,
                    QObject::tr("Loading class"),
                    {},
                    classIdentity(classId)
                    ).userMessage()
            });
        }

        return std::unexpected(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr(
                "Loading class failed for %1: no matching record exists."
                ).arg(classIdentity(classId))
        });
    }

    Classroom classroom;
    bool storedClassIdOk = false;
    const qlonglong storedClassId = query.value("id")
        .toLongLong(&storedClassIdOk);
    if (!storedClassIdOk
        || storedClassId <= 0
        || storedClassId > std::numeric_limits<int>::max())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr(
                "Loading class failed: stored class id %1 is outside the "
                "supported positive integer range."
                ).arg(query.value("id").toString())
        });
    }
    classroom.id = static_cast<int>(storedClassId);

    classroom.name =
        query.value("name").toString();

    return classroom;
}

RepositoryResult<QList<Classroom>> ClassRepository::getClassesByIds(
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
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::InvalidInput,
                QObject::tr(
                    "Loading classes failed: class identifiers must be positive and unique."
                    )
            });
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
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            loaded.error().userMessage()
        });
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
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Loading selected classes failed: returned class order did not match the request."
                    )
            });
        }
        if (loadedClassId.isNull())
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::NotFound,
                QObject::tr(
                    "Loading class failed for class id %1: no matching record exists."
                    ).arg(requestedClassId)
            });
        }

        bool classIdOk = false;
        const qlonglong storedClassId = loadedClassId.toLongLong(&classIdOk);
        if (!classIdOk
            || storedClassId <= 0
            || storedClassId > std::numeric_limits<int>::max())
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Loading selected classes failed: stored class id %1 is "
                    "outside the supported positive integer range."
                    ).arg(loadedClassId.toString())
            });
        }
        const int classId = static_cast<int>(storedClassId);
        if (classId != requestedClassId)
        {
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Loading selected classes failed: returned class identity did not match the request."
                    )
            });
        }

        classrooms.append(Classroom(
            query.value(QStringLiteral("class_name")).toString(),
            classId
            ));
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            query.lastError().text()
        });
    }
    if (classrooms.size() != classIds.size())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr(
                "Loading selected classes failed: the database returned an incomplete result."
                )
        });
    }

    return classrooms;
}

const ClassRepository::ReadMetrics& ClassRepository::readMetrics() const noexcept
{
    return m_readMetrics;
}

RepositoryStatus ClassRepository::updateClassName(
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

RepositoryStatus ClassRepository::deleteClass(
    int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::InvalidInput,
            QObject::tr("Deleting class failed: invalid class id %1.")
                .arg(classId)
        });
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Starting class deletion transaction failed for %1: %2")
                .arg(classIdentity(classId), m_database.lastError().text())
        });
    }

    const QString identity = classIdentity(classId);
    QSqlQuery query(m_database);
    const auto executeClassStatement =
        [&](const QString& sql, const QString& action) -> RepositoryStatus
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
        const RepositoryStatus deleted = executeClassStatement(sql, action);
        if (!deleted)
        {
            return deleted;
        }
    }

    query.prepare(QStringLiteral(R"(
        DELETE FROM speaking_eval_data
        WHERE evaluation_id IN (
            SELECT id
            FROM speaking_evaluations
            WHERE class_id=?
        )
    )"));
    query.addBindValue(classId);
    const RepositoryStatus evaluationDataDeleted = statusFromExecution(
        SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Deleting class speaking evaluation data"),
            identity
            )
        );
    if (!evaluationDataDeleted)
    {
        return evaluationDataDeleted;
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
        const RepositoryStatus deleted = executeClassStatement(sql, action);
        if (!deleted)
        {
            return deleted;
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Committing class deletion failed for %1: %2")
                .arg(identity, m_database.lastError().text())
        });
    }

    return {};
}
