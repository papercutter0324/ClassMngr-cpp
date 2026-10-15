#include "teacher_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"

#include <QDebug>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QStringList>

#include <limits>

namespace
{
QString normalizedTeacherChoice(
    const QString& value,
    const QStringList& choices
    )
{
    const QString trimmed =
        value.trimmed();

    for (const QString& choice : choices)
    {
        if (choice.compare(trimmed, Qt::CaseInsensitive) == 0)
        {
            return choice;
        }
    }

    // Keep legacy or invalid values observable. Feature services validate new
    // writes, and silently turning an unknown value into a default hides data
    // corruption from callers.
    return trimmed;
}

QString normalizedInternetType(
    const QString& value
    )
{
    return normalizedTeacherChoice(
        value,
        {
            QStringLiteral("WiFi"),
            QStringLiteral("LAN"),
            QStringLiteral("Both"),
            QStringLiteral("N/A")
        }
        );
}

QString normalizedProjectionType(
    const QString& value
    )
{
    return normalizedTeacherChoice(
        value,
        {
            QStringLiteral("HDMI"),
            QStringLiteral("Zoom"),
            QStringLiteral("Any"),
            QStringLiteral("N/A")
        }
        );
}

Teacher teacherFromQuery(
    const QSqlQuery& query
    )
{
    Teacher teacher;

    teacher.id =           query.value("id").toInt();
    teacher.teacherKr =    query.value("teacher_kr").toString();
    teacher.teacherEn =    query.value("teacher_en").toString();
    teacher.preferredRomanization =
        query.value("preferred_romanization").toString();
    teacher.preferredName =
        query.value("preferred_name").toString();
    teacher.roomNumber =   query.value("room_number").toString();
    teacher.birthday =     query.value("birthday").toString();
    teacher.phoneNumber =  query.value("phone_number").toString();
    teacher.wifiName =     query.value("wifi_name").toString();
    teacher.wifiPassword = query.value("wifi_password").toString();
    teacher.internetType =
        normalizedInternetType(
            query.value("internet_type").toString()
            );
    teacher.zoomId =       query.value("zoom_id").toString();
    teacher.zoomPassword = query.value("zoom_password").toString();
    teacher.projectionType =
        normalizedProjectionType(
            query.value("projection_type").toString()
            );
    teacher.notes =        query.value("notes").toString();

    return teacher;
}

QString teacherIdentity(qint64 teacherId)
{
    return QObject::tr("teacher id %1").arg(teacherId);
}

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
}

TeacherRepository::TeacherRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<int> TeacherRepository::createTeacher(
    const Teacher& teacher
    )
{
    const Result<qint64> created = createTeacherWithWideId(teacher);
    if (!created)
    {
        return std::unexpected(created.error());
    }
    if (*created > std::numeric_limits<int>::max())
    {
        return std::unexpected(
            QObject::tr(
                "Creating teacher failed: generated record id %1 exceeds "
                "the supported range for this caller."
                ).arg(QString::number(*created))
            );
    }

    return static_cast<int>(*created);
}

Result<qint64> TeacherRepository::createTeacherWithWideId(
    const Teacher& teacher
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        INSERT INTO teachers (
            teacher_kr,
            teacher_en,
            preferred_romanization,
            preferred_name,
            room_number,
            birthday,
            phone_number,
            wifi_name,
            wifi_password,
            internet_type,
            zoom_id,
            zoom_password,
            projection_type,
            notes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.preferredRomanization);
    query.addBindValue(teacher.preferredName);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.birthday);
    query.addBindValue(teacher.phoneNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(
        normalizedInternetType(teacher.internetType));
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
    query.addBindValue(
        normalizedProjectionType(teacher.projectionType));
    query.addBindValue(teacher.notes);

    const QString identity = QObject::tr("teacher '%1'")
        .arg(
            teacher.teacherEn.trimmed().isEmpty()
                ? teacher.teacherKr.trimmed()
                : teacher.teacherEn.trimmed()
            );
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Creating teacher"),
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    bool idOk = false;
    const qint64 teacherId = query.lastInsertId().toLongLong(&idOk);
    if (!idOk || teacherId <= 0)
    {
        return std::unexpected(
            QObject::tr(
                "Creating %1 failed: the database did not return a valid "
                "record id."
                ).arg(identity)
            );
    }

    return teacherId;
}

Result<int> TeacherRepository::saveTeacher(
    const Teacher& teacher
    )
{
    if (teacher.id > 0)
    {
        const Status updated = updateTeacher(teacher);
        if (!updated)
        {
            return std::unexpected(updated.error());
        }

        return teacher.id;
    }

    return createTeacher(teacher);
}

Status TeacherRepository::updateTeacher(
    const Teacher& teacher
    )
{
    const TeacherUpdateOutcome outcome = updateTeacherWithOutcome(teacher);
    if (outcome.code != TeacherUpdateOutcomeCode::Updated)
    {
        return std::unexpected(outcome.diagnostic);
    }

    return {};
}

TeacherUpdateOutcome TeacherRepository::updateTeacherWithOutcome(
    const Teacher& teacher
    )
{
    QSqlQuery query(m_database);

    query.prepare(R"(
        UPDATE teachers
        SET
            teacher_kr=?,
            teacher_en=?,
            preferred_romanization=?,
            preferred_name=?,
            room_number=?,
            birthday=?,
            phone_number=?,
            wifi_name=?,
            wifi_password=?,
            internet_type=?,
            zoom_id=?,
            zoom_password=?,
            projection_type=?,
            notes=?
        WHERE id=?
    )");

    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.preferredRomanization);
    query.addBindValue(teacher.preferredName);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.birthday);
    query.addBindValue(teacher.phoneNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(
        normalizedInternetType(teacher.internetType));
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
    query.addBindValue(
        normalizedProjectionType(teacher.projectionType));
    query.addBindValue(teacher.notes);
    query.addBindValue(teacher.id);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Updating teacher"),
        teacherIdentity(teacher.id)
        );
    if (!executed)
    {
        return {
            .code = TeacherUpdateOutcomeCode::Failure,
            .diagnostic = executed.error().userMessage()
        };
    }

    if (query.numRowsAffected() == 0)
    {
        return {
            .code = TeacherUpdateOutcomeCode::Missing,
            .diagnostic = QObject::tr(
                "Updating teacher failed for %1: no matching record exists."
                ).arg(teacherIdentity(teacher.id))
        };
    }

    return {.code = TeacherUpdateOutcomeCode::Updated};
}

Result<Teacher> TeacherRepository::getTeacher(
    int teacherId
    )
{
    const TeacherByIdLookupResult lookup = lookupTeacherById(teacherId);
    if (lookup.code == TeacherByIdLookupCode::Found)
    {
        return lookup.teacher;
    }

    return std::unexpected(lookup.diagnostic);
}

TeacherByIdLookupResult TeacherRepository::lookupTeacherById(
    int teacherId
    )
{
    if (teacherId <= 0)
    {
        return {
            .code = TeacherByIdLookupCode::Failure,
            .diagnostic = QObject::tr(
                "Loading teacher failed: invalid teacher id %1."
                ).arg(teacherId)
        };
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT *
        FROM teachers
        WHERE id=?
    )");

    query.addBindValue(teacherId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading teacher"),
        teacherIdentity(teacherId)
        );
    if (!executed)
    {
        return {
            .code = TeacherByIdLookupCode::Failure,
            .diagnostic = executed.error().userMessage()
        };
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return {
                .code = TeacherByIdLookupCode::Failure,
                .diagnostic = SqlQueryUtils::errorFor(
                    query,
                    QObject::tr("Loading teacher"),
                    {},
                    teacherIdentity(teacherId)
                    ).userMessage()
            };
        }

        return {
            .code = TeacherByIdLookupCode::Missing,
            .diagnostic = QObject::tr(
                "Loading teacher failed for %1: no matching record exists."
                ).arg(teacherIdentity(teacherId))
        };
    }

    return {
        .code = TeacherByIdLookupCode::Found,
        .teacher = teacherFromQuery(query)
    };
}

