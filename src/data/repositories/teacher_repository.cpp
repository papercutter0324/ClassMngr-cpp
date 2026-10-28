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

std::unexpected<RepositoryError> repositoryUnexpected(
    QString diagnostic,
    const RepositoryErrorCode code = RepositoryErrorCode::Technical
    )
{
    return std::unexpected(RepositoryError{code, std::move(diagnostic)});
}

std::unexpected<RepositoryError> repositoryUnexpected(
    const RepositoryError& error
    )
{
    return std::unexpected(error);
}

RepositoryStatus statusFromExecution(
    const SqlQueryUtils::ExecutionResult& result
    )
{
    if (!result)
    {
        return repositoryUnexpected(result.error().userMessage());
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

RepositoryResult<int> TeacherRepository::createTeacher(
    const Teacher& teacher
    )
{
    const RepositoryResult<qint64> created = createTeacherWithWideId(teacher);
    if (!created)
    {
        return repositoryUnexpected(created.error());
    }
    if (*created > std::numeric_limits<int>::max())
    {
        return repositoryUnexpected(
            QObject::tr(
                "Creating teacher failed: generated record id %1 exceeds "
                "the supported range for this caller."
                ).arg(QString::number(*created)),
            RepositoryErrorCode::Validation
            );
    }

    return static_cast<int>(*created);
}

RepositoryResult<qint64> TeacherRepository::createTeacherWithWideId(
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
        return repositoryUnexpected(executed.error().userMessage());
    }

    bool idOk = false;
    const qint64 teacherId = query.lastInsertId().toLongLong(&idOk);
    if (!idOk || teacherId <= 0)
    {
        return repositoryUnexpected(
            QObject::tr(
                "Creating %1 failed: the database did not return a valid "
                "record id."
                ).arg(identity),
            RepositoryErrorCode::Validation
            );
    }

    return teacherId;
}

RepositoryResult<int> TeacherRepository::saveTeacher(
    const Teacher& teacher
    )
{
    if (teacher.id > 0)
    {
        const RepositoryStatus updated = updateTeacher(teacher);
        if (!updated)
        {
            return repositoryUnexpected(updated.error());
        }

        return teacher.id;
    }

    return createTeacher(teacher);
}

RepositoryStatus TeacherRepository::updateTeacher(
    const Teacher& teacher
    )
{
    return updateTeacherWithOutcome(teacher);
}

RepositoryStatus TeacherRepository::updateTeacherWithOutcome(
    const Teacher& teacher
    )
{
    return updateTeacherWithOutcome(teacher, static_cast<qint64>(teacher.id));
}

RepositoryStatus TeacherRepository::updateTeacherWithOutcome(
    const Teacher& teacher,
    const qint64 teacherId
    )
{
    if (teacherId <= 0)
    {
        return repositoryUnexpected(
            QObject::tr("Updating teacher failed: invalid teacher id %1.")
                .arg(teacherId),
            RepositoryErrorCode::InvalidInput
            );
    }

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
    query.addBindValue(teacherId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Updating teacher"),
        teacherIdentity(teacherId)
        );
    if (!executed)
    {
        return repositoryUnexpected(executed.error().userMessage());
    }

    if (query.numRowsAffected() == 0)
    {
        return repositoryUnexpected(
            QObject::tr(
                "Updating teacher failed for %1: no matching record exists."
                ).arg(teacherIdentity(teacherId)),
            RepositoryErrorCode::NotFound
            );
    }

    return {};
}

RepositoryResult<Teacher> TeacherRepository::getTeacher(
    int teacherId
    )
{
    return lookupTeacherById(teacherId);
}

RepositoryResult<Teacher> TeacherRepository::lookupTeacherById(
    int teacherId
    )
{
    if (teacherId <= 0)
    {
        return repositoryUnexpected(
            QObject::tr(
                "Loading teacher failed: invalid teacher id %1."
                ).arg(teacherId),
            RepositoryErrorCode::InvalidInput
            );
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
        return repositoryUnexpected(executed.error().userMessage());
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return repositoryUnexpected(SqlQueryUtils::errorFor(
                    query,
                    QObject::tr("Loading teacher"),
                    {},
                    teacherIdentity(teacherId)
                    ).userMessage());
        }

        return repositoryUnexpected(
            QObject::tr(
                "Loading teacher failed for %1: no matching record exists."
                ).arg(teacherIdentity(teacherId)),
            RepositoryErrorCode::NotFound
            );
    }

    bool returnedTeacherIdOk = false;
    const qint64 returnedTeacherId = query.value(QStringLiteral("id"))
        .toLongLong(&returnedTeacherIdOk);
    if (!returnedTeacherIdOk || returnedTeacherId != teacherId)
    {
        return repositoryUnexpected(
            QObject::tr(
                "Loading teacher failed for %1: the database returned a different teacher identifier."
                ).arg(teacherIdentity(teacherId)),
            RepositoryErrorCode::Validation
            );
    }
    if (query.lastError().isValid())
    {
        return repositoryUnexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading teacher"),
            {},
            teacherIdentity(teacherId)
            ).userMessage());
    }

    return teacherFromQuery(query);
}

RepositoryResult<TeacherDisplayNameReadRecord>
TeacherRepository::loadTeacherDisplayNameFields(
    int teacherId
    )
{
    return loadTeacherDisplayNameFields(static_cast<qint64>(teacherId));
}

RepositoryResult<TeacherDisplayNameReadRecord>
TeacherRepository::loadTeacherDisplayNameFields(
    qint64 teacherId
    )
{
    return lookupTeacherDisplayName(teacherId);
}

