#include "class_info_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"
#include "domain/models/classroom.h"

#include <QDebug>
#include <QHash>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QStringList>

#include <limits>

namespace
{
struct TimeInterval
{
    int start{-1};
    int end{-1};
};

constexpr int MinutesPerDay = 24 * 60;
constexpr int MinutesPerWeek = 7 * MinutesPerDay;

int dayIndex(
    const QString& day
    )
{
    static const QStringList days{
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday",
        "Sunday"
    };

    return days.indexOf(day);
}

int timeToMinutes(
    const QString& value
    )
{
    const QStringList parts =
        value.trimmed().split(
            ' ',
            Qt::SkipEmptyParts
            );

    if (parts.size() != 2)
    {
        return -1;
    }

    const QStringList timeParts =
        parts[0].split(':');

    if (timeParts.size() != 2)
    {
        return -1;
    }

    bool hourOk = false;
    bool minuteOk = false;

    int hour =
        timeParts[0].toInt(&hourOk);

    const int minute =
        timeParts[1].toInt(&minuteOk);

    const QString period =
        parts[1].toUpper();

    if (
        !hourOk
        || !minuteOk
        || hour < 1
        || hour > 12
        || minute < 0
        || minute > 59
        || (period != "AM" && period != "PM")
        )
    {
        return -1;
    }

    if (period == "AM")
    {
        if (hour == 12)
        {
            hour = 0;
        }
    }
    else if (hour != 12)
    {
        hour += 12;
    }

    return hour * 60 + minute;
}

bool toInterval(
    const ClassTime& time,
    TimeInterval& interval
    )
{
    const int day =
        dayIndex(time.day);

    const int start =
        timeToMinutes(time.startTime);

    const int end =
        timeToMinutes(time.endTime);

    if (day < 0 || start < 0 || end < 0)
    {
        return false;
    }

    interval.start =
        day * MinutesPerDay + start;

    interval.end =
        day * MinutesPerDay + end;

    if (interval.end <= interval.start)
    {
        interval.end += MinutesPerDay;
    }

    return true;
}

bool intervalsOverlap(
    const TimeInterval& first,
    const TimeInterval& second
    )
{
    for (int offset : { -MinutesPerWeek, 0, MinutesPerWeek })
    {
        const int secondStart =
            second.start + offset;

        const int secondEnd =
            second.end + offset;

        if (first.start < secondEnd && secondStart < first.end)
        {
            return true;
        }
    }

    return false;
}

QString classDisplayName(
    const QString& className,
    int classId
    )
{
    if (!className.trimmed().isEmpty())
    {
        return className.trimmed();
    }

    return QString("Class %1").arg(classId);
}

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

    // Preserve unrecognized values from existing profiles rather than
    // fabricating a valid-looking default during a read.
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

Result<Classroom> loadClassById(
    QSqlDatabase& database,
    int classId
    )
{
    QSqlQuery query(database);

    query.prepare(R"(
        SELECT *
        FROM classes
        WHERE id=?
    )");

    query.addBindValue(classId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading class for conflict detection"),
        QObject::tr("class id %1").arg(classId)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    if (!query.next())
    {
        return std::unexpected(
            QObject::tr(
                "Loading class for conflict detection failed for class id "
                "%1: no matching record exists."
                ).arg(classId)
            );
    }

    Classroom classroom;
    classroom.id =
        query.value("id").toInt();

    classroom.name =
        query.value("name").toString();

    return classroom;
}
}

ClassInfoRepository::ClassInfoRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Status ClassInfoRepository::saveClassInfo(
    const ClassInfo& info
    )
{
    if (info.classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Saving class information failed: invalid class id %1.")
                .arg(info.classId)
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr(
                "Starting class information save transaction failed for "
                "class id %1: %2"
                ).arg(info.classId)
                 .arg(m_database.lastError().text())
            );
    }

    QSqlQuery query(m_database);
    const QString identity = QObject::tr("class id %1").arg(info.classId);
    auto execute = [&](const QString& action) -> Status
    {
        const auto result = SqlQueryUtils::executePrepared(
            query, action, identity);
        return result
            ? Status{}
            : Status(std::unexpected(result.error().userMessage()));
    };

    query.prepare(R"(
        INSERT INTO class_info (
            class_id,
            teacher_id,
            class_grade,
            class_level,
            reading_book,
            essay_book,
            class_color,
            font_color,
            notes,
            time_filler_activities
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)

        ON CONFLICT(class_id)
        DO UPDATE SET
            teacher_id=excluded.teacher_id,
            class_grade=excluded.class_grade,
            class_level=excluded.class_level,
            reading_book=excluded.reading_book,
            essay_book=excluded.essay_book,
            class_color=excluded.class_color,
            font_color=excluded.font_color,
            notes=excluded.notes,
            time_filler_activities=excluded.time_filler_activities
    )");

    query.addBindValue(info.classId);
    query.addBindValue(
        info.teacherId > 0
            ? QVariant(info.teacherId)
            : QVariant()
        );
    query.addBindValue(info.classGrade);
    query.addBindValue(info.classLevel);
    query.addBindValue(info.readingBook);
    query.addBindValue(info.essayBook);
    query.addBindValue(info.classColor);
    query.addBindValue(info.fontColor);
    query.addBindValue(info.notes);
    query.addBindValue(info.timeFillerActivities);

    Status statement = execute(QObject::tr("Saving class information"));
    if (!statement)
    {
        return statement;
    }

    query.prepare(
        "DELETE FROM class_times WHERE class_id=?"
        );

    query.addBindValue(info.classId);
    statement = execute(QObject::tr("Deleting regular class times"));
    if (!statement)
    {
        return statement;
    }

    for (const ClassTime& time : info.classTimes)
    {
        query.prepare(R"(
            INSERT INTO class_times (
                class_id,
                day,
                start_time,
                end_time
            )
            VALUES (?, ?, ?, ?)
        )");

        query.addBindValue(info.classId);
        query.addBindValue(time.day);
        query.addBindValue(time.startTime);
        query.addBindValue(time.endTime);

        statement = execute(QObject::tr("Inserting regular class time"));
        if (!statement)
        {
            return statement;
        }
    }

    query.prepare(
        "DELETE FROM class_intensive_times WHERE class_id=?"
        );

    query.addBindValue(info.classId);
    statement = execute(QObject::tr("Deleting intensive class times"));
    if (!statement)
    {
        return statement;
    }

    for (const ClassTime& time : info.intensiveTimes)
    {
        query.prepare(R"(
            INSERT INTO class_intensive_times (
                class_id,
                day,
                start_time,
                end_time
            )
            VALUES (?, ?, ?, ?)
        )");

        query.addBindValue(info.classId);
        query.addBindValue(time.day);
        query.addBindValue(time.startTime);
        query.addBindValue(time.endTime);

        statement = execute(QObject::tr("Inserting intensive class time"));
        if (!statement)
        {
            return statement;
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Committing class information failed for %1: %2")
                .arg(identity, m_database.lastError().text())
            );
    }

    return {};
}

Status ClassInfoRepository::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Saving class notes failed: invalid class id %1.")
                .arg(classId)
            );
    }

    QSqlQuery query(m_database);

    query.prepare(R"(
        INSERT INTO class_info (
            class_id,
            notes,
            time_filler_activities
        )
        VALUES (?, ?, ?)

        ON CONFLICT(class_id)
        DO UPDATE SET
            notes=excluded.notes,
            time_filler_activities=excluded.time_filler_activities
    )");

    query.addBindValue(classId);
    query.addBindValue(notes);
    query.addBindValue(timeFillerActivities);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Saving class notes"),
        QObject::tr("class id %1").arg(classId)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    return {};
}

Result<ClassInfo> ClassInfoRepository::loadClassInfo(
    int classId
    )
{
    ++m_scheduleClassInfoReadMetrics.singleClassInfoReadCount;
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId)
            );
    }

    ClassInfo info;
    info.classId =    classId;
    info.classColor = "#FFFFFF";
    info.fontColor =  "#000000";

    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT
            ci.*,

            t.teacher_kr,
            t.teacher_en,
            t.preferred_name,
            t.room_number,
            t.wifi_name,
            t.wifi_password,
            t.internet_type,
            t.zoom_id,
            t.zoom_password,
            t.projection_type

        FROM class_info ci

        LEFT JOIN teachers t
        ON ci.teacher_id = t.id

        WHERE ci.class_id = ?
    )");

    query.addBindValue(classId);

    const QString identity = QObject::tr("class id %1").arg(classId);
    const auto loadedInfo = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading class information"),
        identity
        );
    if (!loadedInfo)
    {
        return std::unexpected(loadedInfo.error().userMessage());
    }

    if (query.next())
    {
        const QVariant teacherId = query.value("teacher_id");
        info.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
        info.teacherKr =    query.value("teacher_kr").toString();
        info.teacherEn =    query.value("teacher_en").toString();
        info.teacherPreferredName =
            query.value("preferred_name").toString();
        info.roomNumber =   query.value("room_number").toString();
        info.wifiName =     query.value("wifi_name").toString();
        info.wifiPassword = query.value("wifi_password").toString();
        info.internetType =
            normalizedInternetType(
                query.value("internet_type").toString()
                );
        info.zoomId =       query.value("zoom_id").toString();
        info.zoomPassword = query.value("zoom_password").toString();
        info.projectionType =
            normalizedProjectionType(
                query.value("projection_type").toString()
                );
        info.classGrade =   query.value("class_grade").toString();
        info.classLevel =   query.value("class_level").toString();
        info.readingBook =  query.value("reading_book").toString();
        info.essayBook =    query.value("essay_book").toString();

        const QString classColor =
            query.value("class_color").toString();

        if (!classColor.isEmpty())
        {
            info.classColor = classColor;
        }

        const QString fontColor =
            query.value("font_color").toString();

        if (!fontColor.isEmpty())
        {
            info.fontColor = fontColor;
        }

        info.notes =
            query.value("notes").toString();

        info.timeFillerActivities =
            query.value("time_filler_activities").toString();
    }

    query.prepare(R"(
        SELECT *
        FROM class_times
        WHERE class_id = ?
        ORDER BY id
    )");

    query.addBindValue(classId);

    const auto loadedRegularTimes = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading regular class times"),
        identity
        );
    if (!loadedRegularTimes)
    {
        return std::unexpected(loadedRegularTimes.error().userMessage());
    }

    while (query.next())
    {
        ClassTime time;

        time.day =       query.value("day").toString();
        time.startTime = query.value("start_time").toString();
        time.endTime =   query.value("end_time").toString();

        info.classTimes.append(time);
    }

    query.prepare(R"(
        SELECT *
        FROM class_intensive_times
        WHERE class_id = ?
        ORDER BY id
    )");

    query.addBindValue(classId);

    const auto loadedIntensiveTimes = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading intensive class times"),
        identity
        );
    if (!loadedIntensiveTimes)
    {
        return std::unexpected(loadedIntensiveTimes.error().userMessage());
    }

    while (query.next())
    {
        ClassTime time;

        time.day =       query.value("day").toString();
        time.startTime = query.value("start_time").toString();
        time.endTime =   query.value("end_time").toString();

        info.intensiveTimes.append(time);
    }

    return info;
}

Result<ScheduleEditorClassInfoReadRecord>
ClassInfoRepository::loadScheduleEditorClassInfoRecord(const int classId)
{
    ++m_scheduleEditorClassInfoReadMetrics.callCount;
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId)
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    ScheduleEditorClassInfoReadRecord record;
    record.classId = classId;

    QSqlQuery query(m_database);
    query.prepare(R"(
        WITH requested(class_id) AS (VALUES (?))
        SELECT requested.class_id AS requested_class_id,
               ci.class_grade AS class_grade,
               ci.class_level AS class_level,
               ci.reading_book AS reading_book,
               ci.essay_book AS essay_book,
               ci.class_color AS class_color,
               ci.font_color AS font_color,
               teachers.teacher_kr AS teacher_korean_name,
               teachers.room_number AS room_number
        FROM requested
        LEFT JOIN class_info ci ON ci.class_id = requested.class_id
        LEFT JOIN teachers ON teachers.id = ci.teacher_id
    )");
    query.addBindValue(classId);

    ++m_scheduleEditorClassInfoReadMetrics.statementCount;
    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading class information"),
        identity
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error().userMessage());
    }

    if (query.next())
    {
        record.classId = query.value("requested_class_id").toInt();
        record.classGrade = query.value("class_grade").toString();
        record.classLevel = query.value("class_level").toString();
        record.readingBook = query.value("reading_book").toString();
        record.essayBook = query.value("essay_book").toString();

        const QString classColor = query.value("class_color").toString();
        if (!classColor.isEmpty())
        {
            record.classColor = classColor;
        }

        const QString fontColor = query.value("font_color").toString();
        if (!fontColor.isEmpty())
        {
            record.fontColor = fontColor;
        }

        record.teacherKoreanName =
            query.value("teacher_korean_name").toString();
        record.roomNumber = query.value("room_number").toString();
    }

    return record;
}