Result<TeacherDisplayNameReadRecord>
TeacherRepository::loadTeacherDisplayNameFields(
    int teacherId
    )
{
    return loadTeacherDisplayNameFields(static_cast<qint64>(teacherId));
}

Result<TeacherDisplayNameReadRecord>
TeacherRepository::loadTeacherDisplayNameFields(
    qint64 teacherId
    )
{
    const TeacherDisplayNameLookupResult lookup =
        lookupTeacherDisplayName(teacherId);
    if (lookup.code == TeacherDisplayNameLookupCode::Found)
    {
        return lookup.record;
    }
    return std::unexpected(lookup.diagnostic);
}

TeacherDisplayNameLookupResult
TeacherRepository::lookupTeacherDisplayName(
    int teacherId
    )
{
    return lookupTeacherDisplayName(static_cast<qint64>(teacherId));
}

TeacherDisplayNameLookupResult
TeacherRepository::lookupTeacherDisplayName(
    qint64 teacherId
    )
{
    if (teacherId <= 0)
    {
        return {
            .code = TeacherDisplayNameLookupCode::InvalidInput,
            .diagnostic = QObject::tr(
                "Loading teacher display name failed: invalid teacher id %1."
                ).arg(teacherId)
        };
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT id AS teacher_id,
               teacher_kr,
               teacher_en,
               preferred_romanization,
               preferred_name
        FROM teachers
        WHERE id=?
    )");
    query.addBindValue(teacherId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading teacher display name"),
        teacherIdentity(teacherId)
        );
    if (!executed)
    {
        return {
            .code = TeacherDisplayNameLookupCode::Failure,
            .diagnostic = executed.error().userMessage()
        };
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return {
                .code = TeacherDisplayNameLookupCode::Failure,
                .diagnostic = SqlQueryUtils::errorFor(
                    query,
                    QObject::tr("Loading teacher display name"),
                    {},
                    teacherIdentity(teacherId)
                    ).userMessage()
            };
        }

        return {
            .code = TeacherDisplayNameLookupCode::Missing,
            .diagnostic = QObject::tr(
                "Loading teacher display name failed for %1: no matching record exists."
                ).arg(teacherIdentity(teacherId))
        };
    }

    if (query.lastError().isValid())
    {
        return {
            .code = TeacherDisplayNameLookupCode::Failure,
            .diagnostic = SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading teacher display name"),
                {},
                teacherIdentity(teacherId)
                ).userMessage()
        };
    }

    bool returnedTeacherIdOk = false;
    const qint64 returnedTeacherId = query.value("teacher_id").toLongLong(
        &returnedTeacherIdOk
        );
    if (!returnedTeacherIdOk || returnedTeacherId != teacherId)
    {
        return {
            .code = TeacherDisplayNameLookupCode::Failure,
            .diagnostic = QObject::tr(
                "Loading teacher display name failed for %1: the database returned an invalid teacher identifier."
                ).arg(teacherIdentity(teacherId))
        };
    }

    return {
        .code = TeacherDisplayNameLookupCode::Found,
        .record = {
            query.value("teacher_kr").toString(),
            query.value("teacher_en").toString(),
            query.value("preferred_romanization").toString(),
            query.value("preferred_name").toString()
        }
    };
}

Result<QList<TeacherDisplayNameBatchReadRecord>>
TeacherRepository::loadTeacherDisplayNameRecords(
    const QList<int>& teacherIds
    )
{
    QList<qint64> wideTeacherIds;
    wideTeacherIds.reserve(teacherIds.size());
    for (const int teacherId : teacherIds)
    {
        wideTeacherIds.append(teacherId);
    }

    const auto loaded = loadMyClassesTeacherDisplayNameRecords(
        wideTeacherIds
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }

    QList<TeacherDisplayNameBatchReadRecord> records;
    records.reserve(loaded->size());
    for (const MyClassesTeacherDisplayNameBatchReadRecord& record : *loaded)
    {
        if (record.teacherId < std::numeric_limits<int>::min()
            || record.teacherId > std::numeric_limits<int>::max())
        {
            return std::unexpected(QObject::tr(
                "Loading teacher display names failed: the database returned a teacher identifier outside the supported range."
                ));
        }

        records.append({
            static_cast<int>(record.teacherId),
            record.teacherKr,
            record.teacherEn,
            record.preferredRomanization,
            record.preferredName
        });
    }
    return records;
}