RepositoryResult<TeacherDisplayNameReadRecord>
TeacherRepository::lookupTeacherDisplayName(
    int teacherId
    )
{
    return lookupTeacherDisplayName(static_cast<qint64>(teacherId));
}

RepositoryResult<TeacherDisplayNameReadRecord>
TeacherRepository::lookupTeacherDisplayName(
    qint64 teacherId
    )
{
    if (teacherId <= 0)
    {
        return repositoryUnexpected(
            QObject::tr(
                "Loading teacher display name failed: invalid teacher id %1."
                ).arg(teacherId),
            RepositoryErrorCode::InvalidInput
            );
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
        return repositoryUnexpected(executed.error().userMessage());
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return repositoryUnexpected(SqlQueryUtils::errorFor(
                    query,
                    QObject::tr("Loading teacher display name"),
                    {},
                    teacherIdentity(teacherId)
                    ).userMessage());
        }

        return repositoryUnexpected(
            QObject::tr(
                "Loading teacher display name failed for %1: no matching record exists."
                ).arg(teacherIdentity(teacherId)),
            RepositoryErrorCode::NotFound
            );
    }

    if (query.lastError().isValid())
    {
        return repositoryUnexpected(SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading teacher display name"),
                {},
                teacherIdentity(teacherId)
                ).userMessage());
    }

    bool returnedTeacherIdOk = false;
    const qint64 returnedTeacherId = query.value("teacher_id").toLongLong(
        &returnedTeacherIdOk
        );
    if (!returnedTeacherIdOk || returnedTeacherId != teacherId)
    {
        return repositoryUnexpected(
            QObject::tr(
                "Loading teacher display name failed for %1: the database returned an invalid teacher identifier."
                ).arg(teacherIdentity(teacherId)),
            RepositoryErrorCode::Validation
            );
    }

    return TeacherDisplayNameReadRecord{
            query.value("teacher_kr").toString(),
            query.value("teacher_en").toString(),
            query.value("preferred_romanization").toString(),
            query.value("preferred_name").toString()
    };
}

RepositoryResult<QList<TeacherDisplayNameBatchReadRecord>>
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
        return repositoryUnexpected(loaded.error());
    }

    QList<TeacherDisplayNameBatchReadRecord> records;
    records.reserve(loaded->size());
    for (const MyClassesTeacherDisplayNameBatchReadRecord& record : *loaded)
    {
        if (record.teacherId < std::numeric_limits<int>::min()
            || record.teacherId > std::numeric_limits<int>::max())
        {
            return repositoryUnexpected(QObject::tr(
                "Loading teacher display names failed: the database returned a teacher identifier outside the supported range."
                ), RepositoryErrorCode::Validation);
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

RepositoryResult<QList<MyClassesTeacherDisplayNameBatchReadRecord>>
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
            return repositoryUnexpected(QObject::tr(
                "Loading teacher display names failed: teacher identifiers must be positive and unique."
                ), RepositoryErrorCode::InvalidInput);
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
        return repositoryUnexpected(executed.error().userMessage());
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
            return repositoryUnexpected(QObject::tr(
                "Loading teacher display names failed: the database returned an invalid teacher identifier."
                ), RepositoryErrorCode::Validation);
        }

        const qsizetype ordinal = static_cast<qsizetype>(requestedOrdinal);
        const qint64 expectedTeacherId = teacherIds[ordinal];
        if (ordinal <= lastOrdinal
            || requestedTeacherId != expectedTeacherId
            || returnedTeacherId != expectedTeacherId)
        {
            return repositoryUnexpected(QObject::tr(
                "Loading teacher display names failed: the database returned records in a different identifier order."
                ), RepositoryErrorCode::Validation);
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

    if (query.lastError().isValid())
    {
        return repositoryUnexpected(
            SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading teacher display names")
                ).userMessage()
            );
    }

    return records;
}

const TeacherDisplayNameBatchReadMetrics&
TeacherRepository::teacherDisplayNameBatchReadMetrics() const noexcept
{
    return m_teacherDisplayNameBatchReadMetrics;
}

RepositoryResult<QList<TeacherProfileBatchReadRecord>>
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
            return repositoryUnexpected(QObject::tr(
                "Loading teacher profiles failed: teacher identifiers must be positive and unique."
                ), RepositoryErrorCode::InvalidInput);
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
            return repositoryUnexpected(QObject::tr(
                "Loading teacher profiles failed: the database returned an unexpected number of records."
                ), RepositoryErrorCode::Validation);
        }

        const int requestedTeacherId = teacherIds[rowIndex];
        bool requestedTeacherIdOk = false;
        const qint64 returnedRequestedTeacherId = query.value(
            "requested_teacher_id").toLongLong(&requestedTeacherIdOk);
        bool ordinalOk = false;
        const qint64 returnedOrdinal = query.value("requested_ordinal")
            .toLongLong(&ordinalOk);
        if (!requestedTeacherIdOk
            || returnedRequestedTeacherId != requestedTeacherId
            || !ordinalOk
            || returnedOrdinal != rowIndex)
        {
            return repositoryUnexpected(QObject::tr(
                "Loading teacher profiles failed: the database returned records in a different identifier order."
                ), RepositoryErrorCode::Validation);
        }

        if (query.value("id").isNull())
        {
            entries.append({
                requestedTeacherId,
                repositoryUnexpected(QObject::tr(
                    "Loading teacher failed for %1: no matching record exists."
                    ).arg(teacherIdentity(requestedTeacherId)),
                    RepositoryErrorCode::NotFound)
            });
            ++rowIndex;
            continue;
        }

        bool returnedTeacherIdOk = false;
        const qint64 returnedTeacherId = query.value("id").toLongLong(
            &returnedTeacherIdOk);
        if (!returnedTeacherIdOk || returnedTeacherId != requestedTeacherId)
        {
            return repositoryUnexpected(QObject::tr(
                "Loading teacher profiles failed: a database record returned a different teacher identifier."
                ), RepositoryErrorCode::Validation);
        }

        Teacher teacher = teacherFromQuery(query);
        entries.append({requestedTeacherId, std::move(teacher)});
        ++rowIndex;
    }

    if (query.lastError().isValid())
    {
        return repositoryUnexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading teacher profiles"),
            {},
            QStringLiteral("assigned teachers")
            ).userMessage());
    }

    if (rowIndex != teacherIds.size())
    {
        return repositoryUnexpected(QObject::tr(
            "Loading teacher profiles failed: the database returned an incomplete record list."
            ), RepositoryErrorCode::Validation);
    }

    return entries;
}