Result<SelectedClassGradeReadRecord>
ClassInfoRepository::loadSelectedClassGradeRecord(const int classId)
{
    ++m_selectedClassGradeReadMetrics.callCount;
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId)
            );
    }

    SelectedClassGradeReadRecord record;
    record.classId = classId;

    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT class_grade
        FROM class_info
        WHERE class_id = ?
    )");
    query.addBindValue(classId);

    ++m_selectedClassGradeReadMetrics.statementCount;
    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading class information"),
        QObject::tr("class id %1").arg(classId)
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error().userMessage());
    }

    if (query.next())
    {
        record.classGrade = query.value("class_grade").toString();
    }

    return record;
}

Result<ClassPageDetailsReadRecord> ClassInfoRepository::loadClassPageDetails(
    const int classId
    )
{
    ++m_classPageDetailsReadMetrics.callCount;
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId)
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    ClassPageDetailsReadRecord record;
    record.classId = classId;

    QSqlQuery metadataQuery(m_database);
    metadataQuery.prepare(R"(
        SELECT teacher_id, class_grade, class_level, notes,
               time_filler_activities
        FROM class_info
        WHERE class_id = ?
    )");
    metadataQuery.addBindValue(classId);
    ++m_classPageDetailsReadMetrics.metadataStatementCount;
    const auto loadedMetadata = SqlQueryUtils::executePrepared(
        metadataQuery,
        QObject::tr("Loading class information"),
        identity
        );
    if (!loadedMetadata)
    {
        return std::unexpected(loadedMetadata.error().userMessage());
    }

    if (metadataQuery.next())
    {
        const QVariant teacherId = metadataQuery.value("teacher_id");
        record.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
        record.classGrade = metadataQuery.value("class_grade").toString();
        record.classLevel = metadataQuery.value("class_level").toString();
        record.notes = metadataQuery.value("notes").toString();
        record.timeFillerActivities = metadataQuery.value(
            "time_filler_activities"
            ).toString();
    }

    QSqlQuery regularScheduleQuery(m_database);
    regularScheduleQuery.prepare(R"(
        SELECT day, start_time
        FROM class_times
        WHERE class_id = ?
        ORDER BY id
    )");
    regularScheduleQuery.addBindValue(classId);
    ++m_classPageDetailsReadMetrics.regularScheduleStatementCount;
    const auto loadedRegularSchedule = SqlQueryUtils::executePrepared(
        regularScheduleQuery,
        QObject::tr("Loading regular class times"),
        identity
        );
    if (!loadedRegularSchedule)
    {
        return std::unexpected(loadedRegularSchedule.error().userMessage());
    }

    while (regularScheduleQuery.next())
    {
        record.regularTimes.append({
            regularScheduleQuery.value("day").toString(),
            regularScheduleQuery.value("start_time").toString()
        });
    }

    return record;
}

Result<ClassDetailsPageReadRecord>
ClassInfoRepository::loadClassDetailsPageRecord(const int classId)
{
    ++m_classDetailsPageReadMetrics.callCount;
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId)
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    ClassDetailsPageReadRecord record;
    record.classId = classId;

    QSqlQuery metadataQuery(m_database);
    metadataQuery.prepare(R"(
        SELECT teacher_id, class_grade, class_level,
               reading_book, essay_book, class_color, font_color
        FROM class_info
        WHERE class_id = ?
    )");
    metadataQuery.addBindValue(classId);
    ++m_classDetailsPageReadMetrics.metadataStatementCount;
    const auto loadedMetadata = SqlQueryUtils::executePrepared(
        metadataQuery,
        QObject::tr("Loading class information"),
        identity
        );
    if (!loadedMetadata)
    {
        return std::unexpected(loadedMetadata.error().userMessage());
    }

    if (metadataQuery.next())
    {
        const QVariant teacherId = metadataQuery.value("teacher_id");
        record.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
        record.classGrade = metadataQuery.value("class_grade").toString();
        record.classLevel = metadataQuery.value("class_level").toString();
        record.readingBook = metadataQuery.value("reading_book").toString();
        record.essayBook = metadataQuery.value("essay_book").toString();

        const QString classColor =
            metadataQuery.value("class_color").toString();
        if (!classColor.isEmpty())
        {
            record.classColor = classColor;
        }

        const QString fontColor =
            metadataQuery.value("font_color").toString();
        if (!fontColor.isEmpty())
        {
            record.fontColor = fontColor;
        }
    }

    QSqlQuery scheduleQuery(m_database);
    const QString scheduleStatement = QStringLiteral(R"(
        SELECT schedule_type, id, day, start_time, end_time
        FROM (
            SELECT 0 AS schedule_type, id, day, start_time, end_time
            FROM class_times
            WHERE class_id = ?

            UNION ALL

            SELECT 1 AS schedule_type, id, day, start_time, end_time
            FROM class_intensive_times
            WHERE class_id = ?
        )
        ORDER BY schedule_type, id
    )");
    ++m_classDetailsPageReadMetrics.scheduleStatementCount;
    const auto scheduleErrorMessage = [](
        SqlQueryUtils::ExecutionError error
        )
    {
        const QString errorDetails = error.sqlError.text()
            + QChar(' ') + error.databaseError
            + QChar(' ') + error.driverError;
        if (errorDetails.contains(
                QStringLiteral("class_intensive_times"),
                Qt::CaseInsensitive))
        {
            error.action = QObject::tr("Loading intensive class times");
        }
        return error.userMessage();
    };
    if (!scheduleQuery.prepare(scheduleStatement))
    {
        const SqlQueryUtils::ExecutionError error = SqlQueryUtils::errorFor(
            scheduleQuery,
            QObject::tr("Loading regular class times"),
            scheduleStatement,
            identity
            );
        return std::unexpected(scheduleErrorMessage(error));
    }
    scheduleQuery.addBindValue(classId);
    scheduleQuery.addBindValue(classId);
    const auto loadedSchedules = SqlQueryUtils::executePrepared(
        scheduleQuery,
        QObject::tr("Loading regular class times"),
        identity
        );
    if (!loadedSchedules)
    {
        return std::unexpected(scheduleErrorMessage(loadedSchedules.error()));
    }

    while (scheduleQuery.next())
    {
        ClassTime time;
        time.day = scheduleQuery.value("day").toString();
        time.startTime = scheduleQuery.value("start_time").toString();
        time.endTime = scheduleQuery.value("end_time").toString();
        if (scheduleQuery.value("schedule_type").toInt() == 0)
        {
            record.regularTimes.append(time);
        }
        else
        {
            record.intensiveTimes.append(time);
        }
    }

    return record;
}

Result<QList<ClassInfo>> ClassInfoRepository::loadClassInfoRecords(
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return QList<ClassInfo>{};
    }

    QSet<int> seenClassIds;
    QStringList requestedValues;
    requestedValues.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds[index];
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return std::unexpected(QObject::tr(
                "Loading class information records failed: class identifiers must be positive and unique."
                ));
        }

        seenClassIds.insert(classId);
        requestedValues.append(QStringLiteral("(%1, %2)")
            .arg(classId)
            .arg(index));
    }

    QList<ClassInfo> records;
    records.reserve(classIds.size());
    for (const int classId : classIds)
    {
        ClassInfo info;
        info.classId = classId;
        records.append(std::move(info));
    }

    const QString requestedTable = QStringLiteral(
        "WITH requested(class_id, ordinal) AS (VALUES %1)"
        ).arg(requestedValues.join(QStringLiteral(", ")));

    QSqlQuery metadataQuery(m_database);
    metadataQuery.setForwardOnly(true);
    const auto loadedMetadata = SqlQueryUtils::execute(
        metadataQuery,
        requestedTable + QStringLiteral(R"(
            SELECT requested.class_id AS requested_class_id,
                   requested.ordinal AS requested_ordinal,
                   ci.class_id AS class_info_class_id,
                   ci.teacher_id,
                   teachers.teacher_kr,
                   teachers.teacher_en,
                   teachers.preferred_name,
                   teachers.room_number,
                   teachers.wifi_name,
                   teachers.wifi_password,
                   teachers.internet_type,
                   teachers.zoom_id,
                   teachers.zoom_password,
                   teachers.projection_type,
                   ci.class_grade,
                   ci.class_level,
                   ci.reading_book,
                   ci.essay_book,
                   ci.class_color,
                   ci.font_color,
                   ci.notes,
                   ci.time_filler_activities
            FROM requested
            LEFT JOIN class_info ci ON ci.class_id = requested.class_id
            LEFT JOIN teachers ON teachers.id = ci.teacher_id
            ORDER BY requested.ordinal
        )"),
        QObject::tr("Loading class information record metadata"),
        QObject::tr("%1 classes").arg(classIds.size())
        );
    if (!loadedMetadata)
    {
        return std::unexpected(loadedMetadata.error().userMessage());
    }

    qsizetype metadataCount = 0;
    while (metadataQuery.next())
    {
        const int classId = metadataQuery.value(
            QStringLiteral("requested_class_id")
            ).toInt();
        const int requestOrder = metadataQuery.value(
            QStringLiteral("requested_ordinal")
            ).toInt();
        if (requestOrder < 0
            || requestOrder >= classIds.size()
            || classIds[requestOrder] != classId)
        {
            return std::unexpected(QObject::tr(
                "Loading class information records failed: returned class order did not match the request."
                ));
        }

        ++metadataCount;
        if (metadataQuery.value(
                QStringLiteral("class_info_class_id")
                ).isNull())
        {
            continue;
        }

        ClassInfo& info = records[requestOrder];
        const QVariant teacherId = metadataQuery.value(
            QStringLiteral("teacher_id")
            );
        info.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
        info.teacherKr = metadataQuery.value(
            QStringLiteral("teacher_kr")
            ).toString();
        info.teacherEn = metadataQuery.value(
            QStringLiteral("teacher_en")
            ).toString();
        info.teacherPreferredName = metadataQuery.value(
            QStringLiteral("preferred_name")
            ).toString();
        info.roomNumber = metadataQuery.value(
            QStringLiteral("room_number")
            ).toString();
        info.wifiName = metadataQuery.value(
            QStringLiteral("wifi_name")
            ).toString();
        info.wifiPassword = metadataQuery.value(
            QStringLiteral("wifi_password")
            ).toString();
        info.internetType = normalizedInternetType(
            metadataQuery.value(QStringLiteral("internet_type")).toString()
            );
        info.zoomId = metadataQuery.value(
            QStringLiteral("zoom_id")
            ).toString();
        info.zoomPassword = metadataQuery.value(
            QStringLiteral("zoom_password")
            ).toString();
        info.projectionType = normalizedProjectionType(
            metadataQuery.value(QStringLiteral("projection_type")).toString()
            );
        info.classGrade = metadataQuery.value(
            QStringLiteral("class_grade")
            ).toString();
        info.classLevel = metadataQuery.value(
            QStringLiteral("class_level")
            ).toString();
        info.readingBook = metadataQuery.value(
            QStringLiteral("reading_book")
            ).toString();
        info.essayBook = metadataQuery.value(
            QStringLiteral("essay_book")
            ).toString();

        const QString classColor = metadataQuery.value(
            QStringLiteral("class_color")
            ).toString();
        if (!classColor.isEmpty())
        {
            info.classColor = classColor;
        }

        const QString fontColor = metadataQuery.value(
            QStringLiteral("font_color")
            ).toString();
        if (!fontColor.isEmpty())
        {
            info.fontColor = fontColor;
        }

        info.notes = metadataQuery.value(
            QStringLiteral("notes")
            ).toString();
        info.timeFillerActivities = metadataQuery.value(
            QStringLiteral("time_filler_activities")
            ).toString();
    }
    if (metadataQuery.lastError().isValid())
    {
        return std::unexpected(metadataQuery.lastError().text());
    }
    if (metadataCount != classIds.size())
    {
        return std::unexpected(QObject::tr(
            "Loading class information records failed: metadata returned an incomplete class list."
            ));
    }

    const auto loadTimes = [this, &classIds, &records, &requestedTable](
        const QString& table,
        const QString& action,
        const bool intensive
        ) -> Status
    {
        QSqlQuery scheduleQuery(m_database);
        scheduleQuery.setForwardOnly(true);
        const QString queryText = requestedTable + QStringLiteral(R"(
            SELECT requested.class_id AS requested_class_id,
                   requested.ordinal AS requested_ordinal,
                   schedule.day,
                   schedule.start_time,
                   schedule.end_time
            FROM requested
            INNER JOIN %1 schedule
            ON schedule.class_id = requested.class_id
            ORDER BY requested.ordinal, schedule.id
        )").arg(table);
        const auto loaded = SqlQueryUtils::execute(
            scheduleQuery,
            queryText,
            action,
            QObject::tr("%1 classes").arg(classIds.size())
            );
        if (!loaded)
        {
            return std::unexpected(loaded.error().userMessage());
        }

        while (scheduleQuery.next())
        {
            const int classId = scheduleQuery.value(
                QStringLiteral("requested_class_id")
                ).toInt();
            const int requestOrder = scheduleQuery.value(
                QStringLiteral("requested_ordinal")
                ).toInt();
            if (requestOrder < 0
                || requestOrder >= classIds.size()
                || classIds[requestOrder] != classId)
            {
                return std::unexpected(QObject::tr(
                    "Loading class information records failed: returned schedule order did not match the request."
                    ));
            }

            ClassTime time;
            time.day = scheduleQuery.value(QStringLiteral("day")).toString();
            time.startTime = scheduleQuery.value(
                QStringLiteral("start_time")
                ).toString();
            time.endTime = scheduleQuery.value(
                QStringLiteral("end_time")
                ).toString();
            if (intensive)
            {
                records[requestOrder].intensiveTimes.append(std::move(time));
            }
            else
            {
                records[requestOrder].classTimes.append(std::move(time));
            }
        }

        if (scheduleQuery.lastError().isValid())
        {
            return std::unexpected(scheduleQuery.lastError().text());
        }
        return {};
    };

    if (const Status loadedRegular = loadTimes(
            QStringLiteral("class_times"),
            QObject::tr("Loading regular class information schedules"),
            false
            ); !loadedRegular)
    {
        return std::unexpected(loadedRegular.error());
    }

    if (const Status loadedIntensive = loadTimes(
            QStringLiteral("class_intensive_times"),
            QObject::tr("Loading intensive class information schedules"),
            true
            ); !loadedIntensive)
    {
        return std::unexpected(loadedIntensive.error());
    }

    return records;
}