Result<QList<MyClassesTeacherDisplayNameBatchReadRecord>>
TeacherRepository::loadMyClassesTeacherDisplayNameRecords(
    const QList<qint64>& teacherIds
    )
{
    ++m_teacherDisplayNameBatchReadMetrics.callCount;
    if (teacherIds.isEmpty())
    {
        return QList<MyClassesTeacherDisplayNameBatchReadRecord>{};
    }

    m_teacherDisplayNameBatchReadMetrics.requestedTeacherCount +=
        teacherIds.size();

    QSet<qint64> seenTeacherIds;
    QStringList requestedValues;
    requestedValues.reserve(teacherIds.size());
    for (qsizetype index = 0; index < teacherIds.size(); ++index)
    {
        const qint64 teacherId = teacherIds[index];
        if (teacherId <= 0 || seenTeacherIds.contains(teacherId))
        {
            return std::unexpected(QObject::tr(
                "Loading teacher display names failed: teacher identifiers must be positive and unique."
                ));
        }

        seenTeacherIds.insert(teacherId);
        requestedValues.append(QStringLiteral("(%1, %2)")
            .arg(teacherId)
            .arg(index));
    }

    QSqlQuery query(m_database);
    ++m_teacherDisplayNameBatchReadMetrics.statementCount;
    const QString queryText = QStringLiteral(R"(
        WITH requested(teacher_id, ordinal) AS (VALUES %1)
        SELECT requested.teacher_id AS requested_teacher_id,
               requested.ordinal AS requested_ordinal,
               teachers.id AS teacher_id,
               teachers.teacher_kr,
               teachers.teacher_en,
               teachers.preferred_romanization,
               teachers.preferred_name
        FROM requested
        INNER JOIN teachers ON teachers.id = requested.teacher_id
        ORDER BY requested.ordinal
    )").arg(requestedValues.join(QStringLiteral(", ")));
    const auto executed = SqlQueryUtils::execute(
        query,
        queryText,
        QObject::tr("Loading teacher display names")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    QList<MyClassesTeacherDisplayNameBatchReadRecord> records;
    records.reserve(teacherIds.size());
    qsizetype lastOrdinal = -1;
    while (query.next())
    {
        bool requestedTeacherIdOk = false;
        const qint64 requestedTeacherId = query.value(
            "requested_teacher_id"
            ).toLongLong(&requestedTeacherIdOk);
        bool requestedOrdinalOk = false;
        const qint64 requestedOrdinal = query.value(
            "requested_ordinal"
            ).toLongLong(&requestedOrdinalOk);
        bool returnedTeacherIdOk = false;
        const qint64 returnedTeacherId = query.value("teacher_id").toLongLong(
            &returnedTeacherIdOk
            );
        if (!requestedTeacherIdOk
            || !requestedOrdinalOk
            || !returnedTeacherIdOk
            || requestedOrdinal < 0
            || requestedOrdinal >= teacherIds.size())
        {
            return std::unexpected(QObject::tr(
                "Loading teacher display names failed: the database returned an invalid teacher identifier."
                ));
        }

        const qsizetype ordinal = static_cast<qsizetype>(requestedOrdinal);
        const qint64 expectedTeacherId = teacherIds[ordinal];
        if (ordinal <= lastOrdinal
            || requestedTeacherId != expectedTeacherId
            || returnedTeacherId != expectedTeacherId)
        {
            return std::unexpected(QObject::tr(
                "Loading teacher display names failed: the database returned records in a different identifier order."
                ));
        }
        lastOrdinal = ordinal;

        records.append({
            returnedTeacherId,
            query.value("teacher_kr").toString(),
            query.value("teacher_en").toString(),
            query.value("preferred_romanization").toString(),
            query.value("preferred_name").toString()
        });
    }

    return records;
}

const TeacherDisplayNameBatchReadMetrics&
TeacherRepository::teacherDisplayNameBatchReadMetrics() const noexcept
{
    return m_teacherDisplayNameBatchReadMetrics;
}

Result<QList<TeacherProfileBatchReadRecord>>
TeacherRepository::loadTeacherProfileRecords(
    const QList<int>& teacherIds
    )
{
    if (teacherIds.isEmpty())
    {
        return QList<TeacherProfileBatchReadRecord>{};
    }

    ++m_teacherProfileBatchReadMetrics.callCount;
    m_teacherProfileBatchReadMetrics.requestedTeacherCount +=
        teacherIds.size();

    QSet<int> seenTeacherIds;
    QStringList requestedValues;
    requestedValues.reserve(teacherIds.size());
    for (qsizetype index = 0; index < teacherIds.size(); ++index)
    {
        const int teacherId = teacherIds[index];
        if (teacherId <= 0 || seenTeacherIds.contains(teacherId))
        {
            return std::unexpected(QObject::tr(
                "Loading teacher profiles failed: teacher identifiers must be positive and unique."
                ));
        }

        seenTeacherIds.insert(teacherId);
        requestedValues.append(QStringLiteral("(?, %1)").arg(index));
    }

    QSqlQuery query(m_database);
    const QString queryText = QStringLiteral(R"(
        WITH requested(teacher_id, ordinal) AS (VALUES %1)
        SELECT requested.teacher_id AS requested_teacher_id,
               requested.ordinal AS requested_ordinal,
               teachers.id,
               teachers.teacher_kr,
               teachers.teacher_en,
               teachers.preferred_romanization,
               teachers.preferred_name,
               teachers.room_number,
               teachers.birthday,
               teachers.phone_number,
               teachers.wifi_name,
               teachers.wifi_password,
               teachers.internet_type,
               teachers.zoom_id,
               teachers.zoom_password,
               teachers.projection_type,
               teachers.notes
        FROM requested
        LEFT JOIN teachers ON teachers.id = requested.teacher_id
        ORDER BY requested.ordinal
    )").arg(requestedValues.join(QStringLiteral(", ")));
    if (!query.prepare(queryText))
    {
        ++m_teacherProfileBatchReadMetrics.statementCount;
        QList<TeacherProfileBatchReadRecord> entries;
        entries.reserve(teacherIds.size());
        for (const int teacherId : teacherIds)
        {
            ++m_teacherProfileBatchReadMetrics.fallbackSingleReadCount;
            entries.append({teacherId, getTeacher(teacherId)});
        }
        return entries;
    }

    for (const int teacherId : teacherIds)
    {
        query.addBindValue(teacherId);
    }

    ++m_teacherProfileBatchReadMetrics.statementCount;
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading teacher profiles"),
        QStringLiteral("assigned teachers")
        );
    if (!executed)
    {
        QList<TeacherProfileBatchReadRecord> entries;
        entries.reserve(teacherIds.size());
        for (const int teacherId : teacherIds)
        {
            ++m_teacherProfileBatchReadMetrics.fallbackSingleReadCount;
            entries.append({teacherId, getTeacher(teacherId)});
        }
        return entries;
    }

    QList<TeacherProfileBatchReadRecord> entries;
    entries.reserve(teacherIds.size());
    qsizetype rowIndex = 0;
    while (query.next())
    {
        if (rowIndex >= teacherIds.size())
        {
            return std::unexpected(QObject::tr(
                "Loading teacher profiles failed: the database returned an unexpected number of records."
                ));
        }

        const int requestedTeacherId = teacherIds[rowIndex];
        if (query.value("requested_teacher_id").toInt()
                != requestedTeacherId
            || query.value("requested_ordinal").toInt() != rowIndex)
        {
            return std::unexpected(QObject::tr(
                "Loading teacher profiles failed: the database returned records in a different identifier order."
                ));
        }

        if (query.value("id").isNull())
        {
            entries.append({
                requestedTeacherId,
                std::unexpected(QObject::tr(
                    "Loading teacher failed for %1: no matching record exists."
                    ).arg(teacherIdentity(requestedTeacherId)))
            });
            ++rowIndex;
            continue;
        }

        Teacher teacher = teacherFromQuery(query);
        if (teacher.id != requestedTeacherId)
        {
            return std::unexpected(QObject::tr(
                "Loading teacher profiles failed: a database record returned a different teacher identifier."
                ));
        }

        entries.append({requestedTeacherId, std::move(teacher)});
        ++rowIndex;
    }

    if (rowIndex != teacherIds.size())
    {
        return std::unexpected(QObject::tr(
            "Loading teacher profiles failed: the database returned an incomplete record list."
            ));
    }

    return entries;
}

namespace
{
TeacherProfileLookupResult lookupTeacherProfileRecord(
    QSqlDatabase& database,
    const qint64 teacherId,
    const QString& action
    )
{
    if (teacherId <= 0)
    {
        return {
            .code = TeacherProfileLookupCode::Failure,
            .diagnostic = QObject::tr(
                "Loading teacher profile failed: invalid teacher id %1."
                ).arg(teacherId)
        };
    }

    QSqlQuery query(database);
    query.setForwardOnly(true);
    query.prepare(R"(
        SELECT id,
               teacher_kr,
               teacher_en,
               preferred_romanization,
               preferred_name,
               room_number,
               birthday,
               phone_number,
               wifi_name,
               wifi_password,
               internet_type,
               zoom_id,
               zoom_password,
               projection_type,
               notes
        FROM teachers
        WHERE id=?
    )");
    query.addBindValue(teacherId);

    const QString identity = teacherIdentity(teacherId);
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        action,
        identity
        );
    if (!executed)
    {
        return {
            .code = TeacherProfileLookupCode::Failure,
            .diagnostic = executed.error().userMessage()
        };
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return {
                .code = TeacherProfileLookupCode::Failure,
                .diagnostic = SqlQueryUtils::errorFor(
                    query,
                    action,
                    {},
                    identity
                    ).userMessage()
            };
        }