namespace
{
RepositoryResult<TeacherProfileReadRecord> lookupTeacherProfileRecord(
    QSqlDatabase& database,
    const qint64 teacherId,
    const QString& action
    )
{
    if (teacherId <= 0)
    {
        return repositoryUnexpected(
            QObject::tr(
                "Loading teacher profile failed: invalid teacher id %1."
                ).arg(teacherId),
            RepositoryErrorCode::InvalidInput
            );
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
        return repositoryUnexpected(executed.error().userMessage());
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return repositoryUnexpected(SqlQueryUtils::errorFor(
                    query,
                    action,
                    {},
                    identity
                    ).userMessage());
        }

        return repositoryUnexpected(
            QObject::tr(
                "Loading teacher failed for %1: no matching record exists."
                ).arg(identity),
            RepositoryErrorCode::NotFound
            );
    }

    bool identifierValid = false;
    const qint64 returnedTeacherId = query.value(QStringLiteral("id"))
        .toLongLong(&identifierValid);
    if (!identifierValid || returnedTeacherId != teacherId)
        return repositoryUnexpected(
            QObject::tr(
                "Loading teacher profiles failed: a database record returned a different teacher identifier."),
            RepositoryErrorCode::Validation
            );

    if (query.lastError().isValid())
    {
        return repositoryUnexpected(SqlQueryUtils::errorFor(
            query,
            action,
            {},
            identity
            ).userMessage());
    }

    return TeacherProfileReadRecord{
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
}

}

RepositoryResult<QList<ClassTransferExportTeacherProfileBatchReadRecord>>
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
            return repositoryUnexpected(QObject::tr(
                "Loading teacher profiles failed: teacher identifiers must be positive and unique."
                ), RepositoryErrorCode::InvalidInput);
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
            entries.append({teacherId, std::move(lookup)});
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
            return repositoryUnexpected(QObject::tr(
                "Loading teacher profiles failed: the database returned an unexpected number of records."
                ), RepositoryErrorCode::Validation);
        }

        const qint64 requestedTeacherId = teacherIds[rowIndex];
        bool requestedIdValid = false;
        const qint64 returnedRequestedId = query.value("requested_teacher_id")
            .toLongLong(&requestedIdValid);
        bool ordinalValid = false;
        const qint64 returnedOrdinal = query.value("requested_ordinal").toLongLong(&ordinalValid);
        if (!requestedIdValid || returnedRequestedId != requestedTeacherId
            || !ordinalValid || returnedOrdinal != rowIndex)
        {
            return repositoryUnexpected(QObject::tr(
                "Loading teacher profiles failed: the database returned records in a different identifier order."
                ), RepositoryErrorCode::Validation);
        }

        if (query.value("id").isNull())
        {
            entries.append({
                requestedTeacherId,
                repositoryUnexpected(QObject::tr(
                    "Loading teacher failed for %1: no matching record exists."
                    ).arg(teacherIdentity(requestedTeacherId)),
                    RepositoryErrorCode::NotFound)
            });
            ++rowIndex;
            continue;
        }

        bool teacherIdValid = false;
        const qint64 returnedTeacherId = query.value("id").toLongLong(&teacherIdValid);
        if (!teacherIdValid || returnedTeacherId != requestedTeacherId)
        {
            return repositoryUnexpected(QObject::tr(
                "Loading teacher profiles failed: a database record returned a different teacher identifier."
                ), RepositoryErrorCode::Validation);
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
        return repositoryUnexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading teacher profiles"),
            {},
            QStringLiteral("assigned teachers")
            ).userMessage());
    if (rowIndex != teacherIds.size())
    {
        return repositoryUnexpected(QObject::tr(
            "Loading teacher profiles failed: the database returned an incomplete record list."
            ), RepositoryErrorCode::Validation);
    }

    return entries;
}

const TeacherProfileBatchReadMetrics&
TeacherRepository::teacherProfileBatchReadMetrics() const noexcept
{
    return m_teacherProfileBatchReadMetrics;
}

RepositoryResult<TeacherProfileReadRecord> TeacherRepository::lookupTeacherProfileById(
    const qint64 teacherId)
{
    return lookupTeacherProfileById(teacherId, QObject::tr("Loading teacher profile"));
}

RepositoryResult<TeacherProfileReadRecord> TeacherRepository::lookupTeacherProfileById(
    const qint64 teacherId, const QString& action)
{
    return lookupTeacherProfileRecord(m_database, teacherId, action);
}