Result<ClassSubtitleReadRecord> ClassInfoRepository::loadClassSubtitleRecord(
    int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading class subtitle failed: invalid class id %1.")
                .arg(classId)
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    ClassSubtitleReadRecord record;
    record.classId = classId;

    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT teacher_id, class_grade, class_level
        FROM class_info
        WHERE class_id = ?
    )");
    query.addBindValue(classId);

    const auto loadedFields = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading class subtitle details"),
        identity
        );
    if (!loadedFields)
    {
        return std::unexpected(loadedFields.error().userMessage());
    }

    if (query.next())
    {
        const QVariant teacherId = query.value("teacher_id");
        record.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
        record.grade = query.value("class_grade").toString();
        record.level = query.value("class_level").toString();
    }

    query.prepare(R"(
        SELECT day, start_time
        FROM class_times
        WHERE class_id = ?
        ORDER BY id
    )");
    query.addBindValue(classId);

    const auto loadedTimes = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading class subtitle schedule"),
        identity
        );
    if (!loadedTimes)
    {
        return std::unexpected(loadedTimes.error().userMessage());
    }

    while (query.next())
    {
        ClassTime time;
        time.day = query.value("day").toString();
        time.startTime = query.value("start_time").toString();
        time.endTime.clear();
        record.regularTimes.append(std::move(time));
    }

    return record;
}

Result<QList<ClassSubtitleBatchReadRecord>>
ClassInfoRepository::loadClassSubtitleRecords(
    const QList<int>& classIds
    )
{
    ++m_classSubtitleBatchReadMetrics.callCount;
    if (classIds.isEmpty())
    {
        return QList<ClassSubtitleBatchReadRecord>{};
    }

    QSet<int> seenClassIds;
    QStringList requestedValues;
    requestedValues.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds[index];
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return std::unexpected(
                QObject::tr(
                    "Loading class subtitles failed: class identifiers must be positive and unique."
                    )
                );
        }

        seenClassIds.insert(classId);
        requestedValues.append(QStringLiteral("(%1, %2)")
            .arg(classId)
            .arg(index));
    }
    m_classSubtitleBatchReadMetrics.requestedClassCount +=
        static_cast<int>(classIds.size());

    const QString requestedTable = QStringLiteral(
        "WITH requested(class_id, ordinal) AS (VALUES %1)"
        ).arg(requestedValues.join(QStringLiteral(", ")));
    QList<ClassSubtitleBatchReadRecord> records;
    records.reserve(classIds.size());
    QHash<int, qsizetype> indexByClassId;
    indexByClassId.reserve(classIds.size());

    QSqlQuery metadataQuery(m_database);
    ++m_classSubtitleBatchReadMetrics.metadataStatementCount;
    const auto loadedMetadata = SqlQueryUtils::execute(
        metadataQuery,
        requestedTable + QStringLiteral(R"(
            SELECT requested.class_id,
                   ci.class_id AS class_info_class_id,
                   ci.teacher_id,
                   ci.class_grade,
                   ci.class_level
            FROM requested
            LEFT JOIN class_info ci ON ci.class_id = requested.class_id
            ORDER BY requested.ordinal
        )"),
        QObject::tr("Loading class subtitle metadata")
        );
    if (!loadedMetadata)
    {
        return std::unexpected(loadedMetadata.error().userMessage());
    }

    while (metadataQuery.next())
    {
        ClassSubtitleBatchReadRecord record;
        record.classId = metadataQuery.value("class_id").toInt();
        if (!metadataQuery.value("class_info_class_id").isNull())
        {
            const QVariant teacherId = metadataQuery.value("teacher_id");
            record.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
            record.grade = metadataQuery.value("class_grade").toString();
            record.level = metadataQuery.value("class_level").toString();
        }

        indexByClassId.insert(record.classId, records.size());
        records.append(std::move(record));
    }

    if (records.size() != classIds.size())
    {
        return std::unexpected(QObject::tr(
            "Loading class subtitles failed: the metadata query returned an incomplete class list."
            ));
    }

    QSqlQuery scheduleQuery(m_database);
    ++m_classSubtitleBatchReadMetrics.regularScheduleStatementCount;
    const auto loadedSchedule = SqlQueryUtils::execute(
        scheduleQuery,
        requestedTable + QStringLiteral(R"(
            SELECT schedule.class_id, schedule.day, schedule.start_time
            FROM requested
            INNER JOIN class_times schedule
            ON schedule.class_id = requested.class_id
            ORDER BY requested.ordinal, schedule.id
        )"),
        QObject::tr("Loading class subtitle regular schedules")
        );
    if (!loadedSchedule)
    {
        return std::unexpected(loadedSchedule.error().userMessage());
    }

    while (scheduleQuery.next())
    {
        const int classId = scheduleQuery.value("class_id").toInt();
        const auto recordIndex = indexByClassId.constFind(classId);
        if (recordIndex == indexByClassId.cend())
        {
            continue;
        }

        records[*recordIndex].regularTimes.append({
            scheduleQuery.value("day").toString(),
            scheduleQuery.value("start_time").toString(),
            QString{}
        });
    }

    return records;
}

Result<std::vector<MyClassesClassInformationBatchReadEntry>>
ClassInfoRepository::loadMyClassesClassInformationRecords(
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return std::vector<MyClassesClassInformationBatchReadEntry>{};
    }

    ++m_myClassesClassInformationBatchReadMetrics.callCount;
    m_myClassesClassInformationBatchReadMetrics.requestedClassCount +=
        static_cast<int>(classIds.size());

    std::vector<MyClassesClassInformationBatchReadEntry> entries;
    entries.reserve(static_cast<std::size_t>(classIds.size()));
    QHash<int, std::size_t> entryIndexByClassId;
    entryIndexByClassId.reserve(classIds.size());
    QStringList requestedValues;
    requestedValues.reserve(classIds.size());
    std::vector<std::size_t> validEntryIndexes;
    validEntryIndexes.reserve(static_cast<std::size_t>(classIds.size()));

    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds[index];
        if (classId <= 0)
        {
            entries.push_back({
                .classId = classId,
                .information = std::unexpected(
                    QObject::tr(
                        "Loading My Classes class information failed: invalid class id %1."
                        ).arg(classId)
                    )
            });
            continue;
        }

        if (entryIndexByClassId.contains(classId))
        {
            return std::unexpected(QObject::tr(
                "Loading My Classes class information failed: class identifiers must be unique."
                ));
        }

        const std::size_t entryIndex = entries.size();
        entryIndexByClassId.insert(classId, entryIndex);
        validEntryIndexes.push_back(entryIndex);
        entries.push_back({
            .classId = classId,
            .information = std::unexpected(QString{})
        });
        requestedValues.append(QStringLiteral("(%1, %2)")
            .arg(classId)
            .arg(index));
    }

    if (validEntryIndexes.empty())
    {
        return entries;
    }

    const QString requestedTable = QStringLiteral(
        "WITH requested(class_id, ordinal) AS (VALUES %1)"
        ).arg(requestedValues.join(QStringLiteral(", ")));

    const auto loadIndividually = [&]()
    {
        for (const std::size_t index : validEntryIndexes)
        {
            ++m_myClassesClassInformationBatchReadMetrics
                  .fallbackClassReadCount;
            entries[index].information =
                loadMyClassesClassInformationRecord(entries[index].classId);
        }
    };

    QSqlQuery metadataQuery(m_database);
    ++m_myClassesClassInformationBatchReadMetrics.metadataStatementCount;
    const auto loadedMetadata = SqlQueryUtils::execute(
        metadataQuery,
        requestedTable + QStringLiteral(R"(
            SELECT requested.class_id,
                   ci.class_id AS class_info_class_id,
                   ci.teacher_id,
                   ci.class_grade,
                   ci.class_level,
                   ci.notes,
                   ci.time_filler_activities
            FROM requested
            LEFT JOIN class_info ci ON ci.class_id = requested.class_id
            ORDER BY requested.ordinal
        )"),
        QObject::tr("Loading My Classes class information metadata")
        );
    if (!loadedMetadata)
    {
        loadIndividually();
        return entries;
    }

    bool invalidReturnedClassId = false;
    std::size_t loadedMetadataCount = 0;
    while (metadataQuery.next())
    {
        const int classId = metadataQuery.value("class_id").toInt();
        const auto entryIndex = entryIndexByClassId.constFind(classId);
        if (entryIndex == entryIndexByClassId.cend())
        {
            invalidReturnedClassId = true;
            break;
        }

        MyClassesClassInformationReadRecord record;
        record.classId = classId;
        if (!metadataQuery.value("class_info_class_id").isNull())
        {
            const QVariant teacherId = metadataQuery.value("teacher_id");
            record.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
            record.classGrade = metadataQuery.value("class_grade").toString();
            record.classLevel = metadataQuery.value("class_level").toString();
            record.notes = metadataQuery.value("notes").toString();
            record.timeFillerActivities = metadataQuery.value(
                "time_filler_activities"
                ).toString();
        }
        entries[*entryIndex].information = std::move(record);
        ++loadedMetadataCount;
    }

    if (invalidReturnedClassId
        || loadedMetadataCount != validEntryIndexes.size())
    {
        loadIndividually();
        return entries;
    }

    QSqlQuery regularScheduleQuery(m_database);
    ++m_myClassesClassInformationBatchReadMetrics
          .regularScheduleStatementCount;
    const auto loadedRegularSchedule = SqlQueryUtils::execute(
        regularScheduleQuery,
        requestedTable + QStringLiteral(R"(
            SELECT schedule.class_id,
                   schedule.day,
                   schedule.start_time,
                   schedule.end_time
            FROM requested
            INNER JOIN class_times schedule
            ON schedule.class_id = requested.class_id
            ORDER BY requested.ordinal, schedule.id
        )"),
        QObject::tr("Loading My Classes regular schedules")
        );
    if (!loadedRegularSchedule)
    {
        loadIndividually();
        return entries;
    }

    invalidReturnedClassId = false;
    while (regularScheduleQuery.next())
    {
        const int classId = regularScheduleQuery.value("class_id").toInt();
        const auto entryIndex = entryIndexByClassId.constFind(classId);
        if (entryIndex == entryIndexByClassId.cend())
        {
            invalidReturnedClassId = true;
            break;
        }

        ClassTime time;
        time.day = regularScheduleQuery.value("day").toString();
        time.startTime = regularScheduleQuery.value("start_time").toString();
        time.endTime = regularScheduleQuery.value("end_time").toString();
        entries[*entryIndex].information->regularTimes.append(std::move(time));
    }
    if (invalidReturnedClassId)
    {
        loadIndividually();
        return entries;
    }

    QSqlQuery intensiveScheduleQuery(m_database);
    ++m_myClassesClassInformationBatchReadMetrics
          .intensiveScheduleStatementCount;
    const auto loadedIntensiveSchedule = SqlQueryUtils::execute(
        intensiveScheduleQuery,
        requestedTable + QStringLiteral(R"(
            SELECT schedule.class_id,
                   schedule.day,
                   schedule.start_time,
                   schedule.end_time
            FROM requested
            INNER JOIN class_intensive_times schedule
            ON schedule.class_id = requested.class_id
            ORDER BY requested.ordinal, schedule.id
        )"),
        QObject::tr("Loading My Classes intensive schedules")
        );
    if (!loadedIntensiveSchedule)
    {
        loadIndividually();
        return entries;
    }

    while (intensiveScheduleQuery.next())
    {
        const int classId = intensiveScheduleQuery.value("class_id").toInt();
        const auto entryIndex = entryIndexByClassId.constFind(classId);
        if (entryIndex == entryIndexByClassId.cend())
        {
            invalidReturnedClassId = true;
            break;
        }

        ClassTime time;
        time.day = intensiveScheduleQuery.value("day").toString();
        time.startTime = intensiveScheduleQuery.value("start_time").toString();
        time.endTime = intensiveScheduleQuery.value("end_time").toString();
        entries[*entryIndex].information->intensiveTimes.append(
            std::move(time)
            );
    }
    if (invalidReturnedClassId)
    {
        loadIndividually();
        return entries;
    }

    return entries;
}