        return {
            .code = TeacherProfileLookupCode::Missing,
            .diagnostic = QObject::tr(
                "Loading teacher failed for %1: no matching record exists."
                ).arg(identity)
        };
    }

    bool identifierValid = false;
    const qint64 returnedTeacherId = query.value(QStringLiteral("id"))
        .toLongLong(&identifierValid);
    if (!identifierValid || returnedTeacherId != teacherId)
        return {
            .code = TeacherProfileLookupCode::Failure,
            .diagnostic = QObject::tr(
                "Loading teacher profiles failed: a database record returned a different teacher identifier.")
        };

    return {
        .code = TeacherProfileLookupCode::Found,
        .profile = {
            .teacherId = returnedTeacherId,
            .teacherKr = query.value(QStringLiteral("teacher_kr")).toString(),
            .teacherEn = query.value(QStringLiteral("teacher_en")).toString(),
            .preferredRomanization = query.value(
                QStringLiteral("preferred_romanization")
                ).toString(),
            .preferredName = query.value(
                QStringLiteral("preferred_name")
                ).toString(),
            .roomNumber = query.value(QStringLiteral("room_number")).toString(),
            .birthday = query.value(QStringLiteral("birthday")).toString(),
            .phoneNumber = query.value(QStringLiteral("phone_number")).toString(),
            .wifiName = query.value(QStringLiteral("wifi_name")).toString(),
            .wifiPassword = query.value(QStringLiteral("wifi_password"))
                                .toString(),
            .internetType = normalizedInternetType(
                query.value(QStringLiteral("internet_type")).toString()
                ),
            .zoomId = query.value(QStringLiteral("zoom_id")).toString(),
            .zoomPassword = query.value(QStringLiteral("zoom_password"))
                                .toString(),
            .projectionType = normalizedProjectionType(
                query.value(QStringLiteral("projection_type")).toString()
                ),
            .notes = query.value(QStringLiteral("notes")).toString()
        }
    };
}

}

Result<QList<ClassTransferExportTeacherProfileBatchReadRecord>>
TeacherRepository::loadClassTransferExportTeacherProfileRecords(
    const QList<qint64>& teacherIds
    )
{
    if (teacherIds.isEmpty())
    {
        return QList<ClassTransferExportTeacherProfileBatchReadRecord>{};
    }

    ++m_teacherProfileBatchReadMetrics.callCount;
    m_teacherProfileBatchReadMetrics.requestedTeacherCount +=
        teacherIds.size();

    QSet<qint64> seenTeacherIds;
    QStringList requestedValues;
    requestedValues.reserve(teacherIds.size());
    for (qsizetype index = 0; index < teacherIds.size(); ++index)
    {
        const qint64 teacherId = teacherIds[index];
        if (teacherId <= 0 || seenTeacherIds.contains(teacherId))
        {
            return std::unexpected(QObject::tr(
                "Loading teacher profiles failed: teacher identifiers must be positive and unique."
                ));
        }

        seenTeacherIds.insert(teacherId);
        requestedValues.append(QStringLiteral("(?, %1)").arg(index));
    }

    const auto loadIndividually = [this, &teacherIds]()
    {
        QList<ClassTransferExportTeacherProfileBatchReadRecord> entries;
        entries.reserve(teacherIds.size());
        for (const qint64 teacherId : teacherIds)
        {
            ++m_teacherProfileBatchReadMetrics.fallbackSingleReadCount;
            auto lookup = lookupTeacherProfileRecord(
                m_database, teacherId, QObject::tr("Loading teacher"));
            if (lookup.code == TeacherProfileLookupCode::Found
                && lookup.profile.teacherId == teacherId)
                entries.append({teacherId, std::move(lookup.profile)});
            else
                entries.append({teacherId, std::unexpected(lookup.diagnostic.isEmpty()
                    ? QObject::tr("Loading teacher profiles failed: a database record returned a different teacher identifier.")
                    : lookup.diagnostic)});
        }
        return entries;
    };

    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    const QString queryText = QStringLiteral(R"(
        WITH requested(teacher_id, ordinal) AS (VALUES %1)
        SELECT requested.teacher_id AS requested_teacher_id,
               requested.ordinal AS requested_ordinal,
               teachers.id,
               teachers.teacher_kr,
               teachers.teacher_en,
               teachers.preferred_romanization,
               teachers.preferred_name,
               teachers.room_number,
               teachers.birthday,
               teachers.phone_number,
               teachers.wifi_name,
               teachers.wifi_password,
               teachers.internet_type,
               teachers.zoom_id,
               teachers.zoom_password,
               teachers.projection_type,
               teachers.notes
        FROM requested
        LEFT JOIN teachers ON teachers.id = requested.teacher_id
        ORDER BY requested.ordinal
    )").arg(requestedValues.join(QStringLiteral(", ")));
    if (!query.prepare(queryText))
    {
        ++m_teacherProfileBatchReadMetrics.statementCount;
        return loadIndividually();
    }

    for (const qint64 teacherId : teacherIds)
    {
        query.addBindValue(teacherId);
    }

    ++m_teacherProfileBatchReadMetrics.statementCount;
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading teacher profiles"),
        QStringLiteral("assigned teachers")
        );
    if (!executed)
    {
        return loadIndividually();
    }

    QList<ClassTransferExportTeacherProfileBatchReadRecord> entries;
    entries.reserve(teacherIds.size());
    qsizetype rowIndex = 0;
    while (query.next())
    {
        if (rowIndex >= teacherIds.size())
        {
            return std::unexpected(QObject::tr(
                "Loading teacher profiles failed: the database returned an unexpected number of records."
                ));
        }

        const qint64 requestedTeacherId = teacherIds[rowIndex];
        bool requestedIdValid = false;
        const qint64 returnedRequestedId = query.value("requested_teacher_id")
            .toLongLong(&requestedIdValid);
        bool ordinalValid = false;
        const int returnedOrdinal = query.value("requested_ordinal").toInt(&ordinalValid);
        if (!requestedIdValid || returnedRequestedId != requestedTeacherId
            || !ordinalValid || returnedOrdinal != rowIndex)
        {
            return std::unexpected(QObject::tr(
                "Loading teacher profiles failed: the database returned records in a different identifier order."
                ));
        }

        if (query.value("id").isNull())
        {
            entries.append({
                requestedTeacherId,
                std::unexpected(QObject::tr(
                    "Loading teacher failed for %1: no matching record exists."
                    ).arg(teacherIdentity(requestedTeacherId)))
            });
            ++rowIndex;
            continue;
        }

        bool teacherIdValid = false;
        const qint64 returnedTeacherId = query.value("id").toLongLong(&teacherIdValid);
        if (!teacherIdValid || returnedTeacherId != requestedTeacherId)
        {
            return std::unexpected(QObject::tr(
                "Loading teacher profiles failed: a database record returned a different teacher identifier."
                ));
        }

        TeacherProfileReadRecord profile{
            .teacherId = returnedTeacherId,
            .teacherKr = query.value(QStringLiteral("teacher_kr")).toString(),
            .teacherEn = query.value(QStringLiteral("teacher_en")).toString(),
            .preferredRomanization = query.value(
                QStringLiteral("preferred_romanization")
                ).toString(),
            .preferredName = query.value(
                QStringLiteral("preferred_name")
                ).toString(),
            .roomNumber = query.value(QStringLiteral("room_number")).toString(),
            .birthday = query.value(QStringLiteral("birthday")).toString(),
            .phoneNumber = query.value(QStringLiteral("phone_number")).toString(),
            .wifiName = query.value(QStringLiteral("wifi_name")).toString(),
            .wifiPassword = query.value(QStringLiteral("wifi_password"))
                                .toString(),
            .internetType = normalizedInternetType(
                query.value(QStringLiteral("internet_type")).toString()
                ),
            .zoomId = query.value(QStringLiteral("zoom_id")).toString(),
            .zoomPassword = query.value(QStringLiteral("zoom_password"))
                                .toString(),
            .projectionType = normalizedProjectionType(
                query.value(QStringLiteral("projection_type")).toString()
                ),
            .notes = query.value(QStringLiteral("notes")).toString()
        };
        entries.append({requestedTeacherId, std::move(profile)});
        ++rowIndex;
    }

    if (query.lastError().isValid())
        return std::unexpected(query.lastError().text());
    if (rowIndex != teacherIds.size())
    {
        return std::unexpected(QObject::tr(
            "Loading teacher profiles failed: the database returned an incomplete record list."
            ));
    }

    return entries;
}