RepositoryResult<QList<SubPrepPrintSourceTeacherProfileBatchReadRecord>>
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
            return repositoryUnexpected(QObject::tr(
                "Loading Sub Prep print source teacher profiles failed: teacher identifiers must be positive and unique."
                ), RepositoryErrorCode::InvalidInput);
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
            return repositoryUnexpected(QObject::tr(
                "Loading Sub Prep print source teacher profiles failed: the database returned an unexpected number of records."
                ), RepositoryErrorCode::Validation);
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
            return repositoryUnexpected(QObject::tr(
                "Loading Sub Prep print source teacher profiles failed: the database returned records in a different identifier order."
                ), RepositoryErrorCode::Validation);
        }

        if (query.value(QStringLiteral("teacher_id")).isNull())
        {
            entries.append({
                requestedTeacherId,
                repositoryUnexpected(QObject::tr(
                    "Loading teacher failed for %1: no matching record exists."
                    ).arg(teacherIdentity(requestedTeacherId)),
                    RepositoryErrorCode::NotFound)
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
            return repositoryUnexpected(QObject::tr(
                "Loading Sub Prep print source teacher profiles failed: the database returned a different teacher identifier."
                ), RepositoryErrorCode::Validation);
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

    if (query.lastError().isValid())
    {
        return repositoryUnexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading Sub Prep print source teacher profiles"),
            {},
            QStringLiteral("assigned teachers")
            ).userMessage());
    }

    if (rowIndex != teacherIds.size())
    {
        return repositoryUnexpected(QObject::tr(
            "Loading Sub Prep print source teacher profiles failed: the database returned an incomplete record list."
            ), RepositoryErrorCode::Validation);
    }

    return entries;
}

const SubPrepPrintSourceTeacherProfileBatchReadMetrics&
TeacherRepository::subPrepPrintSourceTeacherProfileBatchReadMetrics() const noexcept
{
    return m_subPrepPrintSourceTeacherProfileBatchReadMetrics;
}

RepositoryResult<SubPrepPrintSourceTeacherProfileReadFields>
TeacherRepository::loadSubPrepPrintSourceTeacherProfileRecord(
    const qint64 teacherId
    )
{
    if (teacherId <= 0)
    {
        return repositoryUnexpected(QObject::tr(
            "Loading teacher failed: invalid %1."
            ).arg(teacherIdentity(teacherId)), RepositoryErrorCode::InvalidInput);
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
        return repositoryUnexpected(SqlQueryUtils::errorFor(
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
        return repositoryUnexpected(executed.error().userMessage());
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return repositoryUnexpected(SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading Sub Prep print source teacher profile"),
                {},
                teacherIdentity(teacherId)
                ).userMessage());
        }
        return repositoryUnexpected(QObject::tr(
            "Loading teacher failed for %1: no matching record exists."
            ).arg(teacherIdentity(teacherId)), RepositoryErrorCode::NotFound);
    }

    bool returnedTeacherIdValid = false;
    const qint64 returnedTeacherId = query.value(
        QStringLiteral("teacher_id")
        ).toLongLong(&returnedTeacherIdValid);
    if (!returnedTeacherIdValid || returnedTeacherId != teacherId)
    {
        return repositoryUnexpected(QObject::tr(
            "Loading Sub Prep print source teacher profile failed for %1: the database returned a different teacher identifier."
            ).arg(teacherIdentity(teacherId)), RepositoryErrorCode::Validation);
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

    const bool hasDuplicate = query.next();
    if (query.lastError().isValid())
    {
        return repositoryUnexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading Sub Prep print source teacher profile"),
            {},
            teacherIdentity(teacherId)
            ).userMessage());
    }
    if (hasDuplicate)
    {
        return repositoryUnexpected(QObject::tr(
            "Loading Sub Prep print source teacher profile failed for %1: the database returned duplicate records."
            ).arg(teacherIdentity(teacherId)), RepositoryErrorCode::Validation);
    }

    return profile;
}