Result<MyClassesClassInformationReadRecord>
ClassInfoRepository::loadMyClassesClassInformationRecord(
    const int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr(
                "Loading My Classes class information failed: invalid class id %1."
                ).arg(classId)
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    MyClassesClassInformationReadRecord record;
    record.classId = classId;

    QSqlQuery metadataQuery(m_database);
    metadataQuery.prepare(R"(
        SELECT teacher_id, class_grade, class_level, notes,
               time_filler_activities
        FROM class_info
        WHERE class_id = ?
    )");
    metadataQuery.addBindValue(classId);
    const auto loadedMetadata = SqlQueryUtils::executePrepared(
        metadataQuery,
        QObject::tr("Loading My Classes class information metadata"),
        identity
        );
    if (!loadedMetadata)
    {
        return std::unexpected(loadedMetadata.error().userMessage());
    }

    if (metadataQuery.next())
    {
        const QVariant teacherId = metadataQuery.value("teacher_id");
        record.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
        record.classGrade = metadataQuery.value("class_grade").toString();
        record.classLevel = metadataQuery.value("class_level").toString();
        record.notes = metadataQuery.value("notes").toString();
        record.timeFillerActivities = metadataQuery.value(
            "time_filler_activities"
            ).toString();
    }

    QSqlQuery regularScheduleQuery(m_database);
    regularScheduleQuery.prepare(R"(
        SELECT day, start_time, end_time
        FROM class_times
        WHERE class_id = ?
        ORDER BY id
    )");
    regularScheduleQuery.addBindValue(classId);
    const auto loadedRegularSchedule = SqlQueryUtils::executePrepared(
        regularScheduleQuery,
        QObject::tr("Loading My Classes regular schedules"),
        identity
        );
    if (!loadedRegularSchedule)
    {
        return std::unexpected(loadedRegularSchedule.error().userMessage());
    }
    while (regularScheduleQuery.next())
    {
        record.regularTimes.append({
            regularScheduleQuery.value("day").toString(),
            regularScheduleQuery.value("start_time").toString(),
            regularScheduleQuery.value("end_time").toString()
        });
    }

    QSqlQuery intensiveScheduleQuery(m_database);
    intensiveScheduleQuery.prepare(R"(
        SELECT day, start_time, end_time
        FROM class_intensive_times
        WHERE class_id = ?
        ORDER BY id
    )");
    intensiveScheduleQuery.addBindValue(classId);
    const auto loadedIntensiveSchedule = SqlQueryUtils::executePrepared(
        intensiveScheduleQuery,
        QObject::tr("Loading My Classes intensive schedules"),
        identity
        );
    if (!loadedIntensiveSchedule)
    {
        return std::unexpected(loadedIntensiveSchedule.error().userMessage());
    }
    while (intensiveScheduleQuery.next())
    {
        record.intensiveTimes.append({
            intensiveScheduleQuery.value("day").toString(),
            intensiveScheduleQuery.value("start_time").toString(),
            intensiveScheduleQuery.value("end_time").toString()
        });
    }

    return record;
}

Result<RosterPrintClassInfoReadRecord>
ClassInfoRepository::loadRosterPrintClassInfoRecord(
    int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading roster print class information failed: invalid class id %1.")
                .arg(classId)
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    RosterPrintClassInfoReadRecord record;
    record.classId = classId;

    QSqlQuery metadataQuery(m_database);
    metadataQuery.prepare(R"(
        SELECT
            ci.class_grade,
            ci.class_level,
            t.teacher_en,
            t.teacher_kr,
            t.room_number,
            t.wifi_name,
            t.wifi_password,
            t.zoom_id,
            t.zoom_password
        FROM class_info ci
        LEFT JOIN teachers t ON ci.teacher_id = t.id
        WHERE ci.class_id = ?
    )");
    metadataQuery.addBindValue(classId);

    const auto loadedMetadata = SqlQueryUtils::executePrepared(
        metadataQuery,
        QObject::tr("Loading roster print class information"),
        identity
        );
    if (!loadedMetadata)
    {
        return std::unexpected(loadedMetadata.error().userMessage());
    }

    if (metadataQuery.next())
    {
        record.classGrade =
            metadataQuery.value("class_grade").toString();
        record.classLevel =
            metadataQuery.value("class_level").toString();
        record.teacherEnglishName =
            metadataQuery.value("teacher_en").toString();
        record.teacherKoreanName =
            metadataQuery.value("teacher_kr").toString();
        record.roomNumber =
            metadataQuery.value("room_number").toString();
        record.wifiName =
            metadataQuery.value("wifi_name").toString();
        record.wifiPassword =
            metadataQuery.value("wifi_password").toString();
        record.zoomId =
            metadataQuery.value("zoom_id").toString();
        record.zoomPassword =
            metadataQuery.value("zoom_password").toString();
    }

    QSqlQuery scheduleQuery(m_database);
    scheduleQuery.prepare(R"(
        SELECT day, start_time, end_time
        FROM class_times
        WHERE class_id = ?
        ORDER BY id
    )");
    scheduleQuery.addBindValue(classId);

    const auto loadedSchedule = SqlQueryUtils::executePrepared(
        scheduleQuery,
        QObject::tr("Loading roster print regular class times"),
        identity
        );
    if (!loadedSchedule)
    {
        return std::unexpected(loadedSchedule.error().userMessage());
    }

    while (scheduleQuery.next())
    {
        record.regularTimes.append({
            scheduleQuery.value("day").toString(),
            scheduleQuery.value("start_time").toString(),
            scheduleQuery.value("end_time").toString()
        });
    }

    return record;
}

Result<QList<RosterPrintClassInfoReadRecord>>
ClassInfoRepository::loadRosterPrintClassInfoRecords(
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return QList<RosterPrintClassInfoReadRecord>{};
    }

    ++m_rosterPrintClassInfoBatchReadMetrics.callCount;
    m_rosterPrintClassInfoBatchReadMetrics.requestedClassCount +=
        classIds.size();

    QHash<int, qsizetype> requestIndexByClassId;
    requestIndexByClassId.reserve(classIds.size());
    QStringList requestedValues;
    requestedValues.reserve(classIds.size());
    QList<RosterPrintClassInfoReadRecord> records;
    records.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds.at(index);
        if (classId <= 0 || requestIndexByClassId.contains(classId))
        {
            return std::unexpected(QObject::tr(
                "Loading roster print class information failed: class ids must be positive and unique."
                ));
        }

        requestIndexByClassId.insert(classId, index);
        requestedValues.append(
            QStringLiteral("(%1, %2)").arg(classId).arg(index)
            );
        RosterPrintClassInfoReadRecord record;
        record.classId = classId;
        records.append(std::move(record));
    }

    QStringList metadataRowsSeen;
    metadataRowsSeen.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        metadataRowsSeen.append(QStringLiteral("0"));
    }

    QSqlQuery metadataQuery(m_database);
    metadataQuery.setForwardOnly(true);
    ++m_rosterPrintClassInfoBatchReadMetrics.metadataStatementCount;
    const QString metadataQueryText = QStringLiteral(R"(
        WITH requested(class_id, request_order) AS (VALUES %1)
        SELECT requested.class_id AS requested_class_id,
               requested.request_order AS request_order,
               ci.class_id AS class_info_class_id,
               ci.class_grade AS class_grade,
               ci.class_level AS class_level,
               teachers.teacher_en AS teacher_en,
               teachers.teacher_kr AS teacher_kr,
               teachers.room_number AS room_number,
               teachers.wifi_name AS wifi_name,
               teachers.wifi_password AS wifi_password,
               teachers.zoom_id AS zoom_id,
               teachers.zoom_password AS zoom_password
        FROM requested
        LEFT JOIN class_info ci ON ci.class_id=requested.class_id
        LEFT JOIN teachers ON teachers.id=ci.teacher_id
        ORDER BY requested.request_order
    )").arg(requestedValues.join(QStringLiteral(", ")));
    const auto loadedMetadata = SqlQueryUtils::execute(
        metadataQuery,
        metadataQueryText,
        QObject::tr("Loading roster print class information metadata"),
        QObject::tr("%1 classes").arg(classIds.size())
        );
    if (!loadedMetadata)
    {
        return std::unexpected(loadedMetadata.error().userMessage());
    }

    bool invalidMetadataIdentity = false;
    while (metadataQuery.next())
    {
        const int classId = metadataQuery.value("requested_class_id").toInt();
        const int requestOrder = metadataQuery.value("request_order").toInt();
        if (requestOrder < 0
            || requestOrder >= classIds.size()
            || classIds.at(requestOrder) != classId
            || !requestIndexByClassId.contains(classId)
            || metadataRowsSeen[requestOrder] == QStringLiteral("1"))
        {
            invalidMetadataIdentity = true;
            break;
        }
        metadataRowsSeen[requestOrder] = QStringLiteral("1");

        if (metadataQuery.value("class_info_class_id").isNull())
        {
            continue;
        }
        if (metadataQuery.value("class_info_class_id").toInt() != classId)
        {
            invalidMetadataIdentity = true;
            break;
        }

        RosterPrintClassInfoReadRecord& record = records[requestOrder];
        record.classGrade = metadataQuery.value("class_grade").toString();
        record.classLevel = metadataQuery.value("class_level").toString();
        record.teacherEnglishName = metadataQuery.value("teacher_en").toString();
        record.teacherKoreanName = metadataQuery.value("teacher_kr").toString();
        record.roomNumber = metadataQuery.value("room_number").toString();
        record.wifiName = metadataQuery.value("wifi_name").toString();
        record.wifiPassword = metadataQuery.value("wifi_password").toString();
        record.zoomId = metadataQuery.value("zoom_id").toString();
        record.zoomPassword = metadataQuery.value("zoom_password").toString();
    }
    if (metadataQuery.lastError().isValid())
    {
        return std::unexpected(
            QObject::tr("Reading roster print class information metadata failed: %1")
                .arg(metadataQuery.lastError().text())
            );
    }
    for (const QString& seen : metadataRowsSeen)
    {
        if (seen != QStringLiteral("1"))
        {
            invalidMetadataIdentity = true;
            break;
        }
    }
    if (invalidMetadataIdentity)
    {
        return std::unexpected(QObject::tr(
            "Loading roster print class information returned an invalid class identity."
            ));
    }

    QSqlQuery scheduleQuery(m_database);
    scheduleQuery.setForwardOnly(true);
    ++m_rosterPrintClassInfoBatchReadMetrics.regularScheduleStatementCount;
    const QString scheduleQueryText = QStringLiteral(R"(
        WITH requested(class_id, request_order) AS (VALUES %1)
        SELECT requested.class_id AS requested_class_id,
               requested.request_order AS request_order,
               schedule.day AS day,
               schedule.start_time AS start_time,
               schedule.end_time AS end_time
        FROM requested
        INNER JOIN class_times schedule
            ON schedule.class_id=requested.class_id
        ORDER BY requested.request_order, schedule.id
    )").arg(requestedValues.join(QStringLiteral(", ")));
    const auto loadedSchedule = SqlQueryUtils::execute(
        scheduleQuery,
        scheduleQueryText,
        QObject::tr("Loading roster print regular schedules"),
        QObject::tr("%1 classes").arg(classIds.size())
        );
    if (!loadedSchedule)
    {
        return std::unexpected(loadedSchedule.error().userMessage());
    }

    bool invalidScheduleIdentity = false;
    while (scheduleQuery.next())
    {
        const int classId = scheduleQuery.value("requested_class_id").toInt();
        const int requestOrder = scheduleQuery.value("request_order").toInt();
        if (requestOrder < 0
            || requestOrder >= classIds.size()
            || classIds.at(requestOrder) != classId
            || !requestIndexByClassId.contains(classId))
        {
            invalidScheduleIdentity = true;
            break;
        }

        records[requestOrder].regularTimes.append({
            scheduleQuery.value("day").toString(),
            scheduleQuery.value("start_time").toString(),
            scheduleQuery.value("end_time").toString()
        });
    }
    if (scheduleQuery.lastError().isValid())
    {
        return std::unexpected(
            QObject::tr("Reading roster print regular schedules failed: %1")
                .arg(scheduleQuery.lastError().text())
            );
    }
    if (invalidScheduleIdentity)
    {
        return std::unexpected(QObject::tr(
            "Loading roster print regular schedules returned an invalid class identity."
            ));
    }

    return records;
}