const TeacherProfileBatchReadMetrics&
TeacherRepository::teacherProfileBatchReadMetrics() const noexcept
{
    return m_teacherProfileBatchReadMetrics;
}

TeacherProfileLookupResult TeacherRepository::lookupTeacherProfileById(
    const qint64 teacherId)
{
    return lookupTeacherProfileRecord(
        m_database, teacherId, QObject::tr("Loading teacher profile"));
}

Result<QList<SubPrepPrintSourceTeacherProfileBatchReadRecord>>
TeacherRepository::loadSubPrepPrintSourceTeacherProfileRecords(
    const QList<qint64>& teacherIds
    )
{
    if (teacherIds.isEmpty())
    {
        return QList<SubPrepPrintSourceTeacherProfileBatchReadRecord>{};
    }

    ++m_subPrepPrintSourceTeacherProfileBatchReadMetrics.callCount;
    m_subPrepPrintSourceTeacherProfileBatchReadMetrics.requestedTeacherCount +=
        teacherIds.size();

    QSet<qint64> seenTeacherIds;
    QStringList requestedValues;
    requestedValues.reserve(teacherIds.size());
    for (qsizetype index = 0; index < teacherIds.size(); ++index)
    {
        const qint64 teacherId = teacherIds[index];
        if (teacherId <= 0 || seenTeacherIds.contains(teacherId))
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep print source teacher profiles failed: teacher identifiers must be positive and unique."
                ));
        }

        seenTeacherIds.insert(teacherId);
        requestedValues.append(QStringLiteral("(?, %1)").arg(index));
    }

    const auto loadIndividually = [this, &teacherIds]()
    {
        QList<SubPrepPrintSourceTeacherProfileBatchReadRecord> entries;
        entries.reserve(teacherIds.size());
        for (const qint64 teacherId : teacherIds)
        {
            ++m_subPrepPrintSourceTeacherProfileBatchReadMetrics
                  .fallbackSingleReadCount;
            entries.append({
                teacherId,
                loadSubPrepPrintSourceTeacherProfileRecord(teacherId)
            });
        }
        return entries;
    };

    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    const QString queryText = QStringLiteral(R"(
        WITH requested(teacher_id, ordinal) AS (VALUES %1)
        SELECT requested.teacher_id AS requested_teacher_id,
               requested.ordinal AS requested_ordinal,
               teachers.id AS teacher_id,
               teachers.teacher_kr,
               teachers.teacher_en,
               teachers.preferred_romanization,
               teachers.preferred_name,
               teachers.room_number,
               teachers.wifi_name,
               teachers.wifi_password,
               teachers.internet_type,
               teachers.zoom_id,
               teachers.zoom_password,
               teachers.projection_type,
               teachers.notes
        FROM requested
        LEFT JOIN teachers ON teachers.id = requested.teacher_id
        ORDER BY requested.ordinal
    )").arg(requestedValues.join(QStringLiteral(", ")));
    if (!query.prepare(queryText))
    {
        ++m_subPrepPrintSourceTeacherProfileBatchReadMetrics.statementCount;
        return loadIndividually();
    }

    for (const qint64 teacherId : teacherIds)
    {
        query.addBindValue(teacherId);
    }

    ++m_subPrepPrintSourceTeacherProfileBatchReadMetrics.statementCount;
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading Sub Prep print source teacher profiles"),
        QStringLiteral("assigned teachers")
        );
    if (!executed)
    {
        return loadIndividually();
    }

    QList<SubPrepPrintSourceTeacherProfileBatchReadRecord> entries;
    entries.reserve(teacherIds.size());
    qsizetype rowIndex = 0;
    while (query.next())
    {
        if (rowIndex >= teacherIds.size())
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep print source teacher profiles failed: the database returned an unexpected number of records."
                ));
        }

        const qint64 requestedTeacherId = teacherIds[rowIndex];
        bool returnedRequestedTeacherIdValid = false;
        const qint64 returnedRequestedTeacherId = query.value(
            QStringLiteral("requested_teacher_id")
            ).toLongLong(&returnedRequestedTeacherIdValid);
        bool returnedOrdinalValid = false;
        const int returnedOrdinal = query.value(
            QStringLiteral("requested_ordinal")
            ).toInt(&returnedOrdinalValid);
        if (!returnedRequestedTeacherIdValid
            || returnedRequestedTeacherId != requestedTeacherId
            || !returnedOrdinalValid
            || returnedOrdinal != rowIndex)
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep print source teacher profiles failed: the database returned records in a different identifier order."
                ));
        }

        if (query.value(QStringLiteral("teacher_id")).isNull())
        {
            entries.append({
                requestedTeacherId,
                std::unexpected(QObject::tr(
                    "Loading teacher failed for %1: no matching record exists."
                    ).arg(teacherIdentity(requestedTeacherId)))
            });
            ++rowIndex;
            continue;
        }

        bool returnedTeacherIdValid = false;
        const qint64 returnedTeacherId = query.value(
            QStringLiteral("teacher_id")
            ).toLongLong(&returnedTeacherIdValid);
        if (!returnedTeacherIdValid
            || returnedTeacherId != requestedTeacherId)
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep print source teacher profiles failed: the database returned a different teacher identifier."
                ));
        }

        entries.append({
            requestedTeacherId,
            SubPrepPrintSourceTeacherProfileReadFields{
                .teacherId = returnedTeacherId,
                .teacherKr = query.value(QStringLiteral("teacher_kr"))
                                 .toString(),
                .teacherEn = query.value(QStringLiteral("teacher_en"))
                                 .toString(),
                .preferredRomanization = query.value(
                    QStringLiteral("preferred_romanization")
                    ).toString(),
                .preferredName = query.value(QStringLiteral("preferred_name"))
                                     .toString(),
                .roomNumber = query.value(QStringLiteral("room_number"))
                                  .toString(),
                .wifiName = query.value(QStringLiteral("wifi_name"))
                                .toString(),
                .wifiPassword = query.value(QStringLiteral("wifi_password"))
                                    .toString(),
                .internetType = normalizedInternetType(
                    query.value(QStringLiteral("internet_type")).toString()
                    ),
                .zoomId = query.value(QStringLiteral("zoom_id")).toString(),
                .zoomPassword = query.value(
                    QStringLiteral("zoom_password")
                    ).toString(),
                .projectionType = normalizedProjectionType(
                    query.value(QStringLiteral("projection_type")).toString()
                    ),
                .notes = query.value(QStringLiteral("notes")).toString()
            }
        });
        ++rowIndex;
    }

    if (rowIndex != teacherIds.size())
    {
        return std::unexpected(QObject::tr(
            "Loading Sub Prep print source teacher profiles failed: the database returned an incomplete record list."
            ));
    }

    return entries;
}