RepositoryResult<QList<SubPrepRosterOutputTeacherProfileBatchReadRecord>>
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
            return repositoryUnexpected(QObject::tr(
                "Loading Sub Prep roster-output teacher profiles failed: "
                "teacher identifiers must be positive and unique."
                ), RepositoryErrorCode::InvalidInput);
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
        return repositoryUnexpected(query.lastError().text());
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
        return repositoryUnexpected(executed.error().userMessage());
    }

    QList<SubPrepRosterOutputTeacherProfileBatchReadRecord> entries;
    entries.reserve(teacherIds.size());
    qsizetype rowIndex = 0;
    while (query.next())
    {
        if (rowIndex >= teacherIds.size())
        {
            return repositoryUnexpected(QObject::tr(
                "Loading Sub Prep roster-output teacher profiles failed: "
                "the database returned an unexpected number of records."
                ), RepositoryErrorCode::Validation);
        }

        const qint64 requestedTeacherId = teacherIds[rowIndex];
        bool returnedRequestedTeacherIdOk = false;
        const qint64 returnedRequestedTeacherId = query.value(
            "requested_teacher_id"
            ).toLongLong(&returnedRequestedTeacherIdOk);
        bool returnedOrdinalOk = false;
        const qint64 returnedOrdinal = query.value("requested_ordinal")
            .toLongLong(&returnedOrdinalOk);
        if (!returnedRequestedTeacherIdOk
            || returnedRequestedTeacherId != requestedTeacherId
            || !returnedOrdinalOk
            || returnedOrdinal != rowIndex)
        {
            return repositoryUnexpected(QObject::tr(
                "Loading Sub Prep roster-output teacher profiles failed: "
                "the database returned records in a different identifier order."
                ), RepositoryErrorCode::Validation);
        }

        if (query.value("id").isNull())
        {
            entries.append({
                requestedTeacherId,
                repositoryUnexpected(QObject::tr(
                    "Loading teacher failed for %1: no matching record exists."
                    ).arg(teacherIdentity(requestedTeacherId)),
                    RepositoryErrorCode::NotFound)
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
            return repositoryUnexpected(QObject::tr(
                "Loading Sub Prep roster-output teacher failed for %1: "
                "the database returned a different teacher identifier."
                ).arg(teacherIdentity(requestedTeacherId)), RepositoryErrorCode::Validation);
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

    if (query.lastError().isValid())
    {
        return repositoryUnexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading Sub Prep roster-output teacher profiles"),
            {},
            QStringLiteral("assigned teachers")
            ).userMessage());
    }

    if (rowIndex != teacherIds.size())
    {
        return repositoryUnexpected(QObject::tr(
            "Loading Sub Prep roster-output teacher profiles failed: "
            "the database returned an incomplete record list."
            ), RepositoryErrorCode::Validation);
    }

    return entries;
}

const SubPrepRosterOutputTeacherProfileBatchReadMetrics&
TeacherRepository::subPrepRosterOutputTeacherProfileBatchReadMetrics() const noexcept
{
    return m_subPrepRosterOutputTeacherProfileBatchReadMetrics;
}

RepositoryResult<QList<MyClassesTeacherProfileBatchReadRecord>>
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
            return repositoryUnexpected(QObject::tr(
                "Loading My Classes teacher profiles failed: teacher identifiers must be positive and unique."
                ), RepositoryErrorCode::InvalidInput);
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
        return repositoryUnexpected(query.lastError().text());
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
        return repositoryUnexpected(executed.error().userMessage());
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
                .profile = repositoryUnexpected(QObject::tr(
                    "Loading My Classes teacher profiles failed: the database returned an unexpected number of records."
                    ), RepositoryErrorCode::Validation)
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
                .requestedTeacherId = requestedTeacherId,
                .profile = repositoryUnexpected(QObject::tr(
                    "Loading My Classes teacher profiles failed: the database returned records in a different identifier order."
                    ), RepositoryErrorCode::Validation)
            });
            ++rowIndex;
            continue;
        }

        if (query.value("id").isNull())
        {
            entries.append({
                .requestedTeacherId = requestedTeacherId,
                .profile = repositoryUnexpected(QObject::tr(
                    "Loading teacher failed for %1: no matching record exists."
                    ).arg(teacherIdentity(requestedTeacherId)),
                    RepositoryErrorCode::NotFound)
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
                .profile = repositoryUnexpected(QObject::tr(
                    "Loading My Classes teacher failed for %1: the database returned a different teacher identifier."
                    ).arg(teacherIdentity(requestedTeacherId)),
                    RepositoryErrorCode::Validation)
            });
            ++rowIndex;
            continue;
        }

        entries.append({
            .requestedTeacherId = requestedTeacherId,
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
        return repositoryUnexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading My Classes teacher profiles"),
            {},
            QStringLiteral("assigned teachers")
            ).userMessage());
    }

    if (rowIndex != teacherIds.size())
    {
        return repositoryUnexpected(QObject::tr(
            "Loading My Classes teacher profiles failed: the database returned an incomplete record list."
            ), RepositoryErrorCode::Validation);
    }

    return entries;
}

const MyClassesTeacherProfileBatchReadMetrics&
TeacherRepository::myClassesTeacherProfileBatchReadMetrics() const noexcept
{
    return m_myClassesTeacherProfileBatchReadMetrics;
}

RepositoryResult<QList<Teacher>> TeacherRepository::getAllTeachers()
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
        return repositoryUnexpected(executed.error().userMessage());
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
            return repositoryUnexpected(
                QObject::tr(
                    "Loading teachers found an invalid teacher identifier %1."
                    ).arg(query.value(QStringLiteral("id")).toString()),
                RepositoryErrorCode::Validation
                );
        }

        Teacher teacher = teacherFromQuery(query);
        teacher.id = static_cast<int>(storedTeacherId);
        teachers.append(std::move(teacher));
    }

    if (query.lastError().isValid())
    {
        return repositoryUnexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading teachers")
            ).userMessage());
    }

    return teachers;
}

RepositoryResult<QList<TeacherDirectoryReadRecord>>
TeacherRepository::loadTeacherDirectoryRecords()
{
    QList<TeacherDirectoryReadRecord> teachers;
    QSqlQuery query(m_database);

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            SELECT id, teacher_kr, teacher_en,
                   preferred_romanization, preferred_name
            FROM teachers
        )"),
        QObject::tr("Loading Korean teachers")
        );
    if (!executed)
    {
        return repositoryUnexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        bool teacherIdOk = false;
        const qint64 teacherId =
            query.value(QStringLiteral("id")).toLongLong(&teacherIdOk);
        if (!teacherIdOk || teacherId <= 0)
        {
            return repositoryUnexpected(
                QObject::tr(
                    "Loading Korean teachers found an invalid teacher identifier %1."
                    ).arg(query.value(QStringLiteral("id")).toString()),
                RepositoryErrorCode::Validation
                );
        }

        teachers.append({
            .teacherId = teacherId,
            .teacherKr = query.value(QStringLiteral("teacher_kr")).toString(),
            .teacherEn = query.value(QStringLiteral("teacher_en")).toString(),
            .preferredRomanization = query.value(
                QStringLiteral("preferred_romanization")
                ).toString(),
            .preferredName = query.value(QStringLiteral("preferred_name")).toString()
        });
    }

    if (query.lastError().isValid())
    {
        return repositoryUnexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading Korean teachers")
            ).userMessage());
    }

    return teachers;
}