Result<QList<SubPrepRosterOutputClassInfoReadRecord>>
ClassInfoRepository::loadSubPrepRosterOutputClassInfoRecords(
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return QList<SubPrepRosterOutputClassInfoReadRecord>{};
    }

    ++m_subPrepRosterOutputClassInfoBatchReadMetrics.callCount;
    m_subPrepRosterOutputClassInfoBatchReadMetrics.requestedClassCount +=
        classIds.size();

    QSet<int> seenClassIds;
    QStringList requestedValues;
    requestedValues.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds[index];
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep roster output class information failed: class identifiers must be positive and unique."
                ));
        }

        seenClassIds.insert(classId);
        requestedValues.append(QStringLiteral("(%1, %2)")
            .arg(classId)
            .arg(index));
    }

    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    ++m_subPrepRosterOutputClassInfoBatchReadMetrics.statementCount;
    const auto loaded = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            WITH requested(class_id, ordinal) AS (VALUES %1)
            SELECT requested.class_id AS requested_class_id,
                   ci.class_id AS class_info_class_id,
                   ci.teacher_id,
                   ci.class_grade,
                   ci.class_level,
                   teachers.teacher_en,
                   teachers.teacher_kr,
                   teachers.room_number,
                   teachers.wifi_name,
                   teachers.wifi_password,
                   teachers.zoom_id,
                   teachers.zoom_password
            FROM requested
            LEFT JOIN class_info ci ON ci.class_id = requested.class_id
            LEFT JOIN teachers ON teachers.id = ci.teacher_id
            ORDER BY requested.ordinal
        )").arg(requestedValues.join(QStringLiteral(", "))),
        QObject::tr("Loading Sub Prep roster output class information"),
        QObject::tr("class ids %1").arg(requestedValues.join(
            QStringLiteral(", ")
            ))
        );
    if (!loaded)
    {
        return std::unexpected(loaded.error().userMessage());
    }

    QList<SubPrepRosterOutputClassInfoReadRecord> records;
    records.reserve(classIds.size());
    while (query.next())
    {
        const int requestedClassId =
            query.value(QStringLiteral("requested_class_id")).toInt();
        if (records.size() >= classIds.size()
            || requestedClassId != classIds[records.size()])
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep roster output class information failed: returned class order did not match the request."
                ));
        }

        SubPrepRosterOutputClassInfoReadRecord record;
        const QVariant classInfoClassId = query.value(
            QStringLiteral("class_info_class_id")
            );
        if (classInfoClassId.isNull())
        {
            return std::unexpected(
                QObject::tr(
                    "Loading Sub Prep roster output class information failed for class id %1: no matching record exists."
                    ).arg(requestedClassId)
                );
        }

        record.classId = classInfoClassId.toInt();
        if (record.classId != requestedClassId)
        {
            return std::unexpected(QObject::tr(
                "Loading Sub Prep roster output class information failed: returned class identity did not match the request."
                ));
        }

        const QVariant teacherId = query.value(
            QStringLiteral("teacher_id")
            );
        record.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
        record.classGrade = query.value(
            QStringLiteral("class_grade")
            ).toString();
        record.classLevel = query.value(
            QStringLiteral("class_level")
            ).toString();
        record.teacherEnglishName = query.value(
            QStringLiteral("teacher_en")
            ).toString();
        record.teacherKoreanName = query.value(
            QStringLiteral("teacher_kr")
            ).toString();
        record.roomNumber = query.value(
            QStringLiteral("room_number")
            ).toString();
        record.wifiName = query.value(
            QStringLiteral("wifi_name")
            ).toString();
        record.wifiPassword = query.value(
            QStringLiteral("wifi_password")
            ).toString();
        record.zoomId = query.value(
            QStringLiteral("zoom_id")
            ).toString();
        record.zoomPassword = query.value(
            QStringLiteral("zoom_password")
            ).toString();
        records.append(std::move(record));
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(query.lastError().text());
    }
    if (records.size() != classIds.size())
    {
        return std::unexpected(QObject::tr(
            "Loading Sub Prep roster output class information failed: the database returned an incomplete result."
            ));
    }

    return records;
}

Result<QList<ClassNavigationReadRecord>>
ClassInfoRepository::loadClassesNavigationRecords(
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return QList<ClassNavigationReadRecord>{};
    }

    QSet<int> seenClassIds;
    QStringList requestedValues;
    requestedValues.reserve(classIds.size());
    for (qsizetype index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds[index];
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return std::unexpected(
                QObject::tr(
                    "Loading classes navigation failed: class identifiers must be positive and unique."
                    )
                );
        }

        seenClassIds.insert(classId);
        const QString value = QString::number(classId);
        requestedValues.append(
            QStringLiteral("(%1, %2)").arg(value).arg(index)
            );
    }

    const QString metadataQueryText = QStringLiteral(R"(
        WITH requested(class_id, ordinal) AS (
            VALUES %1
        )
        SELECT
            requested.class_id,
            ci.class_id AS class_info_class_id,
            ci.teacher_id,
            ci.class_grade,
            ci.class_level,
            t.teacher_en,
            t.teacher_kr
        FROM requested
        LEFT JOIN class_info ci
        ON ci.class_id = requested.class_id
        LEFT JOIN teachers t
        ON t.id = ci.teacher_id
        ORDER BY requested.ordinal
    )").arg(requestedValues.join(QStringLiteral(", ")));

    QList<ClassNavigationReadRecord> records;
    records.reserve(classIds.size());
    QHash<int, qsizetype> indexByClassId;
    indexByClassId.reserve(classIds.size());
    QSqlQuery metadataQuery(m_database);
    ++m_classesNavigationReadMetrics.metadataStatementCount;
    const auto loadedMetadata = SqlQueryUtils::execute(
        metadataQuery,
        metadataQueryText,
        QObject::tr("Loading classes navigation metadata")
        );
    if (!loadedMetadata)
    {
        return std::unexpected(loadedMetadata.error().userMessage());
    }

    while (metadataQuery.next())
    {
        ClassNavigationReadRecord record;
        record.classId = metadataQuery.value("class_id").toInt();
        record.hasClassInfo =
            !metadataQuery.value("class_info_class_id").isNull();
        if (record.hasClassInfo)
        {
            const QVariant teacherId = metadataQuery.value("teacher_id");
            record.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
            record.grade = metadataQuery.value("class_grade").toString();
            record.level = metadataQuery.value("class_level").toString();
            record.teacherEnglishName =
                metadataQuery.value("teacher_en").toString();
            record.teacherKoreanName =
                metadataQuery.value("teacher_kr").toString();
        }

        indexByClassId.insert(record.classId, records.size());
        records.append(std::move(record));
    }

    if (records.size() != classIds.size())
    {
        return std::unexpected(
            QObject::tr(
                "Loading classes navigation failed: the metadata query returned an incomplete class list."
                )
            );
    }

    const auto loadTimes = [this, &indexByClassId, &records, &requestedValues](
        const QString& table,
        const QString& action,
        const bool intensive
        ) -> Status
    {
        QSqlQuery timesQuery(m_database);
        if (intensive)
        {
            ++m_classesNavigationReadMetrics.intensiveScheduleStatementCount;
        }
        else
        {
            ++m_classesNavigationReadMetrics.regularScheduleStatementCount;
        }
        const QString queryText = QStringLiteral(R"(
            WITH requested(class_id, ordinal) AS (
                VALUES %1
            )
            SELECT schedule.class_id, schedule.day,
                   schedule.start_time, schedule.end_time
            FROM requested
            INNER JOIN %2 schedule
            ON schedule.class_id = requested.class_id
            ORDER BY requested.ordinal, schedule.id
        )").arg(requestedValues.join(QStringLiteral(", ")), table);
        const auto loadedTimes = SqlQueryUtils::execute(
            timesQuery,
            queryText,
            action
            );
        if (!loadedTimes)
        {
            return std::unexpected(loadedTimes.error().userMessage());
        }

        while (timesQuery.next())
        {
            const int classId = timesQuery.value("class_id").toInt();
            const auto recordIndex = indexByClassId.constFind(classId);
            if (recordIndex == indexByClassId.cend())
            {
                continue;
            }

            ClassNavigationReadRecord& record = records[*recordIndex];

            ClassTime time;
            time.day = timesQuery.value("day").toString();
            time.startTime = timesQuery.value("start_time").toString();
            time.endTime = timesQuery.value("end_time").toString();
            if (intensive)
            {
                record.intensiveTimes.append(std::move(time));
            }
            else
            {
                record.regularTimes.append(std::move(time));
            }
        }

        return {};
    };

    if (const Status loadedRegular = loadTimes(
            QStringLiteral("class_times"),
            QObject::tr("Loading regular classes navigation schedules"),
            false
            ); !loadedRegular)
    {
        return std::unexpected(loadedRegular.error());
    }

    if (const Status loadedIntensive = loadTimes(
            QStringLiteral("class_intensive_times"),
            QObject::tr("Loading intensive classes navigation schedules"),
            true
            ); !loadedIntensive)
    {
        return std::unexpected(loadedIntensive.error());
    }

    return records;
}

const ClassesNavigationReadMetrics&
ClassInfoRepository::classesNavigationReadMetrics() const noexcept
{
    return m_classesNavigationReadMetrics;
}

const ScheduleClassInfoReadMetrics&
ClassInfoRepository::scheduleClassInfoReadMetrics() const noexcept
{
    return m_scheduleClassInfoReadMetrics;
}

const ScheduleImportStateSnapshotClassInfoReadMetrics&
ClassInfoRepository::scheduleImportStateSnapshotClassInfoReadMetrics() const noexcept
{
    return m_scheduleImportStateSnapshotClassInfoReadMetrics;
}

const ClassPageDetailsReadMetrics&
ClassInfoRepository::classPageDetailsReadMetrics() const noexcept
{
    return m_classPageDetailsReadMetrics;
}

const ClassDetailsPageReadMetrics&
ClassInfoRepository::classDetailsPageReadMetrics() const noexcept
{
    return m_classDetailsPageReadMetrics;
}

const ScheduleEditorClassInfoReadMetrics&
ClassInfoRepository::scheduleEditorClassInfoReadMetrics() const noexcept
{
    return m_scheduleEditorClassInfoReadMetrics;
}

const SelectedClassGradeReadMetrics&
ClassInfoRepository::selectedClassGradeReadMetrics() const noexcept
{
    return m_selectedClassGradeReadMetrics;
}

const ClassSubtitleBatchReadMetrics&
ClassInfoRepository::classSubtitleBatchReadMetrics() const noexcept
{
    return m_classSubtitleBatchReadMetrics;
}

const MyClassesClassInformationBatchReadMetrics&
ClassInfoRepository::myClassesClassInformationBatchReadMetrics() const noexcept
{
    return m_myClassesClassInformationBatchReadMetrics;
}

const SubPrepRosterOutputClassInfoBatchReadMetrics&
ClassInfoRepository::subPrepRosterOutputClassInfoBatchReadMetrics() const noexcept
{
    return m_subPrepRosterOutputClassInfoBatchReadMetrics;
}

const SubPrepRosterOutputScheduleBatchReadMetrics&
ClassInfoRepository::subPrepRosterOutputScheduleBatchReadMetrics() const noexcept
{
    return m_subPrepRosterOutputScheduleBatchReadMetrics;
}

const RosterPrintClassInfoBatchReadMetrics&
ClassInfoRepository::rosterPrintClassInfoBatchReadMetrics() const noexcept
{
    return m_rosterPrintClassInfoBatchReadMetrics;
}