const SubPrepPrintSourceTeacherProfileBatchReadMetrics&
TeacherRepository::subPrepPrintSourceTeacherProfileBatchReadMetrics() const noexcept
{
    return m_subPrepPrintSourceTeacherProfileBatchReadMetrics;
}

Result<SubPrepPrintSourceTeacherProfileReadFields>
TeacherRepository::loadSubPrepPrintSourceTeacherProfileRecord(
    const qint64 teacherId
    )
{
    if (teacherId <= 0)
    {
        return std::unexpected(QObject::tr(
            "Loading teacher failed: invalid %1."
            ).arg(teacherIdentity(teacherId)));
    }

    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    if (!query.prepare(QStringLiteral(R"(
        SELECT id AS teacher_id,
               teacher_kr,
               teacher_en,
               preferred_romanization,
               preferred_name,
               room_number,
               wifi_name,
               wifi_password,
               internet_type,
               zoom_id,
               zoom_password,
               projection_type,
               notes
        FROM teachers
        WHERE id = ?
    )")))
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading Sub Prep print source teacher profile"),
            {},
            teacherIdentity(teacherId)
            ).userMessage());
    }

    query.addBindValue(teacherId);
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading Sub Prep print source teacher profile"),
        teacherIdentity(teacherId)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    if (!query.next())
    {
        return std::unexpected(QObject::tr(
            "Loading teacher failed for %1: no matching record exists."
            ).arg(teacherIdentity(teacherId)));
    }

    bool returnedTeacherIdValid = false;
    const qint64 returnedTeacherId = query.value(
        QStringLiteral("teacher_id")
        ).toLongLong(&returnedTeacherIdValid);
    if (!returnedTeacherIdValid || returnedTeacherId != teacherId)
    {
        return std::unexpected(QObject::tr(
            "Loading Sub Prep print source teacher profile failed for %1: the database returned a different teacher identifier."
            ).arg(teacherIdentity(teacherId)));
    }

    SubPrepPrintSourceTeacherProfileReadFields profile{
        .teacherId = returnedTeacherId,
        .teacherKr = query.value(QStringLiteral("teacher_kr")).toString(),
        .teacherEn = query.value(QStringLiteral("teacher_en")).toString(),
        .preferredRomanization = query.value(
            QStringLiteral("preferred_romanization")
            ).toString(),
        .preferredName = query.value(QStringLiteral("preferred_name"))
                             .toString(),
        .roomNumber = query.value(QStringLiteral("room_number")).toString(),
        .wifiName = query.value(QStringLiteral("wifi_name")).toString(),
        .wifiPassword = query.value(QStringLiteral("wifi_password"))
                            .toString(),
        .internetType = normalizedInternetType(
            query.value(QStringLiteral("internet_type")).toString()
            ),
        .zoomId = query.value(QStringLiteral("zoom_id")).toString(),
        .zoomPassword = query.value(QStringLiteral("zoom_password"))
                            .toString(),
        .projectionType = normalizedProjectionType(
            query.value(QStringLiteral("projection_type")).toString()
            ),
        .notes = query.value(QStringLiteral("notes")).toString()
    };

    if (query.next())
    {
        return std::unexpected(QObject::tr(
            "Loading Sub Prep print source teacher profile failed for %1: the database returned duplicate records."
            ).arg(teacherIdentity(teacherId)));
    }

    return profile;
}

Result<QList<SubPrepRosterOutputTeacherProfileBatchReadRecord>>
TeacherRepository::loadSubPrepRosterOutputTeacherProfileRecords(
    const QList<qint64>& teacherIds
    )
{
    if (teacherIds.isEmpty())
    {
        return QList<SubPrepRosterOutputTeacherProfileBatchReadRecord>{};
    }

    ++m_subPrepRosterOutputTeacherProfileBatchReadMetrics.callCount;
    m_subPrepRosterOutputTeacherProfileBatchReadMetrics.requestedTeacherCount +=
        teacherIds.size();

    QSet<qint64> seenTeacherIds;
    QStringList requestedValues;
    requestedValues.reserve(teacherIds.size());
    for (qsizetype index = 0; index < teacherIds.size(); ++index)
    {
        const qint64 teacherId = teacherIds[index];
        if (teacherId <= 0 || seenTeacherIds.contains(teacherId))
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep roster-output teacher profiles failed: "
                "teacher identifiers must be positive and unique."
                ));
        }

        seenTeacherIds.insert(teacherId);
        requestedValues.append(QStringLiteral("(?, %1)").arg(index));
    }

    QSqlQuery query(m_database);
    const QString queryText = QStringLiteral(R"(
        WITH requested(teacher_id, ordinal) AS (VALUES %1)
        SELECT requested.teacher_id AS requested_teacher_id,
               requested.ordinal AS requested_ordinal,
               teachers.id,
               teachers.teacher_kr,
               teachers.teacher_en,
               teachers.preferred_romanization,
               teachers.preferred_name
        FROM requested
        LEFT JOIN teachers ON teachers.id = requested.teacher_id
        ORDER BY requested.ordinal
    )").arg(requestedValues.join(QStringLiteral(", ")));
    if (!query.prepare(queryText))
    {
        ++m_subPrepRosterOutputTeacherProfileBatchReadMetrics.statementCount;
        return std::unexpected(query.lastError().text());
    }

    for (const qint64 teacherId : teacherIds)
    {
        query.addBindValue(teacherId);
    }

    ++m_subPrepRosterOutputTeacherProfileBatchReadMetrics.statementCount;
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading Sub Prep roster-output teacher profiles"),
        QStringLiteral("assigned teachers")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    QList<SubPrepRosterOutputTeacherProfileBatchReadRecord> entries;
    entries.reserve(teacherIds.size());
    qsizetype rowIndex = 0;
    while (query.next())
    {
        if (rowIndex >= teacherIds.size())
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep roster-output teacher profiles failed: "
                "the database returned an unexpected number of records."
                ));
        }

        const qint64 requestedTeacherId = teacherIds[rowIndex];
        bool returnedRequestedTeacherIdOk = false;
        const qint64 returnedRequestedTeacherId = query.value(
            "requested_teacher_id"
            ).toLongLong(&returnedRequestedTeacherIdOk);
        if (!returnedRequestedTeacherIdOk
            || returnedRequestedTeacherId != requestedTeacherId
            || query.value("requested_ordinal").toInt() != rowIndex)
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep roster-output teacher profiles failed: "
                "the database returned records in a different identifier order."
                ));
        }

        if (query.value("id").isNull())
        {
            entries.append({
                requestedTeacherId,
                std::unexpected(QObject::tr(
                    "Loading teacher failed for %1: no matching record exists."
                    ).arg(teacherIdentity(requestedTeacherId)))
            });
            ++rowIndex;
            continue;
        }

        bool returnedTeacherIdOk = false;
        const qint64 returnedTeacherId = query.value("id").toLongLong(
            &returnedTeacherIdOk
            );
        if (!returnedTeacherIdOk
            || returnedTeacherId <= 0
            || returnedTeacherId != requestedTeacherId)
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep roster-output teacher failed for %1: "
                "the database returned a different teacher identifier."
                ).arg(teacherIdentity(requestedTeacherId)));
        }

        entries.append({
            requestedTeacherId,
            SubPrepRosterOutputTeacherProfileReadFields{
                .teacherId = returnedTeacherId,
                .teacherKr = query.value("teacher_kr").toString(),
                .teacherEn = query.value("teacher_en").toString(),
                .preferredRomanization = query.value(
                    "preferred_romanization").toString(),
                .preferredName = query.value("preferred_name").toString()
            }
        });
        ++rowIndex;
    }

    if (rowIndex != teacherIds.size())
    {
        return std::unexpected(QObject::tr(
            "Loading Sub Prep roster-output teacher profiles failed: "
            "the database returned an incomplete record list."
            ));
    }

    return entries;
}