RepositoryResult<QList<ScheduleImportTeacherReadRecord>>
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
        return repositoryUnexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        bool teacherIdOk = false;
        const qint64 teacherId = query.value(QStringLiteral("id"))
            .toLongLong(&teacherIdOk);
        if (!teacherIdOk)
        {
            return repositoryUnexpected(
                QObject::tr("A Korean teacher ID could not be read."),
                RepositoryErrorCode::Validation
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
        return repositoryUnexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading schedule import teachers")
            ).userMessage());
    }

    m_scheduleImportTeacherReadMetrics.returnedRowCount += teachers.size();
    return teachers;
}

const ScheduleImportTeacherReadMetrics&
TeacherRepository::scheduleImportTeacherReadMetrics() const noexcept
{
    return m_scheduleImportTeacherReadMetrics;
}

RepositoryResult<QList<InitialSetupTeacherChoiceReadRecord>>
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
        return repositoryUnexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        bool teacherIdOk = false;
        const qint64 teacherId = query.value(QStringLiteral("id"))
            .toLongLong(&teacherIdOk);
        if (!teacherIdOk || teacherId <= 0)
        {
            return repositoryUnexpected(
                QObject::tr(
                    "Initial setup teacher choices contain an invalid teacher identifier."
                    ),
                RepositoryErrorCode::Validation
                );
        }
        teachers.append({
            teacherId,
            query.value(QStringLiteral("teacher_kr")).toString(),
            query.value(QStringLiteral("teacher_en")).toString(),
            query.value(QStringLiteral("preferred_romanization")).toString(),
            query.value(QStringLiteral("preferred_name")).toString()
        });
    }

    if (query.lastError().isValid())
    {
        return repositoryUnexpected(
            SqlQueryUtils::errorFor(query, QObject::tr("Loading teachers"))
                .userMessage()
            );
    }

    return teachers;
}

RepositoryResult<QList<TestingTeacherChoiceReadRecord>>
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
        return repositoryUnexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        bool teacherIdOk = false;
        const qint64 teacherId = query.value(QStringLiteral("id"))
            .toLongLong(&teacherIdOk);
        if (!teacherIdOk || teacherId <= 0)
        {
            return repositoryUnexpected(
                QObject::tr("A teacher has an invalid identifier."),
                RepositoryErrorCode::Validation
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
        return repositoryUnexpected(
            SqlQueryUtils::errorFor(query, QObject::tr("Loading teachers"))
                .userMessage()
            );
    }

    return teachers;
}

RepositoryResult<QList<ClassCoTeacherTeacherChoiceReadRecord>>
TeacherRepository::loadClassCoTeacherTeacherChoiceRecords()
{
    QList<ClassCoTeacherTeacherChoiceReadRecord> teachers;
    QSqlQuery query(m_database);
    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            SELECT id, teacher_kr, teacher_en, room_number, internet_type,
                   wifi_name, wifi_password, projection_type, zoom_id,
                   zoom_password
            FROM teachers
            ORDER BY teacher_en
        )"),
        QObject::tr("Loading teachers"));
    if (!executed)
    {
        return repositoryUnexpected(executed.error().userMessage());
    }
    while (query.next())
    {
        bool teacherIdOk = false;
        const qint64 teacherId = query.value(QStringLiteral("id"))
            .toLongLong(&teacherIdOk);
        if (!teacherIdOk || teacherId <= 0)
        {
            return repositoryUnexpected(
                QObject::tr("A co-teacher has an invalid identifier."),
                RepositoryErrorCode::Validation);
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
        return repositoryUnexpected(query.lastError().text());
    }
    return teachers;
}