Result<SubPrepClassDetailsRecord>
ClassInfoRepository::loadSubPrepClassDetails(
    const int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr(
                "Loading selected Sub Prep class details failed: invalid class id %1."
                ).arg(classId)
            );
    }

    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    query.prepare(QStringLiteral(R"(
        SELECT
            c.id AS class_id,
            ci.teacher_id AS assigned_teacher_id,
            ci.notes AS class_notes,
            t.id AS resolved_teacher_id,
            t.teacher_kr,
            t.teacher_en,
            t.preferred_romanization,
            t.preferred_name,
            t.room_number,
            t.wifi_name,
            t.wifi_password,
            t.internet_type,
            t.zoom_id,
            t.zoom_password,
            t.projection_type,
            t.notes AS teacher_notes
        FROM classes c
        LEFT JOIN class_info ci
        ON ci.class_id = c.id
        LEFT JOIN teachers t
        ON t.id = ci.teacher_id
        WHERE c.id = ?
        LIMIT 1
    )"));
    query.addBindValue(classId);

    const QString identity = QObject::tr("class id %1").arg(classId);
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading selected Sub Prep class details"),
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    if (!query.next())
    {
        return std::unexpected(
            QObject::tr(
                "Loading selected Sub Prep class details failed: no matching record exists for class id %1."
                ).arg(classId)
            );
    }

    SubPrepClassDetailsRecord record;
    record.classId = query.value(QStringLiteral("class_id")).toInt();
    record.classNotes = query.value(QStringLiteral("class_notes")).toString();

    // A missing assignment, or a stale assignment whose teacher row no longer
    // exists, follows the legacy empty-teacher details fallback.
    const QVariant assignedTeacherId =
        query.value(QStringLiteral("assigned_teacher_id"));
    const QVariant resolvedTeacherId =
        query.value(QStringLiteral("resolved_teacher_id"));
    bool assignedTeacherIdOk = false;
    bool resolvedTeacherIdOk = false;
    const int assignedTeacherIdValue = assignedTeacherId.toInt(
        &assignedTeacherIdOk
        );
    const int resolvedTeacherIdValue = resolvedTeacherId.toInt(
        &resolvedTeacherIdOk
        );
    if (assignedTeacherIdOk && resolvedTeacherIdOk
        && assignedTeacherIdValue > 0
        && assignedTeacherIdValue == resolvedTeacherIdValue)
    {
        record.teacherId = assignedTeacherIdValue;
        record.teacherKr = query.value(QStringLiteral("teacher_kr")).toString();
        record.teacherEn = query.value(QStringLiteral("teacher_en")).toString();
        record.teacherPreferredRomanization = query.value(
            QStringLiteral("preferred_romanization")
            ).toString();
        record.teacherPreferredName = query.value(
            QStringLiteral("preferred_name")
            ).toString();
        record.roomNumber = query.value(QStringLiteral("room_number")).toString();
        record.wifiName = query.value(QStringLiteral("wifi_name")).toString();
        record.wifiPassword = query.value(
            QStringLiteral("wifi_password")
            ).toString();
        record.internetType = normalizedInternetType(
            query.value(QStringLiteral("internet_type")).toString()
            );
        record.zoomId = query.value(QStringLiteral("zoom_id")).toString();
        record.zoomPassword = query.value(
            QStringLiteral("zoom_password")
            ).toString();
        record.projectionType = normalizedProjectionType(
            query.value(QStringLiteral("projection_type")).toString()
            );
        record.teacherNotes = query.value(
            QStringLiteral("teacher_notes")
            ).toString();
    }

    return record;
}

Result<QList<SubPrepClassSummaryRecord>>
ClassInfoRepository::loadSubPrepClassSummaries(
    const QList<int>& classIds,
    const QStringList& selectedDays,
    const ScheduleType type,
    const int maxMeetingsPerClass,
    const int maxTotalMeetings
    )
{
    if (classIds.isEmpty() || selectedDays.isEmpty())
    {
        return QList<SubPrepClassSummaryRecord>{};
    }

    constexpr qsizetype MaxClassIds = 4'096;
    if (classIds.size() > MaxClassIds)
    {
        return std::unexpected(
            QObject::tr("Loading Sub Prep class summaries failed: class scope exceeds its limit.")
            );
    }

    if (type != ScheduleType::Regular && type != ScheduleType::Intensive)
    {
        return std::unexpected(
            QObject::tr("Loading Sub Prep class summaries failed: invalid schedule type.")
            );
    }

    if (maxMeetingsPerClass < 0
        || maxMeetingsPerClass == std::numeric_limits<int>::max()
        || maxTotalMeetings < 0
        || maxTotalMeetings == std::numeric_limits<int>::max())
    {
        return std::unexpected(
            QObject::tr("Loading Sub Prep class summaries failed: invalid meeting limit.")
            );
    }

    QSet<int> seenClassIds;
    QStringList classIdValues;
    classIdValues.reserve(classIds.size());
    QStringList requestedIds;
    requestedIds.reserve(classIds.size());
    for (const int classId : classIds)
    {
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return std::unexpected(
                QObject::tr(
                    "Loading Sub Prep class summaries failed: class identifiers must be positive and unique."
                    )
                );
        }
        seenClassIds.insert(classId);
        const QString value = QString::number(classId);
        classIdValues.append(value);
        requestedIds.append(value);
    }

    static const QStringList validDays{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
    if (selectedDays.size() > validDays.size())
    {
        return std::unexpected(
            QObject::tr("Loading Sub Prep class summaries failed: invalid weekday scope.")
            );
    }

    QSet<QString> seenDays;
    for (const QString& day : selectedDays)
    {
        if (!validDays.contains(day) || seenDays.contains(day))
        {
            return std::unexpected(
                QObject::tr(
                    "Loading Sub Prep class summaries failed: weekdays must be valid and unique."
                    )
                );
        }
        seenDays.insert(day);
    }

    QStringList dayPlaceholders;
    dayPlaceholders.fill(QStringLiteral("?"), selectedDays.size());
    const QString timesTable = type == ScheduleType::Regular
        ? QStringLiteral("class_times")
        : QStringLiteral("class_intensive_times");

    // Class IDs are validated positive integers and formatted as decimal SQL
    // literals so the maximum 4,096-class scope fits older SQLite bind limits.
    // Day labels and meeting sentinels remain bound parameters.
    const QString queryText = QStringLiteral(R"(
        WITH scoped_classes AS (
            SELECT
                c.id AS class_id,
                ci.teacher_id,
                ci.class_grade,
                ci.class_level,
                t.teacher_kr,
                t.teacher_en,
                t.preferred_romanization,
                t.preferred_name,
                t.room_number,
                t.wifi_name,
                t.wifi_password,
                t.internet_type,
                t.zoom_id,
                t.zoom_password,
                t.projection_type,
                t.notes AS teacher_notes
            FROM classes c
            INNER JOIN class_info ci
            ON ci.class_id = c.id
            INNER JOIN teachers t
            ON t.id = ci.teacher_id
            LEFT JOIN testing_classes tc
            ON tc.class_id = c.id
            WHERE c.id IN (%2)
              AND ci.teacher_id > 0
              AND tc.class_id IS NULL
        ),
        scoped_times AS (
            SELECT
                times.class_id,
                times.id,
                times.day,
                times.start_time
            FROM %1 times
            INNER JOIN scoped_classes scoped
            ON scoped.class_id = times.class_id
            WHERE times.day IN (%3)
        ),
        ranked_times AS (
            SELECT
                class_id,
                id,
                day,
                start_time,
                ROW_NUMBER() OVER (
                    PARTITION BY class_id
                    ORDER BY id
                ) AS class_meeting_order,
                ROW_NUMBER() OVER (
                    ORDER BY class_id, id
                ) AS total_meeting_order
            FROM scoped_times
        )
        SELECT
            scoped.class_id,
            scoped.teacher_id,
            scoped.class_grade,
            scoped.class_level,
            scoped.teacher_kr,
            scoped.teacher_en,
            scoped.preferred_romanization,
            scoped.preferred_name,
            scoped.room_number,
            scoped.wifi_name,
            scoped.wifi_password,
            scoped.internet_type,
            scoped.zoom_id,
            scoped.zoom_password,
            scoped.projection_type,
            scoped.teacher_notes,
            ranked.day,
            ranked.start_time
        FROM ranked_times ranked
        INNER JOIN scoped_classes scoped
        ON scoped.class_id = ranked.class_id
        WHERE ranked.class_meeting_order <= ?
          AND ranked.total_meeting_order <= ?
        ORDER BY ranked.class_id, ranked.id
    )").arg(
        timesTable,
        classIdValues.join(QStringLiteral(", ")),
        dayPlaceholders.join(QStringLiteral(", "))
        );

    const QString identity = QObject::tr("class ids %1, selected days %2")
        .arg(requestedIds.join(QStringLiteral(", ")))
        .arg(selectedDays.join(QStringLiteral(", ")));

    QList<SubPrepClassSummaryRecord> summaries;
    QHash<int, qsizetype> indexesByClassId;
    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    query.prepare(queryText);
    for (const QString& day : selectedDays)
    {
        query.addBindValue(day);
    }
    query.addBindValue(maxMeetingsPerClass + 1);
    query.addBindValue(maxTotalMeetings + 1);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading scoped Sub Prep class summaries"),
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        const int classId = query.value(QStringLiteral("class_id")).toInt();
        auto index = indexesByClassId.constFind(classId);
        if (index == indexesByClassId.cend())
        {
            SubPrepClassSummaryRecord record;
            record.classId = classId;
            record.teacherId = query.value(QStringLiteral("teacher_id")).toInt();
            record.classGrade = query.value(QStringLiteral("class_grade"))
                                    .toString();
            record.classLevel = query.value(QStringLiteral("class_level"))
                                    .toString();
            record.teacherKr = query.value(QStringLiteral("teacher_kr"))
                                   .toString();
            record.teacherEn = query.value(QStringLiteral("teacher_en"))
                                   .toString();
            record.teacherPreferredRomanization = query.value(
                QStringLiteral("preferred_romanization")
                ).toString();
            record.teacherPreferredName = query.value(
                QStringLiteral("preferred_name")
                ).toString();
            record.teacherRoomNumber = query.value(
                QStringLiteral("room_number")
                ).toString();
            record.teacherWifiName = query.value(
                QStringLiteral("wifi_name")
                ).toString();
            record.teacherWifiPassword = query.value(
                QStringLiteral("wifi_password")
                ).toString();
            record.teacherInternetType = query.value(
                QStringLiteral("internet_type")
                ).toString();
            record.teacherZoomId = query.value(QStringLiteral("zoom_id"))
                                       .toString();
            record.teacherZoomPassword = query.value(
                QStringLiteral("zoom_password")
                ).toString();
            record.teacherProjectionType = query.value(
                QStringLiteral("projection_type")
                ).toString();
            record.teacherNotes = query.value(QStringLiteral("teacher_notes"))
                                      .toString();
            indexesByClassId.insert(classId, summaries.size());
            summaries.append(std::move(record));
            index = indexesByClassId.constFind(classId);
        }

        summaries[*index].meetings.append({
            query.value(QStringLiteral("day")).toString(),
            query.value(QStringLiteral("start_time")).toString()
        });
    }

    if (query.lastError().type() != QSqlError::NoError)
    {
        return std::unexpected(
            QObject::tr("Loading scoped Sub Prep class summaries failed for %1: %2")
                .arg(identity, query.lastError().text())
            );
    }

    if (summaries.isEmpty())
    {
        return summaries;
    }

    QStringList summaryClassIdValues;
    summaryClassIdValues.reserve(summaries.size());
    for (const SubPrepClassSummaryRecord& summary : summaries)
    {
        summaryClassIdValues.append(QString::number(summary.classId));
    }

    // Aggregate the two roster name columns in one scoped query rather than
    // loading roster rows per class. As in the legacy page, roster failures
    // retain the zero-count fallback without failing otherwise valid summaries.
    const QString rosterCountSql = QStringLiteral(R"(
        WITH name_positions AS (
            SELECT
                class_id,
                MIN(CASE WHEN name = 'English' THEN position END)
                    AS english_position,
                MIN(CASE WHEN name = 'Korean' THEN position END)
                    AS korean_position
            FROM roster_columns
            WHERE class_id IN (%1)
              AND name IN ('English', 'Korean')
            GROUP BY class_id
        )
        SELECT
            positions.class_id,
            COUNT(DISTINCT cells.row_index) AS student_count
        FROM name_positions positions
        LEFT JOIN roster_data cells
        ON cells.class_id = positions.class_id
        AND (
            (
                positions.english_position IS NOT NULL
                AND cells.col_index = positions.english_position
                AND LENGTH(TRIM(
                    COALESCE(cells.value, ''),
                    char(9) || char(10) || char(11) || char(12)
                        || char(13) || ' '
                    )) > 0
            )
            OR
            (
                positions.korean_position IS NOT NULL
                AND cells.col_index = positions.korean_position
                AND LENGTH(TRIM(
                    COALESCE(cells.value, ''),
                    char(9) || char(10) || char(11) || char(12)
                        || char(13) || ' '
                    )) > 0
            )
        )
        GROUP BY positions.class_id
    )").arg(summaryClassIdValues.join(QStringLiteral(", ")));

    QHash<int, qint64> studentCounts;
    QSqlQuery rosterQuery(m_database);
    rosterQuery.setForwardOnly(true);
    rosterQuery.prepare(rosterCountSql);
    const auto rosterQueryResult = SqlQueryUtils::executePrepared(
        rosterQuery,
        QObject::tr("Loading scoped Sub Prep roster counts"),
        identity
        );
    if (rosterQueryResult)
    {
        while (rosterQuery.next())
        {
            bool countOk = false;
            const qint64 count = rosterQuery.value(
                QStringLiteral("student_count")
                ).toLongLong(&countOk);
            if (!countOk || count < 0)
            {
                studentCounts.clear();
                break;
            }
            studentCounts.insert(
                rosterQuery.value(QStringLiteral("class_id")).toInt(),
                count
                );
        }
        if (rosterQuery.lastError().type() != QSqlError::NoError)
        {
            studentCounts.clear();
        }
    }

    for (SubPrepClassSummaryRecord& summary : summaries)
    {
        summary.studentCount = studentCounts.value(summary.classId, 0);
    }

    return summaries;
}