const SubPrepRosterOutputTeacherProfileBatchReadMetrics&
TeacherRepository::subPrepRosterOutputTeacherProfileBatchReadMetrics() const noexcept
{
    return m_subPrepRosterOutputTeacherProfileBatchReadMetrics;
}

Result<QList<MyClassesTeacherProfileBatchReadRecord>>
TeacherRepository::loadMyClassesTeacherProfileRecords(
    const QList<qint64>& teacherIds
    )
{
    if (teacherIds.isEmpty())
    {
        return QList<MyClassesTeacherProfileBatchReadRecord>{};
    }

    ++m_myClassesTeacherProfileBatchReadMetrics.callCount;
    m_myClassesTeacherProfileBatchReadMetrics.requestedTeacherCount +=
        teacherIds.size();

    QSet<qint64> seenTeacherIds;
    QStringList requestedValues;
    requestedValues.reserve(teacherIds.size());
    for (qsizetype index = 0; index < teacherIds.size(); ++index)
    {
        const qint64 teacherId = teacherIds[index];
        if (teacherId <= 0 || seenTeacherIds.contains(teacherId))
        {
            return std::unexpected(QObject::tr(
                "Loading My Classes teacher profiles failed: teacher identifiers must be positive and unique."
                ));
        }

        seenTeacherIds.insert(teacherId);
        requestedValues.append(QStringLiteral("(?, %1)").arg(index));
    }

    QSqlQuery query(m_database);
    const QString queryText = QStringLiteral(R"(
        WITH requested(teacher_id, ordinal) AS (VALUES %1)
        SELECT requested.teacher_id AS requested_teacher_id,
               requested.ordinal AS requested_ordinal,
               teachers.id,
               teachers.teacher_kr,
               teachers.teacher_en,
               teachers.preferred_romanization,
               teachers.preferred_name,
               teachers.room_number,
               teachers.wifi_name,
               teachers.wifi_password,
               teachers.internet_type,
               teachers.zoom_id,
               teachers.zoom_password,
               teachers.projection_type,
               teachers.notes
        FROM requested
        LEFT JOIN teachers ON teachers.id = requested.teacher_id
        ORDER BY requested.ordinal
    )").arg(requestedValues.join(QStringLiteral(", ")));
    if (!query.prepare(queryText))
    {
        ++m_myClassesTeacherProfileBatchReadMetrics.statementCount;
        return std::unexpected(query.lastError().text());
    }

    for (const qint64 teacherId : teacherIds)
    {
        query.addBindValue(teacherId);
    }

    ++m_myClassesTeacherProfileBatchReadMetrics.statementCount;
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading My Classes teacher profiles"),
        QStringLiteral("assigned teachers")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    QList<MyClassesTeacherProfileBatchReadRecord> entries;
    entries.reserve(teacherIds.size());
    qsizetype rowIndex = 0;
    while (query.next())
    {
        if (rowIndex >= teacherIds.size())
        {
            entries.append({
                .requestedTeacherId = -1,
                .code = MyClassesTeacherProfileReadCode::Invalid,
                .diagnostic = QObject::tr(
                    "Loading My Classes teacher profiles failed: the database returned an unexpected number of records."
                    )
            });
            ++rowIndex;
            continue;
        }

        const qint64 requestedTeacherId = teacherIds[rowIndex];
        bool returnedRequestedTeacherIdValid = false;
        const qint64 returnedRequestedTeacherId = query.value(
            "requested_teacher_id"
            ).toLongLong(&returnedRequestedTeacherIdValid);
        bool returnedOrdinalValid = false;
        const int returnedOrdinal = query.value("requested_ordinal").toInt(
            &returnedOrdinalValid
            );
        if (!returnedRequestedTeacherIdValid
            || returnedRequestedTeacherId != requestedTeacherId
            || !returnedOrdinalValid
            || returnedOrdinal != rowIndex)
        {
            entries.append({
                .requestedTeacherId = returnedRequestedTeacherIdValid
                    ? returnedRequestedTeacherId
                    : -1,
                .code = MyClassesTeacherProfileReadCode::Invalid,
                .diagnostic = QObject::tr(
                    "Loading My Classes teacher profiles failed: the database returned records in a different identifier order."
                    )
            });
            ++rowIndex;
            continue;
        }

        if (query.value("id").isNull())
        {
            entries.append({
                .requestedTeacherId = requestedTeacherId,
                .code = MyClassesTeacherProfileReadCode::Missing,
                .diagnostic = QObject::tr(
                    "Loading teacher failed for %1: no matching record exists."
                    ).arg(teacherIdentity(requestedTeacherId))
            });
            ++rowIndex;
            continue;
        }

        bool returnedTeacherIdValid = false;
        const qint64 returnedTeacherId = query.value("id").toLongLong(
            &returnedTeacherIdValid
            );
        if (!returnedTeacherIdValid
            || returnedTeacherId != requestedTeacherId)
        {
            entries.append({
                .requestedTeacherId = requestedTeacherId,
                .code = MyClassesTeacherProfileReadCode::Invalid,
                .diagnostic = QObject::tr(
                    "Loading My Classes teacher failed for %1: the database returned a different teacher identifier."
                    ).arg(teacherIdentity(requestedTeacherId))
            });
            ++rowIndex;
            continue;
        }

        entries.append({
            .requestedTeacherId = requestedTeacherId,
            .code = MyClassesTeacherProfileReadCode::Found,
            .profile = MyClassesTeacherProfileReadFields{
                .teacherId = returnedTeacherId,
                .teacherKr = query.value("teacher_kr").toString(),
                .teacherEn = query.value("teacher_en").toString(),
                .preferredRomanization = query.value(
                    "preferred_romanization").toString(),
                .preferredName = query.value("preferred_name").toString(),
                .roomNumber = query.value("room_number").toString(),
                .wifiName = query.value("wifi_name").toString(),
                .wifiPassword = query.value("wifi_password").toString(),
                .internetType = normalizedInternetType(
                    query.value("internet_type").toString()),
                .zoomId = query.value("zoom_id").toString(),
                .zoomPassword = query.value("zoom_password").toString(),
                .projectionType = normalizedProjectionType(
                    query.value("projection_type").toString()),
                .notes = query.value("notes").toString()
            }
        });
        ++rowIndex;
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading My Classes teacher profiles"),
            {},
            QStringLiteral("assigned teachers")
            ).userMessage());
    }

    return entries;
}

const MyClassesTeacherProfileBatchReadMetrics&
TeacherRepository::myClassesTeacherProfileBatchReadMetrics() const noexcept
{
    return m_myClassesTeacherProfileBatchReadMetrics;
}

