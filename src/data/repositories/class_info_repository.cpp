#include "class_info_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"
#include "domain/models/classroom.h"

#include <QDebug>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

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