RepositoryResult<QList<ClassCoTeacherTeacherChoiceReadRecord>>
TeacherRepository::loadBoundedClassCoTeacherTeacherChoiceRecords(
    const int maximumRows,
    const int maximumNameBytes,
    const qint64 maximumTotalNameBytes
    )
{
    if (maximumRows <= 0 || maximumNameBytes <= 0
        || maximumTotalNameBytes <= 0)
    {
        return repositoryUnexpected(
            QObject::tr("The co-teacher choice row bound must be positive."),
            RepositoryErrorCode::Validation);
    }
    QList<ClassCoTeacherTeacherChoiceReadRecord> teachers;

    QSqlQuery query(m_database);
    const bool prepared = query.prepare(QStringLiteral(R"(
        WITH limited_names AS (
            SELECT id, teacher_kr, teacher_en,
                   length(CAST(COALESCE(teacher_kr, '') AS BLOB)) AS kr_bytes,
                   length(CAST(COALESCE(teacher_en, '') AS BLOB)) AS en_bytes
            FROM teachers
            ORDER BY teacher_en, id
            LIMIT ?
        ),
        sized_names AS (
            SELECT *,
                   SUM(kr_bytes + en_bytes) OVER (
                       ORDER BY teacher_en, id
                       ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
                   ) AS total_name_bytes
            FROM limited_names
        )
        SELECT id,
               CASE WHEN kr_bytes <= ? AND en_bytes <= ?
                          AND total_name_bytes <= ?
                    THEN teacher_kr ELSE NULL END AS teacher_kr,
               CASE WHEN kr_bytes <= ? AND en_bytes <= ?
                          AND total_name_bytes <= ?
                    THEN teacher_en ELSE NULL END AS teacher_en,
               (kr_bytes > ? OR en_bytes > ? OR total_name_bytes > ?)
                    AS text_exceeds_bound
        FROM sized_names
        ORDER BY teacher_en, id
    )"));
    if (!prepared)
    {
        return repositoryUnexpected(query.lastError().text());
    }
    query.addBindValue(maximumRows);
    for (int copy = 0; copy < 2; ++copy)
    {
        query.addBindValue(maximumNameBytes);
        query.addBindValue(maximumNameBytes);
        query.addBindValue(maximumTotalNameBytes);
    }
    query.addBindValue(maximumNameBytes);
    query.addBindValue(maximumNameBytes);
    query.addBindValue(maximumTotalNameBytes);
    const auto executed = SqlQueryUtils::executePrepared(
        query, QObject::tr("Loading teachers"));
    if (!executed)
    {
        return repositoryUnexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        bool teacherIdOk = false;
        const qint64 teacherId = query.value(QStringLiteral("id"))
            .toLongLong(&teacherIdOk);
        if (!teacherIdOk || teacherId <= 0)
        {
            return repositoryUnexpected(
                QObject::tr("A co-teacher has an invalid identifier."),
                RepositoryErrorCode::Validation
                );
        }

        const bool textExceedsBound = query.value(
            QStringLiteral("text_exceeds_bound")).toBool();
        teachers.append({
            .teacherId = teacherId,
            .teacherKr = query.value(QStringLiteral("teacher_kr")).toString(),
            .teacherEn = query.value(QStringLiteral("teacher_en")).toString(),
            .textExceedsBound = textExceedsBound
        });
    }

    if (query.lastError().isValid())
    {
        return repositoryUnexpected(query.lastError().text());
    }

    return teachers;
}

RepositoryResult<ClassCoTeacherTeacherProfileReadRecord>
TeacherRepository::loadBoundedClassCoTeacherTeacherProfile(
    const qint64 teacherId,
    const int maximumFieldBytes,
    const qint64 maximumTotalBytes
    )
{
    if (teacherId <= 0 || maximumFieldBytes <= 0 || maximumTotalBytes <= 0)
    {
        return repositoryUnexpected(
            QObject::tr("The co-teacher profile bounds and identifier must be positive."),
            RepositoryErrorCode::Validation);
    }
    QSqlQuery query(m_database);
    const bool prepared = query.prepare(QStringLiteral(R"(
        WITH profile AS (
            SELECT id, room_number, internet_type, wifi_name, wifi_password,
                   projection_type, zoom_id, zoom_password,
                   length(CAST(COALESCE(room_number, '') AS BLOB)) AS room_bytes,
                   length(CAST(COALESCE(internet_type, '') AS BLOB)) AS internet_bytes,
                   length(CAST(COALESCE(wifi_name, '') AS BLOB)) AS wifi_name_bytes,
                   length(CAST(COALESCE(wifi_password, '') AS BLOB)) AS wifi_password_bytes,
                   length(CAST(COALESCE(projection_type, '') AS BLOB)) AS projection_bytes,
                   length(CAST(COALESCE(zoom_id, '') AS BLOB)) AS zoom_id_bytes,
                   length(CAST(COALESCE(zoom_password, '') AS BLOB)) AS zoom_password_bytes,
                   length(CAST(COALESCE(room_number, '') AS BLOB))
                     + length(CAST(COALESCE(internet_type, '') AS BLOB))
                     + length(CAST(COALESCE(wifi_name, '') AS BLOB))
                     + length(CAST(COALESCE(wifi_password, '') AS BLOB))
                     + length(CAST(COALESCE(projection_type, '') AS BLOB))
                     + length(CAST(COALESCE(zoom_id, '') AS BLOB))
                     + length(CAST(COALESCE(zoom_password, '') AS BLOB))
                       AS total_bytes
            FROM teachers
            WHERE id = ?
        )
        SELECT id,
               CASE WHEN room_bytes <= ? AND internet_bytes <= ?
                          AND wifi_name_bytes <= ? AND wifi_password_bytes <= ?
                          AND projection_bytes <= ? AND zoom_id_bytes <= ?
                          AND zoom_password_bytes <= ? AND total_bytes <= ?
                    THEN room_number ELSE NULL END AS room_number,
               CASE WHEN room_bytes <= ? AND internet_bytes <= ?
                          AND wifi_name_bytes <= ? AND wifi_password_bytes <= ?
                          AND projection_bytes <= ? AND zoom_id_bytes <= ?
                          AND zoom_password_bytes <= ? AND total_bytes <= ?
                    THEN internet_type ELSE NULL END AS internet_type,
               CASE WHEN room_bytes <= ? AND internet_bytes <= ?
                          AND wifi_name_bytes <= ? AND wifi_password_bytes <= ?
                          AND projection_bytes <= ? AND zoom_id_bytes <= ?
                          AND zoom_password_bytes <= ? AND total_bytes <= ?
                    THEN wifi_name ELSE NULL END AS wifi_name,
               CASE WHEN room_bytes <= ? AND internet_bytes <= ?
                          AND wifi_name_bytes <= ? AND wifi_password_bytes <= ?
                          AND projection_bytes <= ? AND zoom_id_bytes <= ?
                          AND zoom_password_bytes <= ? AND total_bytes <= ?
                    THEN wifi_password ELSE NULL END AS wifi_password,
               CASE WHEN room_bytes <= ? AND internet_bytes <= ?
                          AND wifi_name_bytes <= ? AND wifi_password_bytes <= ?
                          AND projection_bytes <= ? AND zoom_id_bytes <= ?
                          AND zoom_password_bytes <= ? AND total_bytes <= ?
                    THEN projection_type ELSE NULL END AS projection_type,
               CASE WHEN room_bytes <= ? AND internet_bytes <= ?
                          AND wifi_name_bytes <= ? AND wifi_password_bytes <= ?
                          AND projection_bytes <= ? AND zoom_id_bytes <= ?
                          AND zoom_password_bytes <= ? AND total_bytes <= ?
                    THEN zoom_id ELSE NULL END AS zoom_id,
               CASE WHEN room_bytes <= ? AND internet_bytes <= ?
                          AND wifi_name_bytes <= ? AND wifi_password_bytes <= ?
                          AND projection_bytes <= ? AND zoom_id_bytes <= ?
                          AND zoom_password_bytes <= ? AND total_bytes <= ?
                    THEN zoom_password ELSE NULL END AS zoom_password,
               (room_bytes > ? OR internet_bytes > ? OR wifi_name_bytes > ?
                OR wifi_password_bytes > ? OR projection_bytes > ?
                OR zoom_id_bytes > ? OR zoom_password_bytes > ?
                OR total_bytes > ?) AS text_exceeds_bound
        FROM profile
    )"));
    if (!prepared)
    {
        return repositoryUnexpected(query.lastError().text());
    }
    query.addBindValue(teacherId);
    for (int field = 0; field < 7; ++field)
    {
        for (int copy = 0; copy < 7; ++copy)
        {
            query.addBindValue(maximumFieldBytes);
        }
        query.addBindValue(maximumTotalBytes);
    }
    for (int copy = 0; copy < 7; ++copy)
    {
        query.addBindValue(maximumFieldBytes);
    }
    query.addBindValue(maximumTotalBytes);
    const auto executed = SqlQueryUtils::executePrepared(
        query, QObject::tr("Loading co-teacher details"));
    if (!executed)
    {
        return repositoryUnexpected(executed.error().userMessage());
    }
    if (!query.next())
    {
        return repositoryUnexpected(
            QObject::tr("The selected co-teacher could not be found."),
            RepositoryErrorCode::NotFound);
    }
    bool teacherIdOk = false;
    const qint64 returnedTeacherId = query.value(QStringLiteral("id"))
        .toLongLong(&teacherIdOk);
    if (!teacherIdOk || returnedTeacherId != teacherId)
    {
        return repositoryUnexpected(
            QObject::tr("The selected co-teacher has an invalid identifier."),
            RepositoryErrorCode::Validation);
    }
    return ClassCoTeacherTeacherProfileReadRecord{
        .teacherId = returnedTeacherId,
        .roomNumber = query.value(QStringLiteral("room_number")).toString(),
        .internetType = query.value(QStringLiteral("internet_type")).toString(),
        .wifiName = query.value(QStringLiteral("wifi_name")).toString(),
        .wifiPassword = query.value(QStringLiteral("wifi_password")).toString(),
        .projectionType = query.value(QStringLiteral("projection_type")).toString(),
        .zoomId = query.value(QStringLiteral("zoom_id")).toString(),
        .zoomPassword = query.value(QStringLiteral("zoom_password")).toString(),
        .textExceedsBound = query.value(QStringLiteral("text_exceeds_bound")).toBool()
    };
}

RepositoryResult<QList<KoreanTeacherBirthdayDirectoryReadRecord>>
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
        return repositoryUnexpected(executed.error().userMessage());
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
        return repositoryUnexpected(query.lastError().text());
    }

    return teachers;
}