Result<QList<Teacher>> TeacherRepository::getAllTeachers()
{
    QList<Teacher> teachers;

    QSqlQuery query(m_database);

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
        SELECT *
        FROM teachers
        ORDER BY teacher_en
    )"),
        QObject::tr("Loading teachers")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        bool teacherIdOk = false;
        const qint64 storedTeacherId =
            query.value(QStringLiteral("id")).toLongLong(&teacherIdOk);
        if (
            !teacherIdOk
            || storedTeacherId <= 0
            || storedTeacherId > std::numeric_limits<int>::max()
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Loading teachers found an invalid teacher identifier %1."
                    ).arg(query.value(QStringLiteral("id")).toString())
                );
        }

        Teacher teacher = teacherFromQuery(query);
        teacher.id = static_cast<int>(storedTeacherId);
        teachers.append(std::move(teacher));
    }

    return teachers;
}

Result<QList<ScheduleImportTeacherReadRecord>>
TeacherRepository::loadScheduleImportTeacherRecords()
{
    ++m_scheduleImportTeacherReadMetrics.callCount;

    QList<ScheduleImportTeacherReadRecord> teachers;
    QSqlQuery query(m_database);
    ++m_scheduleImportTeacherReadMetrics.statementCount;

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            SELECT id, teacher_kr, room_number
            FROM teachers
            ORDER BY teacher_en
        )"),
        QObject::tr("Loading schedule import teachers")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        bool teacherIdOk = false;
        const qint64 teacherId = query.value(QStringLiteral("id"))
            .toLongLong(&teacherIdOk);
        if (!teacherIdOk)
        {
            return std::unexpected(
                QObject::tr("A Korean teacher ID could not be read.")
                );
        }
        teachers.append({
            teacherId,
            query.value(QStringLiteral("teacher_kr")).toString(),
            query.value(QStringLiteral("room_number")).toString()
        });
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading schedule import teachers")
            ).userMessage());
    }

    return teachers;
}

const ScheduleImportTeacherReadMetrics&
TeacherRepository::scheduleImportTeacherReadMetrics() const noexcept
{
    return m_scheduleImportTeacherReadMetrics;
}

Result<QList<InitialSetupTeacherChoiceReadRecord>>
TeacherRepository::loadInitialSetupTeacherChoiceRecords()
{
    QList<InitialSetupTeacherChoiceReadRecord> teachers;

    QSqlQuery query(m_database);
    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            SELECT
                id,
                teacher_kr,
                teacher_en,
                preferred_romanization,
                preferred_name
            FROM teachers
            ORDER BY teacher_en
        )"),
        QObject::tr("Loading teachers")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        teachers.append({
            query.value(QStringLiteral("id")).toLongLong(),
            query.value(QStringLiteral("teacher_kr")).toString(),
            query.value(QStringLiteral("teacher_en")).toString(),
            query.value(QStringLiteral("preferred_romanization")).toString(),
            query.value(QStringLiteral("preferred_name")).toString()
        });
    }

    return teachers;
}

Result<QList<TestingTeacherChoiceReadRecord>>
TeacherRepository::loadTestingTeacherChoiceRecords()
{
    QList<TestingTeacherChoiceReadRecord> teachers;

    QSqlQuery query(m_database);
    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            SELECT
                id,
                teacher_kr,
                room_number
            FROM teachers
            ORDER BY teacher_en
        )"),
        QObject::tr("Loading teachers")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        bool teacherIdOk = false;
        const qint64 teacherId = query.value(QStringLiteral("id"))
            .toLongLong(&teacherIdOk);
        if (!teacherIdOk)
        {
            return std::unexpected(
                QObject::tr("A teacher has an invalid identifier.")
                );
        }

        teachers.append({
            teacherId,
            query.value(QStringLiteral("teacher_kr")).toString(),
            query.value(QStringLiteral("room_number")).toString()
        });
    }

    return teachers;
}

Result<QList<ClassCoTeacherTeacherChoiceReadRecord>>
TeacherRepository::loadClassCoTeacherTeacherChoiceRecords()
{
    QList<ClassCoTeacherTeacherChoiceReadRecord> teachers;

    QSqlQuery query(m_database);
    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            SELECT
                id,
                teacher_kr,
                teacher_en,
                room_number,
                internet_type,
                wifi_name,
                wifi_password,
                projection_type,
                zoom_id,
                zoom_password
            FROM teachers
            ORDER BY teacher_en
        )"),
        QObject::tr("Loading teachers")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        bool teacherIdOk = false;
        qint64 teacherId = query.value(QStringLiteral("id"))
            .toLongLong(&teacherIdOk);
        if (!teacherIdOk || teacherId <= 0)
        {
            // Preserve invalid IDs for the application adapter to classify
            // as a nonrecoverable validation failure.
            teacherId = 0;
        }

        teachers.append({
            teacherId,
            query.value(QStringLiteral("teacher_kr")).toString(),
            query.value(QStringLiteral("teacher_en")).toString(),
            query.value(QStringLiteral("room_number")).toString(),
            query.value(QStringLiteral("internet_type")).toString(),
            query.value(QStringLiteral("wifi_name")).toString(),
            query.value(QStringLiteral("wifi_password")).toString(),
            query.value(QStringLiteral("projection_type")).toString(),
            query.value(QStringLiteral("zoom_id")).toString(),
            query.value(QStringLiteral("zoom_password")).toString()
        });
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(query.lastError().text());
    }

    return teachers;
}

Result<QList<KoreanTeacherBirthdayDirectoryReadRecord>>
TeacherRepository::loadKoreanTeacherBirthdayDirectoryRecords()
{
    QList<KoreanTeacherBirthdayDirectoryReadRecord> teachers;

    QSqlQuery query(m_database);
    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            SELECT
                birthday,
                teacher_kr,
                teacher_en,
                preferred_romanization,
                preferred_name
            FROM teachers
            ORDER BY teacher_en
        )"),
        QObject::tr("Loading teachers")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        teachers.append({
            query.value(QStringLiteral("birthday")).toString(),
            query.value(QStringLiteral("teacher_kr")).toString(),
            query.value(QStringLiteral("teacher_en")).toString(),
            query.value(QStringLiteral("preferred_romanization")).toString(),
            query.value(QStringLiteral("preferred_name")).toString()
        });
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(query.lastError().text());
    }

    return teachers;
}

Status TeacherRepository::deleteTeacher(
    int teacherId
    )
{
    if (teacherId <= 0)
    {
        return std::unexpected(
            QObject::tr("Deleting teacher failed: invalid teacher id %1.")
                .arg(teacherId)
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Starting teacher deletion transaction failed for %1: %2")
                .arg(teacherIdentity(teacherId), m_database.lastError().text())
            );
    }

    const QString identity = teacherIdentity(teacherId);
    QSqlQuery query(m_database);

    query.prepare(R"(
        UPDATE class_info
        SET teacher_id=NULL
        WHERE teacher_id=?
    )");

    query.addBindValue(teacherId);

    Status status = statusFromExecution(
        SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Removing teacher from classes"),
            identity
            )
        );
    if (!status)
    {
        return status;
    }

    query.prepare(R"(
        DELETE FROM teachers
        WHERE id=?
    )");

    query.addBindValue(teacherId);

    status = statusFromExecution(
        SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Deleting teacher"),
            identity
            )
        );
    if (!status)
    {
        return status;
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing teacher deletion failed for %1: %2")
                .arg(identity, m_database.lastError().text())
            );
    }

    return {};
}