Result<QList<ClassInfo>> ClassInfoRepository::loadClassInfosForScheduleScope(
    const QList<int>& classIds,
    const QStringList& selectedDays,
    const ScheduleType type,
    const int maxMeetingsPerClass,
    const int maxTotalMeetings,
    const bool includeUnassignedTeachers
    )
{
    if (classIds.isEmpty() || selectedDays.isEmpty())
    {
        return QList<ClassInfo>{};
    }

    if (type != ScheduleType::Regular && type != ScheduleType::Intensive)
    {
        return std::unexpected(
            QObject::tr("Loading scoped class information failed: invalid schedule type.")
            );
    }

    if (maxMeetingsPerClass < 0
        || maxMeetingsPerClass == std::numeric_limits<int>::max()
        || maxTotalMeetings < 0
        || maxTotalMeetings == std::numeric_limits<int>::max())
    {
        return std::unexpected(
            QObject::tr("Loading scoped class information failed: invalid meeting limit.")
            );
    }

    QSet<int> seenClassIds;
    for (const int classId : classIds)
    {
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return std::unexpected(
                QObject::tr(
                    "Loading scoped class information failed: class identifiers must be positive and unique."
                    )
                );
        }
        seenClassIds.insert(classId);
    }

    static const QStringList validDays{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
    QSet<QString> seenDays;
    for (const QString& day : selectedDays)
    {
        if (!validDays.contains(day) || seenDays.contains(day))
        {
            return std::unexpected(
                QObject::tr(
                    "Loading scoped class information failed: selected weekdays must be valid and unique."
                    )
                );
        }
        seenDays.insert(day);
    }

    // IDs are validated positive integers above, so formatting them as
    // decimal literals avoids exceeding SQLite's historical bind limit at
    // the application's 4,096-class scope bound.
    QStringList classIdValues;
    classIdValues.reserve(classIds.size());
    QStringList requestedIds;
    requestedIds.reserve(classIds.size());
    for (const int classId : classIds)
    {
        const QString value = QString::number(classId);
        classIdValues.append(value);
        requestedIds.append(value);
    }

    QStringList dayPlaceholders;
    dayPlaceholders.fill(QStringLiteral("?"), selectedDays.size());

    const QString timesTable = type == ScheduleType::Regular
        ? QStringLiteral("class_times")
        : QStringLiteral("class_intensive_times");
    const QString teacherAssignmentFilter = includeUnassignedTeachers
        ? QString()
        : QStringLiteral(R"(
              AND assigned_info.teacher_id > 0
              AND EXISTS (
                  SELECT 1
                  FROM teachers assigned_teacher
                  WHERE assigned_teacher.id = assigned_info.teacher_id
              )
        )");
    const QString queryText = QStringLiteral(R"(
        WITH scoped_times AS (
            SELECT
                times.class_id,
                times.id,
                times.day,
                times.start_time,
                times.end_time
            FROM %1 times
            INNER JOIN class_info assigned_info
            ON assigned_info.class_id = times.class_id
            WHERE times.class_id IN (%2)
              AND times.day IN (%3)
              %4
        ),
        ranked_times AS (
            SELECT
                class_id,
                id,
                day,
                start_time,
                end_time,
                ROW_NUMBER() OVER (
                    PARTITION BY class_id
                    ORDER BY id
                ) AS class_schedule_order,
                ROW_NUMBER() OVER (
                    ORDER BY class_id, id
                ) AS total_schedule_order
            FROM scoped_times
        )
        SELECT
            ci.class_id AS info_class_id,
            ci.teacher_id,
            ci.class_grade,
            ci.class_level,
            ci.class_color,
            ci.font_color,
            ci.notes,
            ranked_times.class_id AS schedule_class_id,
            ranked_times.day,
            ranked_times.start_time,
            ranked_times.end_time,
            ranked_times.class_schedule_order,
            ranked_times.total_schedule_order
        FROM ranked_times
        INNER JOIN class_info ci
        ON ci.class_id = ranked_times.class_id
        WHERE ranked_times.class_schedule_order <= ?
          AND ranked_times.total_schedule_order <= ?
        ORDER BY ranked_times.class_id, ranked_times.id
    )").arg(
        timesTable,
        classIdValues.join(QStringLiteral(", ")),
        dayPlaceholders.join(QStringLiteral(", ")),
        teacherAssignmentFilter
        );

    const QString identity = QObject::tr("class ids %1, selected days %2")
        .arg(requestedIds.join(QStringLiteral(", ")))
        .arg(selectedDays.join(QStringLiteral(", ")));

    QList<ClassInfo> infos;
    QHash<int, qsizetype> indexesByClassId;
    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    query.prepare(queryText);
    for (const QString& day : selectedDays)
    {
        query.addBindValue(day);
    }
    query.addBindValue(maxMeetingsPerClass + 1);
    query.addBindValue(maxTotalMeetings + 1);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading scoped Sub Prep class information"),
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        const int classId = query.value(QStringLiteral("schedule_class_id"))
                                .toInt();
        const QVariant infoClassId =
            query.value(QStringLiteral("info_class_id"));
        if (infoClassId.isNull())
        {
            return std::unexpected(
                QObject::tr(
                    "Loading scoped Sub Prep class information failed for class id %1: no matching class information record exists."
                    ).arg(classId)
                );
        }

        auto index = indexesByClassId.constFind(classId);
        if (index == indexesByClassId.cend())
        {
            ClassInfo info;
            info.classId = infoClassId.toInt();
            const QVariant teacherId =
                query.value(QStringLiteral("teacher_id"));
            info.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
            info.classGrade = query.value(QStringLiteral("class_grade"))
                                  .toString();
            info.classLevel = query.value(QStringLiteral("class_level"))
                                  .toString();

            const QString classColor =
                query.value(QStringLiteral("class_color")).toString();
            if (!classColor.isEmpty())
            {
                info.classColor = classColor;
            }

            const QString fontColor =
                query.value(QStringLiteral("font_color")).toString();
            if (!fontColor.isEmpty())
            {
                info.fontColor = fontColor;
            }
            info.notes = query.value(QStringLiteral("notes")).toString();

            indexesByClassId.insert(classId, infos.size());
            infos.append(std::move(info));
            index = indexesByClassId.constFind(classId);
        }

        ClassTime time{
            query.value(QStringLiteral("day")).toString(),
            query.value(QStringLiteral("start_time")).toString(),
            query.value(QStringLiteral("end_time")).toString()
        };
        ClassInfo& info = infos[*index];
        if (type == ScheduleType::Regular)
        {
            info.classTimes.append(std::move(time));
        }
        else
        {
            info.intensiveTimes.append(std::move(time));
        }
    }

    return infos;
}

Result<QList<SubPrepRosterOutputScheduleReadRecord>>
ClassInfoRepository::loadSubPrepRosterOutputScheduleRecords(
    const QList<int>& classIds,
    const QStringList& selectedDays,
    const ScheduleType type,
    const int maxMeetingsPerClass,
    const int maxTotalMeetings
    )
{
    if (classIds.isEmpty() || selectedDays.isEmpty())
    {
        return QList<SubPrepRosterOutputScheduleReadRecord>{};
    }

    ++m_subPrepRosterOutputScheduleBatchReadMetrics.callCount;
    m_subPrepRosterOutputScheduleBatchReadMetrics.requestedClassCount +=
        classIds.size();

    if (type != ScheduleType::Regular && type != ScheduleType::Intensive)
    {
        return std::unexpected(
            QObject::tr("Loading scoped Sub Prep roster-output schedule failed: invalid schedule type.")
            );
    }

    if (maxMeetingsPerClass < 0
        || maxMeetingsPerClass == std::numeric_limits<int>::max()
        || maxTotalMeetings < 0
        || maxTotalMeetings == std::numeric_limits<int>::max())
    {
        return std::unexpected(
            QObject::tr("Loading scoped Sub Prep roster-output schedule failed: invalid meeting limit.")
            );
    }

    QSet<int> seenClassIds;
    QStringList classIdValues;
    QStringList requestedIds;
    classIdValues.reserve(classIds.size());
    requestedIds.reserve(classIds.size());
    for (const int classId : classIds)
    {
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return std::unexpected(
                QObject::tr(
                    "Loading scoped Sub Prep roster-output schedule failed: class identifiers must be positive and unique."
                    )
                );
        }

        seenClassIds.insert(classId);
        const QString value = QString::number(classId);
        classIdValues.append(value);
        requestedIds.append(value);
    }

    static const QStringList validDays{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
    QSet<QString> seenDays;
    for (const QString& day : selectedDays)
    {
        if (!validDays.contains(day) || seenDays.contains(day))
        {
            return std::unexpected(
                QObject::tr(
                    "Loading scoped Sub Prep roster-output schedule failed: selected weekdays must be valid and unique."
                    )
                );
        }
        seenDays.insert(day);
    }

    QStringList dayPlaceholders;
    dayPlaceholders.fill(QStringLiteral("?"), selectedDays.size());

    const QString timesTable = type == ScheduleType::Regular
        ? QStringLiteral("class_times")
        : QStringLiteral("class_intensive_times");
    const QString queryText = QStringLiteral(R"(
        WITH scoped_times AS (
            SELECT
                times.class_id,
                times.id,
                times.day,
                times.start_time,
                times.end_time,
                assigned_info.teacher_id
            FROM %1 times
            INNER JOIN class_info assigned_info
            ON assigned_info.class_id = times.class_id
            WHERE times.class_id IN (%2)
              AND times.day IN (%3)
        ),
        ranked_times AS (
            SELECT
                class_id,
                teacher_id,
                id,
                day,
                start_time,
                end_time,
                ROW_NUMBER() OVER (
                    PARTITION BY class_id
                    ORDER BY id
                ) AS class_schedule_order,
                ROW_NUMBER() OVER (
                    ORDER BY class_id, id
                ) AS total_schedule_order
            FROM scoped_times
        )
        SELECT
            class_id AS schedule_class_id,
            teacher_id,
            day,
            start_time,
            end_time
        FROM ranked_times
        WHERE class_schedule_order <= ?
          AND total_schedule_order <= ?
        ORDER BY class_id, id
    )").arg(
        timesTable,
        classIdValues.join(QStringLiteral(", ")),
        dayPlaceholders.join(QStringLiteral(", "))
        );

    const QString identity = QObject::tr("class ids %1, selected days %2")
        .arg(requestedIds.join(QStringLiteral(", ")))
        .arg(selectedDays.join(QStringLiteral(", ")));

    QList<SubPrepRosterOutputScheduleReadRecord> records;
    QHash<int, qsizetype> indexesByClassId;
    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    query.prepare(queryText);
    for (const QString& day : selectedDays)
    {
        query.addBindValue(day);
    }
    query.addBindValue(maxMeetingsPerClass + 1);
    query.addBindValue(maxTotalMeetings + 1);

    ++m_subPrepRosterOutputScheduleBatchReadMetrics.statementCount;
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading scoped Sub Prep roster-output schedule"),
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        const int classId = query.value(
            QStringLiteral("schedule_class_id")
            ).toInt();
        auto index = indexesByClassId.constFind(classId);
        if (index == indexesByClassId.cend())
        {
            SubPrepRosterOutputScheduleReadRecord record;
            record.classId = classId;
            const QVariant teacherId = query.value(
                QStringLiteral("teacher_id")
                );
            record.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
            indexesByClassId.insert(classId, records.size());
            records.append(std::move(record));
            index = indexesByClassId.constFind(classId);
        }

        records[*index].meetings.append({
            query.value(QStringLiteral("day")).toString(),
            query.value(QStringLiteral("start_time")).toString(),
            query.value(QStringLiteral("end_time")).toString()
        });
    }

    return records;
}