RepositoryStatus TeacherRepository::deleteTeacher(
    int teacherId
    )
{
    return deleteTeacher(static_cast<qint64>(teacherId));
}

RepositoryStatus TeacherRepository::deleteTeacher(
    qint64 teacherId
    )
{
    if (teacherId <= 0)
    {
        return repositoryUnexpected(
            QObject::tr("Deleting teacher failed: invalid teacher id %1.")
                .arg(teacherId),
            RepositoryErrorCode::InvalidInput
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return repositoryUnexpected(
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

    RepositoryStatus status = statusFromExecution(
        SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Removing teacher from classes"),
            identity
            )
        );
    if (!status)
    {
        const RepositoryError primaryError = status.error();
        if (!transaction.rollbackChecked())
        {
            return repositoryUnexpected(
                QObject::tr("%1; rolling back teacher deletion failed: %2")
                    .arg(primaryError.diagnostic, m_database.lastError().text()),
                RepositoryErrorCode::Technical
                );
        }
        return repositoryUnexpected(primaryError);
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
        const RepositoryError primaryError = status.error();
        if (!transaction.rollbackChecked())
        {
            return repositoryUnexpected(
                QObject::tr("%1; rolling back teacher deletion failed: %2")
                    .arg(primaryError.diagnostic, m_database.lastError().text()),
                RepositoryErrorCode::Technical
                );
        }
        return repositoryUnexpected(primaryError);
    }

    if (!transaction.commit())
    {
        const QString primaryDiagnostic =
            QObject::tr("Committing teacher deletion failed for %1: %2")
                .arg(identity, m_database.lastError().text());
        if (!transaction.rollbackChecked())
        {
            return repositoryUnexpected(
                QObject::tr("%1; rolling back teacher deletion failed: %2")
                    .arg(primaryDiagnostic, m_database.lastError().text()),
                RepositoryErrorCode::Technical
                );
        }
        return repositoryUnexpected(primaryDiagnostic);
    }

    return {};
}