Result<QList<ClassTeacherAssignment>>
ClassInfoRepository::loadClassTeacherAssignments()
{
    QList<ClassTeacherAssignment> assignments;
    QSqlQuery query(m_database);

    const auto executed = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            SELECT
                c.id AS class_id,
                ci.teacher_id
            FROM classes c
            LEFT JOIN testing_classes tc
            ON tc.class_id = c.id
            LEFT JOIN class_info ci
            ON ci.class_id = c.id
            WHERE tc.class_id IS NULL
            ORDER BY c.name, c.id
        )"),
        QObject::tr("Loading class teacher assignments")
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        const QVariant teacherId = query.value(QStringLiteral("teacher_id"));
        assignments.append({
            query.value(QStringLiteral("class_id")).toInt(),
            teacherId.isNull() ? -1 : teacherId.toInt()
        });
    }

    return assignments;
}

Result<QList<ClassInfo>> ClassInfoRepository::loadScheduleClassInfos()
{
    ++m_scheduleClassInfoReadMetrics.scheduleClassInfosCallCount;
    QList<ClassInfo> infos;
    QHash<int, qsizetype> indexesByClassId;
    QSqlQuery query(m_database);

    ++m_scheduleClassInfoReadMetrics.metadataStatementCount;
    const auto loadedClasses = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            SELECT
                c.id AS class_id,
                ci.teacher_id,
                ci.class_grade,
                ci.class_level,
                ci.class_color,
                ci.font_color,
                t.teacher_kr,
                t.teacher_en,
                t.preferred_name,
                t.room_number
            FROM classes c
            LEFT JOIN testing_classes tc
            ON tc.class_id = c.id
            LEFT JOIN class_info ci
            ON ci.class_id = c.id
            LEFT JOIN teachers t
            ON t.id = ci.teacher_id
            WHERE tc.class_id IS NULL
            ORDER BY c.name, c.id
        )"),
        QObject::tr("Loading schedule class information")
        );
    if (!loadedClasses)
    {
        return std::unexpected(loadedClasses.error().userMessage());
    }

    while (query.next())
    {
        ClassInfo info;
        info.classId = query.value(QStringLiteral("class_id")).toInt();
        const QVariant teacherId = query.value(QStringLiteral("teacher_id"));
        info.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
        info.teacherKr = query.value(QStringLiteral("teacher_kr")).toString();
        info.teacherEn = query.value(QStringLiteral("teacher_en")).toString();
        info.teacherPreferredName =
            query.value(QStringLiteral("preferred_name")).toString();
        info.roomNumber = query.value(QStringLiteral("room_number")).toString();
        info.classGrade = query.value(QStringLiteral("class_grade")).toString();
        info.classLevel = query.value(QStringLiteral("class_level")).toString();

        const QString classColor = query.value(QStringLiteral("class_color")).toString();
        if (!classColor.isEmpty())
        {
            info.classColor = classColor;
        }

        const QString fontColor = query.value(QStringLiteral("font_color")).toString();
        if (!fontColor.isEmpty())
        {
            info.fontColor = fontColor;
        }

        indexesByClassId.insert(info.classId, infos.size());
        infos.append(std::move(info));
    }

    auto loadTimes = [&]<typename Times>(
        const QString& tableName,
        Times ClassInfo::* times,
        const bool intensive
        )
        -> Status
    {
        QSqlQuery timesQuery(m_database);
        if (intensive)
        {
            ++m_scheduleClassInfoReadMetrics.intensiveScheduleStatementCount;
        }
        else
        {
            ++m_scheduleClassInfoReadMetrics.regularScheduleStatementCount;
        }
        const auto executed = SqlQueryUtils::execute(
            timesQuery,
            QStringLiteral(R"(
                SELECT
                    times.class_id,
                    times.day,
                    times.start_time,
                    times.end_time
                FROM %1 times
                INNER JOIN classes c
                ON c.id = times.class_id
                LEFT JOIN testing_classes tc
                ON tc.class_id = c.id
                WHERE tc.class_id IS NULL
                ORDER BY c.name, c.id, times.id
            )").arg(tableName),
            QObject::tr("Loading schedule class times")
            );
        if (!executed)
        {
            return std::unexpected(executed.error().userMessage());
        }

        while (timesQuery.next())
        {
            const auto index = indexesByClassId.constFind(
                timesQuery.value(QStringLiteral("class_id")).toInt()
                );
            if (index == indexesByClassId.cend())
            {
                continue;
            }

            (infos[*index].*times).append({
                timesQuery.value(QStringLiteral("day")).toString(),
                timesQuery.value(QStringLiteral("start_time")).toString(),
                timesQuery.value(QStringLiteral("end_time")).toString()
            });
        }

        return {};
    };

    if (const Status status = loadTimes(
            QStringLiteral("class_times"),
            &ClassInfo::classTimes,
            false
            ); !status)
    {
        return std::unexpected(status.error());
    }
    if (const Status status = loadTimes(
            QStringLiteral("class_intensive_times"),
            &ClassInfo::intensiveTimes,
            true
            ); !status)
    {
        return std::unexpected(status.error());
    }

    return infos;
}

Result<QList<ScheduleImportStateSnapshotClassInfoReadRecord>>
ClassInfoRepository::loadScheduleImportStateSnapshotClassInfos()
{
    ++m_scheduleImportStateSnapshotClassInfoReadMetrics.callCount;
    QList<ScheduleImportStateSnapshotClassInfoReadRecord> infos;
    QHash<int, qsizetype> indexesByClassId;
    QSqlQuery query(m_database);

    ++m_scheduleImportStateSnapshotClassInfoReadMetrics.metadataStatementCount;
    const auto loadedClasses = SqlQueryUtils::execute(
        query,
        QStringLiteral(R"(
            SELECT
                c.id AS class_id,
                ci.teacher_id,
                ci.class_grade,
                ci.class_level,
                ci.class_color,
                t.room_number
            FROM classes c
            LEFT JOIN testing_classes tc
            ON tc.class_id = c.id
            LEFT JOIN class_info ci
            ON ci.class_id = c.id
            LEFT JOIN teachers t
            ON t.id = ci.teacher_id
            WHERE tc.class_id IS NULL
            ORDER BY c.name, c.id
        )"),
        QObject::tr("Loading schedule class information")
        );
    if (!loadedClasses)
    {
        return std::unexpected(loadedClasses.error().userMessage());
    }

    while (query.next())
    {
        ScheduleImportStateSnapshotClassInfoReadRecord info;
        info.classId = query.value(QStringLiteral("class_id")).toInt();
        const QVariant teacherId = query.value(QStringLiteral("teacher_id"));
        info.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
        info.classGrade = query.value(QStringLiteral("class_grade")).toString();
        info.classLevel = query.value(QStringLiteral("class_level")).toString();

        const QString classColor =
            query.value(QStringLiteral("class_color")).toString();
        if (!classColor.isEmpty())
        {
            info.classColor = classColor;
        }

        info.roomNumber = query.value(QStringLiteral("room_number")).toString();
        indexesByClassId.insert(info.classId, infos.size());
        infos.append(std::move(info));
    }

    const auto loadTimes = [&]<typename Times>(
        const QString& tableName,
        Times ScheduleImportStateSnapshotClassInfoReadRecord::* times,
        const bool intensive
        ) -> Status
    {
        QSqlQuery timesQuery(m_database);
        if (intensive)
        {
            ++m_scheduleImportStateSnapshotClassInfoReadMetrics
                  .intensiveScheduleStatementCount;
        }
        else
        {
            ++m_scheduleImportStateSnapshotClassInfoReadMetrics
                  .regularScheduleStatementCount;
        }

        const auto executed = SqlQueryUtils::execute(
            timesQuery,
            QStringLiteral(R"(
                SELECT
                    times.class_id,
                    times.day,
                    times.start_time,
                    times.end_time
                FROM %1 times
                INNER JOIN classes c
                ON c.id = times.class_id
                LEFT JOIN testing_classes tc
                ON tc.class_id = c.id
                WHERE tc.class_id IS NULL
                ORDER BY c.name, c.id, times.id
            )").arg(tableName),
            QObject::tr("Loading schedule class times")
            );
        if (!executed)
        {
            return std::unexpected(executed.error().userMessage());
        }

        while (timesQuery.next())
        {
            const auto index = indexesByClassId.constFind(
                timesQuery.value(QStringLiteral("class_id")).toInt()
                );
            if (index == indexesByClassId.cend())
            {
                continue;
            }

            (infos[*index].*times).append({
                timesQuery.value(QStringLiteral("day")).toString(),
                timesQuery.value(QStringLiteral("start_time")).toString(),
                timesQuery.value(QStringLiteral("end_time")).toString()
            });
        }

        return {};
    };

    if (const Status status = loadTimes(
            QStringLiteral("class_times"),
            &ScheduleImportStateSnapshotClassInfoReadRecord::regularTimes,
            false
            ); !status)
    {
        return std::unexpected(status.error());
    }
    if (const Status status = loadTimes(
            QStringLiteral("class_intensive_times"),
            &ScheduleImportStateSnapshotClassInfoReadRecord::intensiveTimes,
            true
            ); !status)
    {
        return std::unexpected(status.error());
    }

    return infos;
}

Result<QList<ClassConflict>> ClassInfoRepository::getClassTimeConflicts(
    int classId,
    const QList<ClassTime>& times,
    ScheduleType type
    )
{
    QList<ClassConflict> conflicts;

    const Result<Classroom> currentClass =
        loadClassById(
            m_database,
            classId
            );
    if (!currentClass)
    {
        return std::unexpected(currentClass.error());
    }

    const QString currentClassName =
        classDisplayName(
            currentClass->name,
            classId
            );

    QList<TimeInterval> candidateIntervals;

    for (const ClassTime& time : times)
    {
        TimeInterval interval;

        if (toInterval(time, interval))
        {
            candidateIntervals.append(interval);
        }
        else
        {
            candidateIntervals.append(TimeInterval{});
        }
    }

    for (int i = 0; i < times.size(); ++i)
    {
        if (candidateIntervals[i].start < 0)
        {
            continue;
        }

        for (int j = i + 1; j < times.size(); ++j)
        {
            if (candidateIntervals[j].start < 0)
            {
                continue;
            }

            if (
                intervalsOverlap(
                    candidateIntervals[i],
                    candidateIntervals[j]
                    )
                )
            {
                ClassConflict conflict;
                conflict.classId = classId;
                conflict.className = currentClassName;
                conflict.day = times[i].day;
                conflict.startTime = times[i].startTime;
                conflict.endTime = times[i].endTime;
                conflict.conflictingClassName =
                    currentClassName;

                conflicts.append(conflict);
            }
        }
    }

    const QString tableName =
        type == ScheduleType::Regular
            ? QString("class_times")
            : QString("class_intensive_times");

    QSqlQuery query(m_database);

    query.prepare(
        QString(R"(
            SELECT
                times.class_id,
                classes.name AS class_name,
                times.day,
                times.start_time,
                times.end_time
            FROM %1 times
            LEFT JOIN classes
            ON classes.id = times.class_id
            WHERE times.class_id != ?
        )").arg(tableName)
        );

    query.addBindValue(classId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading class time conflicts"),
        QObject::tr("class id %1").arg(classId)
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        ClassTime existingTime;
        existingTime.day =
            query.value("day").toString();
        existingTime.startTime =
            query.value("start_time").toString();
        existingTime.endTime =
            query.value("end_time").toString();

        TimeInterval existingInterval;

        if (!toInterval(existingTime, existingInterval))
        {
            continue;
        }

        const int conflictingClassId =
            query.value("class_id").toInt();

        const QString conflictingClassName =
            classDisplayName(
                query.value("class_name").toString(),
                conflictingClassId
                );

        for (int i = 0; i < times.size(); ++i)
        {
            if (candidateIntervals[i].start < 0)
            {
                continue;
            }

            if (
                intervalsOverlap(
                    candidateIntervals[i],
                    existingInterval
                    )
                )
            {
                ClassConflict conflict;
                conflict.classId = classId;
                conflict.className = currentClassName;
                conflict.day = times[i].day;
                conflict.startTime = times[i].startTime;
                conflict.endTime = times[i].endTime;
                conflict.conflictingClassName =
                    conflictingClassName;

                conflicts.append(conflict);
            }
        }
    }

    return conflicts;
}
