#include "class_info_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"
#include "domain/models/classroom.h"
#include "domain/rules/schedule_value_parser.h"

#include <QDebug>
#include <QHash>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QStringList>

#include <array>
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
    const auto parsed = ScheduleValueParser::parseWeekday(day);
    if (!parsed || parsed->text != day)
    {
        return -1;
    }

    static const QStringList days{
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday",
        "Sunday"
    };

    return days.indexOf(parsed->text);
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

    const QString normalizedTime = QStringLiteral("%1:%2 %3")
        .arg(hour)
        .arg(minute, 2, 10, QLatin1Char('0'))
        .arg(period);
    constexpr std::array allowedFormats{
        ScheduleValueParser::DisplayTimeFormat::HourMinuteAmPmSpaced
    };
    const auto parsed = ScheduleValueParser::parseDisplayTime(
        normalizedTime,
        allowedFormats
        );
    if (!parsed)
    {
        return -1;
    }

    return parsed->hour() * 60 + parsed->minute();
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
    qint64 classId
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
    return saveClassInfoWithTeacherId(info, info.teacherId);
}

Status ClassInfoRepository::saveClassDetails(
    const ClassInfo& info,
    const qint64 teacherId
    )
{
    return saveClassInfoWithTeacherId(info, teacherId);
}

ClassInfoRepository::ClassDetailsParentCheckDetailedResult
ClassInfoRepository::checkClassDetailsParentDetailed(const int classId)
{
    if (classId <= 0)
    {
        return std::unexpected(ClassDetailsSaveFailure{
            .kind = ClassDetailsSaveFailure::Kind::Technical,
            .diagnostic = QObject::tr(
                "Loading class details failed: invalid class id %1."
                ).arg(classId)
        });
    }

    const auto parentExists = classDetailsParentExists(classId);
    if (!parentExists)
    {
        return std::unexpected(parentExists.error());
    }
    if (!*parentExists)
    {
        return std::unexpected(ClassDetailsSaveFailure{
            .kind = ClassDetailsSaveFailure::Kind::ParentClassMissing,
            .diagnostic = QObject::tr(
                "Saving class information failed: parent class id %1 was not found."
                ).arg(classId)
        });
    }

    return {};
}

ClassInfoRepository::ClassDetailsSaveDetailedResult
ClassInfoRepository::saveClassDetailsDetailed(
    const ClassInfo& info,
    const qint64 teacherId
    )
{
    return saveClassInfoWithTeacherIdDetailed(
        info,
        teacherId,
        ClassInfoSavePath::Typed
        );
}

Status ClassInfoRepository::saveClassInfoWithTeacherId(
    const ClassInfo& info,
    const qint64 teacherId
    )
{
    const ClassDetailsSaveDetailedResult saved =
        saveClassInfoWithTeacherIdDetailed(
            info,
            teacherId,
            ClassInfoSavePath::Legacy
            );
    if (!saved)
    {
        return std::unexpected(saved.error().diagnostic);
    }

    return {};
}

std::expected<bool, ClassInfoRepository::ClassDetailsSaveFailure>
ClassInfoRepository::classDetailsParentExists(const int classId)
{
    const QString identity = QObject::tr("class id %1").arg(classId);
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM classes WHERE id=?"
        ));
    query.addBindValue(classId);

    const auto loaded = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Checking class details parent class"),
        identity
        );
    if (!loaded)
    {
        return std::unexpected(ClassDetailsSaveFailure{
            .kind = ClassDetailsSaveFailure::Kind::Technical,
            .diagnostic = loaded.error().userMessage()
        });
    }

    const bool exists = query.next();
    const QSqlError fetchError = query.lastError();
    query.finish();
    if (fetchError.isValid())
    {
        return std::unexpected(ClassDetailsSaveFailure{
            .kind = ClassDetailsSaveFailure::Kind::Technical,
            .diagnostic = QObject::tr(
                "Checking class details parent class failed for %1: %2"
                ).arg(identity, fetchError.text())
        });
    }

    return exists;
}

ClassInfoRepository::ClassDetailsSaveDetailedResult
ClassInfoRepository::saveClassInfoWithTeacherIdDetailed(
    const ClassInfo& info,
    const qint64 teacherId,
    const ClassInfoRepository::ClassInfoSavePath savePath
    )
{
    if (info.classId <= 0)
    {
        return std::unexpected(ClassDetailsSaveFailure{
            .kind = ClassDetailsSaveFailure::Kind::Technical,
            .diagnostic = QObject::tr(
                "Saving class information failed: invalid class id %1."
                ).arg(info.classId)
        });
    }

    const QString identity = QObject::tr("class id %1").arg(info.classId);
    const bool typedSave = savePath == ClassInfoSavePath::Typed;
    DatabaseTransaction transaction(
        m_database,
        typedSave
            ? DatabaseTransaction::Mode::Immediate
            : DatabaseTransaction::Mode::Default
        );
    if (!transaction.started())
    {
        const QString startError = typedSave
            ? transaction.startError()
            : m_database.lastError().text();
        return std::unexpected(ClassDetailsSaveFailure{
            .kind = ClassDetailsSaveFailure::Kind::Technical,
            .diagnostic = QObject::tr(
                "Starting class information save transaction failed for "
                "class id %1: %2"
                ).arg(info.classId)
                 .arg(startError)
        });
    }

    if (typedSave)
    {
        const auto parentExists = classDetailsParentExists(info.classId);
        if (!parentExists)
        {
            return std::unexpected(parentExists.error());
        }
        if (!*parentExists)
        {
            if (!transaction.rollbackChecked())
            {
                return std::unexpected(ClassDetailsSaveFailure{
                    .kind = ClassDetailsSaveFailure::Kind::Technical,
                    .diagnostic = QObject::tr(
                        "Rolling back class information save failed for %1: %2"
                        ).arg(identity, m_database.lastError().text())
                });
            }

            return std::unexpected(ClassDetailsSaveFailure{
                .kind = ClassDetailsSaveFailure::Kind::ParentClassMissing,
                .diagnostic = QObject::tr(
                    "Saving class information failed: parent class id %1 was not found."
                    ).arg(info.classId)
            });
        }
    }

    QSqlQuery query(m_database);
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
        teacherId > 0
            ? QVariant(teacherId)
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
        return std::unexpected(ClassDetailsSaveFailure{
            .kind = ClassDetailsSaveFailure::Kind::Technical,
            .diagnostic = statement.error()
        });
    }

    query.prepare(
        "DELETE FROM class_times WHERE class_id=?"
        );

    query.addBindValue(info.classId);
    statement = execute(QObject::tr("Deleting regular class times"));
    if (!statement)
    {
        return std::unexpected(ClassDetailsSaveFailure{
            .kind = ClassDetailsSaveFailure::Kind::Technical,
            .diagnostic = statement.error()
        });
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
            return std::unexpected(ClassDetailsSaveFailure{
                .kind = ClassDetailsSaveFailure::Kind::Technical,
                .diagnostic = statement.error()
            });
        }
    }

    query.prepare(
        "DELETE FROM class_intensive_times WHERE class_id=?"
        );

    query.addBindValue(info.classId);
    statement = execute(QObject::tr("Deleting intensive class times"));
    if (!statement)
    {
        return std::unexpected(ClassDetailsSaveFailure{
            .kind = ClassDetailsSaveFailure::Kind::Technical,
            .diagnostic = statement.error()
        });
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
            return std::unexpected(ClassDetailsSaveFailure{
                .kind = ClassDetailsSaveFailure::Kind::Technical,
                .diagnostic = statement.error()
            });
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(ClassDetailsSaveFailure{
            .kind = ClassDetailsSaveFailure::Kind::Technical,
            .diagnostic = QObject::tr(
                "Committing class information failed for %1: %2"
                ).arg(identity, m_database.lastError().text())
        });
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

    return writeClassNotes(classId, notes, timeFillerActivities);
}

ClassInfoRepository::ClassNotesSaveDetailedResult
ClassInfoRepository::saveClassNotesDetailed(
    const int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    if (classId <= 0)
    {
        return std::unexpected(ClassNotesSaveFailure{
            .kind = ClassNotesSaveFailure::Kind::Technical,
            .diagnostic = QObject::tr(
                "Saving class notes failed: invalid class id %1."
                ).arg(classId)
        });
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    DatabaseTransaction transaction(
        m_database,
        DatabaseTransaction::Mode::Immediate
        );
    if (!transaction.started())
    {
        return std::unexpected(ClassNotesSaveFailure{
            .kind = ClassNotesSaveFailure::Kind::Technical,
            .diagnostic = QObject::tr(
                "Starting class notes save transaction failed for %1: %2"
                ).arg(identity, transaction.startError())
        });
    }

    bool parentClassExists = false;
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "SELECT 1 FROM classes WHERE id=?"
            ));
        query.addBindValue(classId);

        const auto loaded = SqlQueryUtils::executePrepared(
            query,
            QObject::tr("Checking class notes parent class"),
            identity
            );
        if (!loaded)
        {
            return std::unexpected(ClassNotesSaveFailure{
                .kind = ClassNotesSaveFailure::Kind::Technical,
                .diagnostic = loaded.error().userMessage()
            });
        }

        parentClassExists = query.next();
        const QSqlError fetchError = query.lastError();
        query.finish();
        if (!parentClassExists && fetchError.isValid())
        {
            return std::unexpected(ClassNotesSaveFailure{
                .kind = ClassNotesSaveFailure::Kind::Technical,
                .diagnostic = QObject::tr(
                    "Checking class notes parent class failed for %1: %2"
                    ).arg(identity, fetchError.text())
            });
        }
    }

    if (!parentClassExists)
    {
        if (!transaction.rollbackChecked())
        {
            return std::unexpected(ClassNotesSaveFailure{
                .kind = ClassNotesSaveFailure::Kind::Technical,
                .diagnostic = QObject::tr(
                    "Rolling back class notes save failed for %1: %2"
                    ).arg(identity, m_database.lastError().text())
            });
        }

        return std::unexpected(ClassNotesSaveFailure{
            .kind = ClassNotesSaveFailure::Kind::ParentClassMissing,
            .diagnostic = QObject::tr(
                "Saving class notes failed: parent class id %1 was not found."
                ).arg(classId)
        });
    }

    const Status saved = writeClassNotes(
        classId,
        notes,
        timeFillerActivities
        );
    if (!saved)
    {
        return std::unexpected(ClassNotesSaveFailure{
            .kind = ClassNotesSaveFailure::Kind::Technical,
            .diagnostic = saved.error()
        });
    }

    if (!transaction.commit())
    {
        return std::unexpected(ClassNotesSaveFailure{
            .kind = ClassNotesSaveFailure::Kind::Technical,
            .diagnostic = QObject::tr(
                "Committing class notes save failed for %1: %2"
                ).arg(identity, m_database.lastError().text())
        });
    }

    return {};
}

Status ClassInfoRepository::writeClassNotes(
    const int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
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

Status ClassInfoRepository::saveClassCoTeacherAssignment(
    const int classId,
    const std::optional<qint64> teacherId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr(
                "Saving class information failed: invalid class id %1."
                ).arg(classId)
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr(
                "Starting class information save transaction failed for "
                "class id %1: %2"
                ).arg(classId)
                 .arg(m_database.lastError().text())
            );
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO class_info (
            class_id,
            teacher_id
        )
        VALUES (?, ?)

        ON CONFLICT(class_id)
        DO UPDATE SET
            teacher_id=excluded.teacher_id
    )");
    query.addBindValue(classId);
    query.addBindValue(
        teacherId ? QVariant(*teacherId) : QVariant()
        );

    const QString identity = QObject::tr("class id %1").arg(classId);
    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Saving class information"),
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr(
                "Committing class information failed for %1: %2"
                ).arg(identity, m_database.lastError().text())
            );
    }

    return {};
}

namespace
{
Status assignExportTeacherId(ClassInfo& info, const QVariant& value)
{
    info.teacherId = value.isNull() ? -1 : value.toInt();
    return {};
}

Status assignExportTeacherId(
    ClassTransferExportClassInfoReadRecord& info, const QVariant& value)
{
    if (value.isNull())
    {
        info.teacherId = -1;
        return {};
    }
    bool valid = false;
    info.teacherId = value.toLongLong(&valid);
    if (!valid)
        return std::unexpected(QObject::tr(
            "Loading class information failed: invalid assigned teacher identifier."));
    return {};
}

template<class Record>
Result<Record> loadClassInfoRecord(
    QSqlDatabase& database,
    int classId,
    qint64* exactTeacherId = nullptr
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId)
            );
    }

    Record info;
    info.classId =    classId;
    info.classColor = "#FFFFFF";
    info.fontColor =  "#000000";

    QSqlQuery query(database);

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
        if (const Status assigned = assignExportTeacherId(info, teacherId); !assigned)
            return std::unexpected(assigned.error());
        if (exactTeacherId)
        {
            bool valid = false;
            *exactTeacherId = teacherId.isNull()
                ? -1 : teacherId.toLongLong(&valid);
            if (!teacherId.isNull() && !valid)
            {
                return std::unexpected(QObject::tr(
                    "Loading class information failed: invalid assigned teacher identifier."));
            }
            // Only validation uses the int surrogate; persistence binds the
            // separate exact teacher ID.
            if (*exactTeacherId > std::numeric_limits<int>::max())
            {
                info.teacherId = -1;
            }
            else if (*exactTeacherId < std::numeric_limits<int>::min())
            {
                info.teacherId = -2; // Preserve invalid negative-ID validation.
            }
        }
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

}

Result<ClassInfo> ClassInfoRepository::loadClassInfo(int classId)
{
    ++m_scheduleClassInfoReadMetrics.singleClassInfoReadCount;
    return loadClassInfoRecord<ClassInfo>(m_database, classId);
}

Result<ClassDetailsSaveReadRecord> ClassInfoRepository::loadClassDetailsSaveInfo(
    const int classId
    )
{
    ClassDetailsSaveReadRecord record;
    const auto loaded = loadClassInfoRecord<ClassInfo>(
        m_database, classId, &record.teacherId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }
    record.info = *loaded;
    return record;
}

Result<ClassTransferExportClassInfoReadRecord>
ClassInfoRepository::loadClassTransferExportClassInfoRecord(int classId)
{
    return loadClassInfoRecord<ClassTransferExportClassInfoReadRecord>(
        m_database, classId);
}

Result<ClassCoTeacherAssignmentValidationSnapshotReadRecord>
ClassInfoRepository::loadClassCoTeacherAssignmentValidationSnapshot(
    const int classId
    )
{
    const auto loaded =
        loadClassCoTeacherAssignmentValidationSnapshotDetailed(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error().message);
    }

    return *loaded;
}

ClassCoTeacherAssignmentValidationSnapshotDetailedResult
ClassInfoRepository::loadClassCoTeacherAssignmentValidationSnapshotDetailed(
    const int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            ClassCoTeacherAssignmentValidationSnapshotReadFailure{
                .kind =
                    ClassCoTeacherAssignmentValidationSnapshotFailureKind::
                        Technical,
                .message = QObject::tr(
                    "Loading class information failed: invalid class id %1."
                    ).arg(classId)
            }
            );
    }

    ClassCoTeacherAssignmentValidationSnapshotReadRecord snapshot;
    snapshot.classId = classId;

    const QString identity = QObject::tr("class id %1").arg(classId);
    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT
            classes.id AS class_id,
            class_info.class_grade,
            class_info.class_level,
            class_info.reading_book,
            class_info.essay_book,
            class_info.class_color,
            class_info.font_color,
            class_info.notes,
            class_info.time_filler_activities
        FROM classes
        LEFT JOIN class_info
        ON class_info.class_id = classes.id
        WHERE classes.id = ?
    )");
    query.addBindValue(classId);

    const auto loadedInfo = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Loading class information"),
        identity
        );
    if (!loadedInfo)
    {
        return std::unexpected(
            ClassCoTeacherAssignmentValidationSnapshotReadFailure{
                .kind =
                    ClassCoTeacherAssignmentValidationSnapshotFailureKind::
                        Technical,
                .message = loadedInfo.error().userMessage()
            }
            );
    }
    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            const auto readError = SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading class information"),
                {},
                identity
                );
            return std::unexpected(
                ClassCoTeacherAssignmentValidationSnapshotReadFailure{
                    .kind =
                        ClassCoTeacherAssignmentValidationSnapshotFailureKind::
                            Technical,
                    .message = readError.userMessage()
                }
                );
        }

        return std::unexpected(
            ClassCoTeacherAssignmentValidationSnapshotReadFailure{
                .kind =
                    ClassCoTeacherAssignmentValidationSnapshotFailureKind::
                        ClassNotFound,
                .message = QObject::tr(
                    "Loading class information failed for %1: class not found."
                    ).arg(identity)
            }
            );
    }

    snapshot.classGrade = query.value("class_grade").toString();
    snapshot.classLevel = query.value("class_level").toString();
    snapshot.readingBook = query.value("reading_book").toString();
    snapshot.essayBook = query.value("essay_book").toString();
    const QString classColor = query.value("class_color").toString();
    if (!classColor.isEmpty())
    {
        snapshot.classColor = classColor;
    }
    const QString fontColor = query.value("font_color").toString();
    if (!fontColor.isEmpty())
    {
        snapshot.fontColor = fontColor;
    }
    snapshot.notes = query.value("notes").toString();
    snapshot.timeFillerActivities =
        query.value("time_filler_activities").toString();

    query.prepare(R"(
        SELECT day, start_time, end_time
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
        return std::unexpected(
            ClassCoTeacherAssignmentValidationSnapshotReadFailure{
                .kind =
                    ClassCoTeacherAssignmentValidationSnapshotFailureKind::
                        Technical,
                .message = loadedRegularTimes.error().userMessage()
            }
            );
    }
    while (query.next())
    {
        snapshot.regularTimes.append({
            query.value("day").toString(),
            query.value("start_time").toString(),
            query.value("end_time").toString()
        });
    }

    query.prepare(R"(
        SELECT day, start_time, end_time
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
        return std::unexpected(
            ClassCoTeacherAssignmentValidationSnapshotReadFailure{
                .kind =
                    ClassCoTeacherAssignmentValidationSnapshotFailureKind::
                        Technical,
                .message = loadedIntensiveTimes.error().userMessage()
            }
            );
    }
    while (query.next())
    {
        snapshot.intensiveTimes.append({
            query.value("day").toString(),
            query.value("start_time").toString(),
            query.value("end_time").toString()
        });
    }

    return snapshot;
}

Result<ScheduleEditorClassInfoReadRecord>
ClassInfoRepository::loadScheduleEditorClassInfoRecord(const int classId)
{
    const auto loaded = loadScheduleEditorClassInfoRecordDetailed(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }

    return std::move(loaded->record);
}

Result<ScheduleEditorClassInfoReadOutcome>
ClassInfoRepository::loadScheduleEditorClassInfoRecordDetailed(
    const int classId
    )
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
    ScheduleEditorClassInfoReadOutcome outcome;
    outcome.record.classId = classId;

    QSqlQuery query(m_database);
    query.prepare(R"(
        WITH requested(class_id) AS (VALUES (?))
        SELECT requested.class_id AS requested_class_id,
               classes.id AS parent_class_id,
               ci.class_grade AS class_grade,
               ci.class_level AS class_level,
               ci.reading_book AS reading_book,
               ci.essay_book AS essay_book,
               ci.class_color AS class_color,
               ci.font_color AS font_color,
               teachers.teacher_kr AS teacher_korean_name,
               teachers.room_number AS room_number
        FROM requested
        LEFT JOIN classes ON classes.id = requested.class_id
        LEFT JOIN class_info ci ON ci.class_id = classes.id
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

    const bool rowFound = query.next();
    if (!rowFound)
    {
        if (query.lastError().isValid())
        {
            return std::unexpected(SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading class information"),
                {},
                identity
                ).userMessage());
        }

        return std::unexpected(QObject::tr(
            "Loading class information failed: no result was returned for class id %1."
            ).arg(classId));
    }

    outcome.parentMissing = query.value("parent_class_id").isNull();
    if (!outcome.parentMissing)
    {
        outcome.record.classId = query.value("requested_class_id").toInt();
        outcome.record.classGrade = query.value("class_grade").toString();
        outcome.record.classLevel = query.value("class_level").toString();
        outcome.record.readingBook = query.value("reading_book").toString();
        outcome.record.essayBook = query.value("essay_book").toString();

        const QString classColor = query.value("class_color").toString();
        if (!classColor.isEmpty())
        {
            outcome.record.classColor = classColor;
        }

        const QString fontColor = query.value("font_color").toString();
        if (!fontColor.isEmpty())
        {
            outcome.record.fontColor = fontColor;
        }

        outcome.record.teacherKoreanName =
            query.value("teacher_korean_name").toString();
        outcome.record.roomNumber = query.value("room_number").toString();
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading class information"),
            {},
            identity
            ).userMessage());
    }

    return outcome;
}

Result<SelectedClassGradeReadRecord>
ClassInfoRepository::loadSelectedClassGradeRecord(const int classId)
{
    const auto loaded = loadSelectedClassGradeRecordDetailed(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }

    return std::move(loaded->record);
}

Result<SelectedClassGradeReadOutcome>
ClassInfoRepository::loadSelectedClassGradeRecordDetailed(const int classId)
{
    ++m_selectedClassGradeReadMetrics.callCount;
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId)
            );
    }

    SelectedClassGradeReadOutcome outcome;
    outcome.record.classId = classId;

    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT classes.id AS parent_class_id, class_info.class_grade
        FROM classes
        LEFT JOIN class_info ON class_info.class_id = classes.id
        WHERE classes.id = ?
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

    const bool parentRowFound = query.next();
    if (query.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading class information"),
            {},
            QObject::tr("class id %1").arg(classId)
            ).userMessage());
    }

    if (!parentRowFound)
    {
        outcome.parentMissing = true;
        return outcome;
    }

    outcome.record.classGrade =
        query.value("class_grade").toString();
    return outcome;
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

Result<ClassPageDetailsReadOutcome>
ClassInfoRepository::loadClassPageDetailsDetailed(const int classId)
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
        SELECT class_info.teacher_id, class_info.class_grade,
               class_info.class_level, class_info.notes,
               class_info.time_filler_activities
        FROM classes
        LEFT JOIN class_info ON class_info.class_id = classes.id
        WHERE classes.id = ?
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

    const bool parentRowFound = metadataQuery.next();
    if (metadataQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading class information"),
            {},
            identity
            ).userMessage());
    }

    if (!parentRowFound)
    {
        return ClassPageDetailsReadOutcome{
            std::move(record),
            true
        };
    }

    const QVariant teacherId = metadataQuery.value("teacher_id");
    record.teacherId = teacherId.isNull() ? -1 : teacherId.toInt();
    record.classGrade = metadataQuery.value("class_grade").toString();
    record.classLevel = metadataQuery.value("class_level").toString();
    record.notes = metadataQuery.value("notes").toString();
    record.timeFillerActivities = metadataQuery.value(
        "time_filler_activities"
        ).toString();

    QSqlQuery regularScheduleQuery(m_database);
    regularScheduleQuery.prepare(R"(
        SELECT class_times.day, class_times.start_time
        FROM class_times
        INNER JOIN classes ON classes.id = class_times.class_id
        WHERE classes.id = ?
        ORDER BY class_times.id
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

    if (regularScheduleQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            regularScheduleQuery,
            QObject::tr("Loading regular class times"),
            {},
            identity
            ).userMessage());
    }

    return ClassPageDetailsReadOutcome{
        std::move(record),
        false
    };
}

Result<ClassCoTeacherPageDetailsReadRecord>
ClassInfoRepository::loadClassCoTeacherPageDetails(const int classId)
{
    const auto loaded = loadClassCoTeacherPageDetailsDetailed(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }

    return std::move(loaded->record);
}

Result<ClassCoTeacherPageDetailsReadOutcome>
ClassInfoRepository::loadClassCoTeacherPageDetailsDetailed(const int classId)
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
    ClassCoTeacherPageDetailsReadRecord record;
    record.classId = classId;

    QSqlQuery metadataQuery(m_database);
    metadataQuery.prepare(R"(
        SELECT class_info.teacher_id, class_info.class_grade,
               class_info.class_level, class_info.notes,
               class_info.time_filler_activities
        FROM classes
        LEFT JOIN class_info ON class_info.class_id = classes.id
        WHERE classes.id = ?
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

    const bool parentRowFound = metadataQuery.next();
    if (metadataQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading class information"),
            {},
            identity
            ).userMessage());
    }

    const bool parentMissing = !parentRowFound;
    if (parentRowFound)
    {
        const QVariant teacherId = metadataQuery.value("teacher_id");
        if (teacherId.isNull())
        {
            record.teacherId = -1;
        }
        else
        {
            bool teacherIdOk = false;
            record.teacherId = teacherId.toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return std::unexpected(QObject::tr(
                    "Loading class information failed: invalid co-teacher id for %1."
                    ).arg(identity));
            }
        }
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

    if (regularScheduleQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            regularScheduleQuery,
            QObject::tr("Loading regular class times"),
            {},
            identity
            ).userMessage());
    }

    return ClassCoTeacherPageDetailsReadOutcome{
        std::move(record),
        parentMissing
    };
}

Result<ClassNotesPageReadRecord>
ClassInfoRepository::loadClassNotesPageRecord(const int classId)
{
    const auto loaded = loadClassNotesPageRecordDetailed(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }

    return std::move(loaded->record);
}

Result<ClassNotesPageReadOutcome>
ClassInfoRepository::loadClassNotesPageRecordDetailed(const int classId)
{
    ++m_classNotesPageReadMetrics.callCount;
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId)
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    ClassNotesPageReadRecord record;
    record.classId = classId;

    QSqlQuery metadataQuery(m_database);
    metadataQuery.prepare(R"(
        SELECT class_info.teacher_id, class_info.class_grade,
               class_info.class_level, class_info.notes,
               class_info.time_filler_activities
        FROM classes
        LEFT JOIN class_info ON class_info.class_id = classes.id
        WHERE classes.id = ?
    )");
    metadataQuery.addBindValue(classId);
    ++m_classNotesPageReadMetrics.metadataStatementCount;
    const auto loadedMetadata = SqlQueryUtils::executePrepared(
        metadataQuery,
        QObject::tr("Loading class information"),
        identity
        );
    if (!loadedMetadata)
    {
        return std::unexpected(loadedMetadata.error().userMessage());
    }

    const bool parentRowFound = metadataQuery.next();
    if (!parentRowFound && metadataQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading class information"),
            {},
            identity
            ).userMessage());
    }

    const bool parentMissing = !parentRowFound;
    if (parentRowFound)
    {
        const QVariant teacherId = metadataQuery.value("teacher_id");
        if (teacherId.isNull())
        {
            record.teacherId = -1;
        }
        else
        {
            bool teacherIdOk = false;
            const qint64 parsedTeacherId = teacherId.toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return std::unexpected(QObject::tr(
                    "Loading class information failed: teacher id for class id %1 is not representable as a 64-bit integer."
                    ).arg(classId));
            }
            record.teacherId = parsedTeacherId;
        }
        record.classGrade = metadataQuery.value("class_grade").toString();
        record.classLevel = metadataQuery.value("class_level").toString();
        record.notes = metadataQuery.value("notes").toString();
        record.timeFillerActivities = metadataQuery.value(
            "time_filler_activities"
            ).toString();
    }
    if (metadataQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading class information"),
            {},
            identity
            ).userMessage());
    }

    QSqlQuery regularScheduleQuery(m_database);
    regularScheduleQuery.prepare(R"(
        SELECT day, start_time
        FROM class_times
        WHERE class_id = ?
        ORDER BY id
    )");
    regularScheduleQuery.addBindValue(classId);
    ++m_classNotesPageReadMetrics.regularScheduleStatementCount;
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

    if (regularScheduleQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            regularScheduleQuery,
            QObject::tr("Loading regular class times"),
            {},
            identity
            ).userMessage());
    }

    return ClassNotesPageReadOutcome{
        std::move(record),
        parentMissing
    };
}

Result<ClassDetailsPageReadRecord>
ClassInfoRepository::loadClassDetailsPageRecord(const int classId)
{
    const auto loaded = loadClassDetailsPageRecordDetailed(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }

    return std::move(loaded->record);
}

Result<ClassDetailsPageReadOutcome>
ClassInfoRepository::loadClassDetailsPageRecordDetailed(const int classId)
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
        SELECT class_info.teacher_id, class_info.class_grade,
               class_info.class_level, class_info.reading_book,
               class_info.essay_book, class_info.class_color,
               class_info.font_color
        FROM classes
        LEFT JOIN class_info ON class_info.class_id = classes.id
        WHERE classes.id = ?
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

    const bool parentRowFound = metadataQuery.next();
    if (!parentRowFound && metadataQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading class information"),
            {},
            identity
            ).userMessage());
    }

    const bool parentMissing = !parentRowFound;
    if (!parentMissing)
    {
        const QVariant teacherId = metadataQuery.value("teacher_id");
        if (teacherId.isNull())
        {
            record.teacherId = -1;
        }
        else
        {
            bool teacherIdOk = false;
            const qint64 parsedTeacherId =
                teacherId.toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return std::unexpected(QObject::tr(
                    "Loading class information failed: teacher id for class id %1 is not representable as a 64-bit integer."
                    ).arg(classId));
            }
            record.teacherId = parsedTeacherId;
        }
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

    if (scheduleQuery.lastError().isValid())
    {
        const SqlQueryUtils::ExecutionError error = SqlQueryUtils::errorFor(
            scheduleQuery,
            QObject::tr("Loading regular class times"),
            scheduleStatement,
            identity
            );
        return std::unexpected(scheduleErrorMessage(error));
    }

    return ClassDetailsPageReadOutcome{
        std::move(record),
        parentMissing
    };
}

namespace
{
template<class Record>
Result<QList<Record>> loadClassInfoRecords(
    QSqlDatabase& database,
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return QList<Record>{};
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

    QList<Record> records;
    records.reserve(classIds.size());
    for (const int classId : classIds)
    {
        Record info;
        info.classId = classId;
        records.append(std::move(info));
    }

    const QString requestedTable = QStringLiteral(
        "WITH requested(class_id, ordinal) AS (VALUES %1)"
        ).arg(requestedValues.join(QStringLiteral(", ")));

    QSqlQuery metadataQuery(database);
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

        Record& info = records[requestOrder];
        const QVariant teacherId = metadataQuery.value(
            QStringLiteral("teacher_id")
            );
        if (const Status assigned = assignExportTeacherId(info, teacherId); !assigned)
            return std::unexpected(assigned.error());
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

    const auto loadTimes = [&database, &classIds, &records, &requestedTable](
        const QString& table,
        const QString& action,
        const bool intensive
        ) -> Status
    {
        QSqlQuery scheduleQuery(database);
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

}

Result<QList<ClassInfo>> ClassInfoRepository::loadClassInfoRecords(
    const QList<int>& classIds)
{
    return ::loadClassInfoRecords<ClassInfo>(m_database, classIds);
}

Result<QList<ClassTransferExportClassInfoReadRecord>>
ClassInfoRepository::loadClassTransferExportClassInfoRecords(
    const QList<int>& classIds)
{
    return ::loadClassInfoRecords<ClassTransferExportClassInfoReadRecord>(
        m_database, classIds);
}

Result<ClassSubtitleReadRecord> ClassInfoRepository::loadClassSubtitleRecord(
    int classId
    )
{
    const auto loaded = loadClassSubtitleRecordDetailed(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }

    return std::move(loaded->record);
}

Result<ClassSubtitleReadOutcome>
ClassInfoRepository::loadClassSubtitleRecordDetailed(const int classId)
{
    if (classId <= 0)
    {
        return std::unexpected(
            QObject::tr("Loading class subtitle failed: invalid class id %1.")
                .arg(classId)
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    ClassSubtitleReadOutcome outcome;
    outcome.record.classId = classId;

    QSqlQuery query(m_database);
    query.prepare(R"(
        WITH requested(class_id) AS (VALUES (?))
        SELECT requested.class_id AS requested_class_id,
               classes.id AS parent_class_id,
               ci.teacher_id AS teacher_id,
               ci.class_grade AS class_grade,
               ci.class_level AS class_level
        FROM requested
        LEFT JOIN classes ON classes.id = requested.class_id
        LEFT JOIN class_info ci ON ci.class_id = requested.class_id
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

    const bool parentRowFound = query.next();
    if (query.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading class subtitle details"),
            {},
            identity
            ).userMessage());
    }

    if (!parentRowFound)
    {
        return std::unexpected(QObject::tr(
            "Loading class subtitle failed: no result was returned for class id %1."
            ).arg(classId));
    }

    outcome.parentMissing = query.value("parent_class_id").isNull();
    outcome.record.classId = query.value("requested_class_id").toInt();

    const QVariant teacherId = query.value("teacher_id");
    if (!teacherId.isNull())
    {
        bool teacherIdOk = false;
        outcome.record.teacherId = teacherId.toLongLong(&teacherIdOk);
        if (!teacherIdOk)
        {
            return std::unexpected(QObject::tr(
                "Loading class subtitle failed: the database returned an invalid teacher identifier."
                ));
        }
    }
    outcome.record.grade = query.value("class_grade").toString();
    outcome.record.level = query.value("class_level").toString();

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
        outcome.record.regularTimes.append(std::move(time));
    }

    if (query.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading class subtitle schedule"),
            {},
            identity
            ).userMessage());
    }

    return outcome;
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
                   classes.id AS parent_class_id,
                   ci.class_id AS class_info_class_id,
                   ci.teacher_id,
                   ci.class_grade,
                   ci.class_level
            FROM requested
            LEFT JOIN classes ON classes.id = requested.class_id
            LEFT JOIN class_info ci ON ci.class_id = classes.id
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
        record.parentMissing = metadataQuery.value("parent_class_id").isNull();
        if (!record.parentMissing
            && !metadataQuery.value("class_info_class_id").isNull())
        {
            const QVariant teacherId = metadataQuery.value("teacher_id");
            if (!teacherId.isNull())
            {
                bool teacherIdOk = false;
                record.teacherId = teacherId.toLongLong(&teacherIdOk);
                if (!teacherIdOk)
                {
                    return std::unexpected(QObject::tr(
                        "Loading class subtitles failed: the database returned an invalid teacher identifier."
                        ));
                }
            }
            record.grade = metadataQuery.value("class_grade").toString();
            record.level = metadataQuery.value("class_level").toString();
        }

        indexByClassId.insert(record.classId, records.size());
        records.append(std::move(record));
    }

    if (metadataQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading class subtitle metadata")
            ).userMessage());
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
            INNER JOIN classes ON classes.id = requested.class_id
            INNER JOIN class_times schedule ON schedule.class_id = classes.id
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

    if (scheduleQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            scheduleQuery,
            QObject::tr("Loading class subtitle regular schedules")
            ).userMessage());
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
            entries[index].parentMissing = false;
            const auto loaded = loadMyClassesClassInformationRecord(
                entries[index].classId
                );
            if (!loaded)
            {
                entries[index].information = std::unexpected(loaded.error());
                continue;
            }

            entries[index].parentMissing = loaded->parentMissing;
            entries[index].information = std::move(loaded->record);
        }
    };

    QSqlQuery metadataQuery(m_database);
    ++m_myClassesClassInformationBatchReadMetrics.metadataStatementCount;
    const auto loadedMetadata = SqlQueryUtils::execute(
        metadataQuery,
        requestedTable + QStringLiteral(R"(
            SELECT requested.class_id,
                   classes.id AS parent_class_id,
                   ci.class_id AS class_info_class_id,
                   ci.teacher_id,
                   ci.class_grade,
                   ci.class_level,
                   ci.notes,
                   ci.time_filler_activities
            FROM requested
            LEFT JOIN classes ON classes.id = requested.class_id
            LEFT JOIN class_info ci ON ci.class_id = classes.id
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
        const bool parentMissing =
            metadataQuery.value("parent_class_id").isNull();
        if (parentMissing)
        {
            entries[*entryIndex].parentMissing = true;
            entries[*entryIndex].information = std::move(record);
        }
        else if (!metadataQuery.value("class_info_class_id").isNull())
        {
            const QVariant teacherId = metadataQuery.value("teacher_id");
            bool teacherIdOk = true;
            if (!teacherId.isNull())
            {
                record.teacherId = teacherId.toLongLong(&teacherIdOk);
            }

            if (!teacherIdOk)
            {
                entries[*entryIndex].information = std::unexpected(
                    QObject::tr(
                        "Loading My Classes class information failed: teacher id for class id %1 cannot be represented as a signed 64-bit value."
                        ).arg(classId)
                    );
            }
            else
            {
                record.classGrade = metadataQuery.value("class_grade").toString();
                record.classLevel = metadataQuery.value("class_level").toString();
                record.notes = metadataQuery.value("notes").toString();
                record.timeFillerActivities = metadataQuery.value(
                    "time_filler_activities"
                    ).toString();
                entries[*entryIndex].information = std::move(record);
            }
        }
        else
        {
            entries[*entryIndex].information = std::move(record);
        }
        ++loadedMetadataCount;
    }

    if (invalidReturnedClassId
        || metadataQuery.lastError().isValid()
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
            INNER JOIN classes ON classes.id = requested.class_id
            INNER JOIN class_times schedule
            ON schedule.class_id = classes.id
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
        if (entries[*entryIndex].parentMissing
            || !entries[*entryIndex].information)
        {
            continue;
        }

        ClassTime time;
        time.day = regularScheduleQuery.value("day").toString();
        time.startTime = regularScheduleQuery.value("start_time").toString();
        time.endTime = regularScheduleQuery.value("end_time").toString();
        entries[*entryIndex].information->regularTimes.append(std::move(time));
    }
    if (invalidReturnedClassId || regularScheduleQuery.lastError().isValid())
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
            INNER JOIN classes ON classes.id = requested.class_id
            INNER JOIN class_intensive_times schedule
            ON schedule.class_id = classes.id
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
        if (entries[*entryIndex].parentMissing
            || !entries[*entryIndex].information)
        {
            continue;
        }

        ClassTime time;
        time.day = intensiveScheduleQuery.value("day").toString();
        time.startTime = intensiveScheduleQuery.value("start_time").toString();
        time.endTime = intensiveScheduleQuery.value("end_time").toString();
        entries[*entryIndex].information->intensiveTimes.append(
            std::move(time)
            );
    }
    if (invalidReturnedClassId || intensiveScheduleQuery.lastError().isValid())
    {
        loadIndividually();
        return entries;
    }

    return entries;
}

Result<std::vector<MyClassesClassSummaryBatchReadEntry>>
ClassInfoRepository::loadMyClassesClassSummaryRecords(
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return std::vector<MyClassesClassSummaryBatchReadEntry>{};
    }

    ++m_myClassesClassSummaryBatchReadMetrics.callCount;
    m_myClassesClassSummaryBatchReadMetrics.requestedClassCount +=
        static_cast<int>(classIds.size());

    std::vector<MyClassesClassSummaryBatchReadEntry> entries;
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
                .summary = std::unexpected(QObject::tr(
                    "Loading My Classes class summaries failed: invalid class id %1."
                    ).arg(classId))
            });
            continue;
        }

        if (entryIndexByClassId.contains(classId))
        {
            return std::unexpected(QObject::tr(
                "Loading My Classes class summaries failed: class identifiers must be unique."
                ));
        }

        const std::size_t entryIndex = entries.size();
        entryIndexByClassId.insert(classId, entryIndex);
        validEntryIndexes.push_back(entryIndex);
        entries.push_back({
            .classId = classId,
            .summary = std::unexpected(QString{})
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
            ++m_myClassesClassSummaryBatchReadMetrics.fallbackClassReadCount;
            entries[index].parentMissing = false;
            auto loaded = loadMyClassesClassSummaryRecord(
                entries[index].classId
                );
            if (!loaded)
            {
                entries[index].summary = std::unexpected(loaded.error());
                continue;
            }

            entries[index].parentMissing = loaded->parentMissing;
            entries[index].summary = std::move(loaded->record);
        }
    };

    QSqlQuery metadataQuery(m_database);
    ++m_myClassesClassSummaryBatchReadMetrics.metadataStatementCount;
    const auto loadedMetadata = SqlQueryUtils::execute(
        metadataQuery,
        requestedTable + QStringLiteral(R"(
            SELECT requested.class_id,
                   classes.id AS parent_class_id,
                   ci.class_id AS class_info_class_id,
                   ci.teacher_id,
                   ci.class_grade,
                   ci.class_level
            FROM requested
            LEFT JOIN classes ON classes.id = requested.class_id
            LEFT JOIN class_info ci ON ci.class_id = classes.id
            ORDER BY requested.ordinal
        )"),
        QObject::tr("Loading My Classes class summaries")
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

        MyClassesClassSummaryReadRecord record;
        record.classId = classId;
        const bool parentMissing =
            metadataQuery.value("parent_class_id").isNull();
        entries[*entryIndex].parentMissing = parentMissing;
        if (parentMissing)
        {
            entries[*entryIndex].summary = std::move(record);
        }
        else if (!metadataQuery.value("class_info_class_id").isNull())
        {
            const QVariant teacherId = metadataQuery.value("teacher_id");
            bool teacherIdOk = true;
            if (!teacherId.isNull())
            {
                record.teacherId = teacherId.toLongLong(&teacherIdOk);
            }

            if (!teacherIdOk)
            {
                entries[*entryIndex].summary = std::unexpected(
                    QObject::tr(
                        "Loading My Classes class summary failed: teacher id for class id %1 cannot be represented as a signed 64-bit value."
                        ).arg(classId)
                    );
            }
            else
            {
                record.classGrade = metadataQuery.value("class_grade").toString();
                record.classLevel = metadataQuery.value("class_level").toString();
                entries[*entryIndex].summary = std::move(record);
            }
        }
        else
        {
            entries[*entryIndex].summary = std::move(record);
        }
        ++loadedMetadataCount;
    }

    if (invalidReturnedClassId
        || metadataQuery.lastError().isValid()
        || loadedMetadataCount != validEntryIndexes.size())
    {
        loadIndividually();
        return entries;
    }

    QSqlQuery scheduleQuery(m_database);
    ++m_myClassesClassSummaryBatchReadMetrics.scheduleStatementCount;
    const auto loadedSchedule = SqlQueryUtils::execute(
        scheduleQuery,
        requestedTable + QStringLiteral(R"(
            , schedules AS (
                SELECT requested.ordinal,
                       requested.class_id,
                       0 AS schedule_type,
                       schedule.id AS schedule_id,
                       schedule.day,
                       schedule.start_time,
                       schedule.end_time
                FROM requested
                INNER JOIN classes ON classes.id = requested.class_id
                INNER JOIN class_times schedule
                ON schedule.class_id = classes.id
                UNION ALL
                SELECT requested.ordinal,
                       requested.class_id,
                       1 AS schedule_type,
                       schedule.id AS schedule_id,
                       schedule.day,
                       schedule.start_time,
                       schedule.end_time
                FROM requested
                INNER JOIN classes ON classes.id = requested.class_id
                INNER JOIN class_intensive_times schedule
                ON schedule.class_id = classes.id
            )
            SELECT class_id,
                   schedule_type,
                   day,
                   start_time,
                   end_time
            FROM schedules
            ORDER BY ordinal, schedule_type, schedule_id
        )"),
        QObject::tr("Loading My Classes class-summary schedules")
        );
    if (!loadedSchedule)
    {
        loadIndividually();
        return entries;
    }

    while (scheduleQuery.next())
    {
        const int classId = scheduleQuery.value("class_id").toInt();
        const auto entryIndex = entryIndexByClassId.constFind(classId);
        if (entryIndex == entryIndexByClassId.cend())
        {
            invalidReturnedClassId = true;
            break;
        }
        if (entries[*entryIndex].parentMissing
            || !entries[*entryIndex].summary)
        {
            continue;
        }

        ClassTime time;
        time.day = scheduleQuery.value("day").toString();
        time.startTime = scheduleQuery.value("start_time").toString();
        time.endTime = scheduleQuery.value("end_time").toString();
        MyClassesClassSummaryReadRecord& record =
            entries[*entryIndex].summary.value();
        if (scheduleQuery.value("schedule_type").toInt() == 0)
        {
            record.regularTimes.append(std::move(time));
        }
        else
        {
            record.intensiveTimes.append(std::move(time));
        }
    }

    if (invalidReturnedClassId || scheduleQuery.lastError().isValid())
    {
        loadIndividually();
    }

    return entries;
}

Result<ClassInfoRepository::MyClassesClassSummaryReadOutcome>
ClassInfoRepository::loadMyClassesClassSummaryRecord(
    const int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(QObject::tr(
            "Loading My Classes class summary failed: invalid class id %1."
            ).arg(classId));
    }

    MyClassesClassSummaryReadOutcome outcome;
    MyClassesClassSummaryReadRecord& record = outcome.record;
    record.classId = classId;

    QSqlQuery metadataQuery(m_database);
    metadataQuery.prepare(R"(
        SELECT classes.id AS parent_class_id,
               class_info.class_id AS class_info_class_id,
               class_info.teacher_id,
               class_info.class_grade,
               class_info.class_level
        FROM classes
        LEFT JOIN class_info ON class_info.class_id = classes.id
        WHERE classes.id = ?
    )");
    metadataQuery.addBindValue(classId);
    const auto loadedMetadata = SqlQueryUtils::executePrepared(
        metadataQuery,
        QObject::tr("Loading My Classes class-summary metadata"),
        QObject::tr("class id %1").arg(classId)
        );
    if (!loadedMetadata)
    {
        return std::unexpected(loadedMetadata.error().userMessage());
    }

    const bool parentFound = metadataQuery.next();
    if (metadataQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading My Classes class-summary metadata"),
            QObject::tr("class id %1").arg(classId)
            ).userMessage());
    }

    if (!parentFound)
    {
        outcome.parentMissing = true;
        return outcome;
    }

    if (!metadataQuery.value("class_info_class_id").isNull())
    {
        const QVariant teacherId = metadataQuery.value("teacher_id");
        if (!teacherId.isNull())
        {
            bool teacherIdOk = false;
            const qint64 convertedTeacherId =
                teacherId.toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return std::unexpected(
                    QObject::tr(
                        "Loading My Classes class summary failed: teacher id for class id %1 cannot be represented as a signed 64-bit value."
                        ).arg(classId)
                    );
            }
            record.teacherId = convertedTeacherId;
        }
        record.classGrade = metadataQuery.value("class_grade").toString();
        record.classLevel = metadataQuery.value("class_level").toString();
    }

    QSqlQuery scheduleQuery(m_database);
    scheduleQuery.prepare(R"(
        SELECT 0 AS schedule_type,
               class_times.id AS schedule_id,
               class_times.day,
               class_times.start_time,
               class_times.end_time
        FROM classes
        INNER JOIN class_times ON class_times.class_id = classes.id
        WHERE classes.id = ?
        UNION ALL
        SELECT 1 AS schedule_type,
               class_intensive_times.id AS schedule_id,
               class_intensive_times.day,
               class_intensive_times.start_time,
               class_intensive_times.end_time
        FROM classes
        INNER JOIN class_intensive_times
            ON class_intensive_times.class_id = classes.id
        WHERE classes.id = ?
        ORDER BY schedule_type, schedule_id
    )");
    scheduleQuery.addBindValue(classId);
    scheduleQuery.addBindValue(classId);
    const auto loadedSchedule = SqlQueryUtils::executePrepared(
        scheduleQuery,
        QObject::tr("Loading My Classes class-summary schedules"),
        QObject::tr("class id %1").arg(classId)
        );
    if (!loadedSchedule)
    {
        return std::unexpected(loadedSchedule.error().userMessage());
    }

    while (scheduleQuery.next())
    {
        ClassTime time;
        time.day = scheduleQuery.value("day").toString();
        time.startTime = scheduleQuery.value("start_time").toString();
        time.endTime = scheduleQuery.value("end_time").toString();
        if (scheduleQuery.value("schedule_type").toInt() == 0)
        {
            record.regularTimes.append(std::move(time));
        }
        else
        {
            record.intensiveTimes.append(std::move(time));
        }
    }

    if (scheduleQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            scheduleQuery,
            QObject::tr("Loading My Classes class-summary schedules"),
            QObject::tr("class id %1").arg(classId)
            ).userMessage());
    }

    return outcome;
}

Result<ClassInfoRepository::MyClassesClassInformationReadOutcome>
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
    MyClassesClassInformationReadOutcome outcome;
    MyClassesClassInformationReadRecord& record = outcome.record;
    record.classId = classId;

    QSqlQuery metadataQuery(m_database);
    metadataQuery.prepare(R"(
        SELECT classes.id AS parent_class_id,
               class_info.class_id AS class_info_class_id,
               class_info.teacher_id,
               class_info.class_grade,
               class_info.class_level,
               class_info.notes,
               class_info.time_filler_activities
        FROM classes
        LEFT JOIN class_info ON class_info.class_id = classes.id
        WHERE classes.id = ?
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

    const bool parentFound = metadataQuery.next();
    if (metadataQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading My Classes class information metadata"),
            identity
            ).userMessage());
    }

    if (!parentFound)
    {
        outcome.parentMissing = true;
        return outcome;
    }

    if (!metadataQuery.value("class_info_class_id").isNull())
    {
        const QVariant teacherId = metadataQuery.value("teacher_id");
        if (!teacherId.isNull())
        {
            bool teacherIdOk = false;
            const qint64 convertedTeacherId =
                teacherId.toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return std::unexpected(
                    QObject::tr(
                        "Loading My Classes class information failed: teacher id for class id %1 cannot be represented as a signed 64-bit value."
                        ).arg(classId)
                    );
            }
            record.teacherId = convertedTeacherId;
        }
        record.classGrade = metadataQuery.value("class_grade").toString();
        record.classLevel = metadataQuery.value("class_level").toString();
        record.notes = metadataQuery.value("notes").toString();
        record.timeFillerActivities = metadataQuery.value(
            "time_filler_activities"
            ).toString();
    }

    QSqlQuery regularScheduleQuery(m_database);
    regularScheduleQuery.prepare(R"(
        SELECT schedule.day, schedule.start_time, schedule.end_time
        FROM classes
        INNER JOIN class_times schedule ON schedule.class_id = classes.id
        WHERE classes.id = ?
        ORDER BY schedule.id
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
    if (regularScheduleQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            regularScheduleQuery,
            QObject::tr("Loading My Classes regular schedules"),
            identity
            ).userMessage());
    }

    QSqlQuery intensiveScheduleQuery(m_database);
    intensiveScheduleQuery.prepare(R"(
        SELECT schedule.day, schedule.start_time, schedule.end_time
        FROM classes
        INNER JOIN class_intensive_times schedule
            ON schedule.class_id = classes.id
        WHERE classes.id = ?
        ORDER BY schedule.id
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
    if (intensiveScheduleQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            intensiveScheduleQuery,
            QObject::tr("Loading My Classes intensive schedules"),
            identity
            ).userMessage());
    }

    return outcome;
}

Result<RosterPrintClassInfoReadRecord>
ClassInfoRepository::loadRosterPrintClassInfoRecord(
    int classId
    )
{
    const auto loaded = loadRosterPrintClassInfoRecordDetailed(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }

    return std::move(loaded->record);
}

Result<RosterPrintClassInfoReadOutcome>
ClassInfoRepository::loadRosterPrintClassInfoRecordDetailed(
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
    RosterPrintClassInfoReadOutcome outcome;
    outcome.record.classId = classId;

    QSqlQuery metadataQuery(m_database);
    metadataQuery.prepare(R"(
        WITH requested(class_id) AS (VALUES (?))
        SELECT
            requested.class_id AS requested_class_id,
            classes.id AS parent_class_id,
            ci.class_grade,
            ci.class_level,
            t.teacher_en,
            t.teacher_kr,
            t.room_number,
            t.wifi_name,
            t.wifi_password,
            t.zoom_id,
            t.zoom_password
        FROM requested
        LEFT JOIN classes ON classes.id = requested.class_id
        LEFT JOIN class_info ci ON ci.class_id = classes.id
        LEFT JOIN teachers t ON ci.teacher_id = t.id
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

    const bool parentRowFound = metadataQuery.next();
    if (metadataQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading roster print class information"),
            {},
            identity
            ).userMessage());
    }

    if (!parentRowFound)
    {
        return std::unexpected(QObject::tr(
            "Loading roster print class information failed: no result was returned for class id %1."
            ).arg(classId));
    }

    outcome.parentMissing = metadataQuery.value("parent_class_id").isNull();
    outcome.record.classId = metadataQuery.value("requested_class_id").toInt();
    outcome.record.parentMissing = outcome.parentMissing;

    outcome.record.classGrade =
        metadataQuery.value("class_grade").toString();
    outcome.record.classLevel =
        metadataQuery.value("class_level").toString();
    outcome.record.teacherEnglishName =
        metadataQuery.value("teacher_en").toString();
    outcome.record.teacherKoreanName =
        metadataQuery.value("teacher_kr").toString();
    outcome.record.roomNumber =
        metadataQuery.value("room_number").toString();
    outcome.record.wifiName =
        metadataQuery.value("wifi_name").toString();
    outcome.record.wifiPassword =
        metadataQuery.value("wifi_password").toString();
    outcome.record.zoomId =
        metadataQuery.value("zoom_id").toString();
    outcome.record.zoomPassword =
        metadataQuery.value("zoom_password").toString();

    QSqlQuery scheduleQuery(m_database);
    scheduleQuery.prepare(R"(
        SELECT schedule.day, schedule.start_time, schedule.end_time
        FROM classes
        INNER JOIN class_times schedule ON schedule.class_id = classes.id
        WHERE classes.id = ?
        ORDER BY schedule.id
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
        outcome.record.regularTimes.append({
            scheduleQuery.value("day").toString(),
            scheduleQuery.value("start_time").toString(),
            scheduleQuery.value("end_time").toString()
        });
    }

    if (scheduleQuery.lastError().isValid())
    {
        return std::unexpected(SqlQueryUtils::errorFor(
            scheduleQuery,
            QObject::tr("Loading roster print regular class times"),
            {},
            identity
            ).userMessage());
    }

    return outcome;
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
               classes.id AS parent_class_id,
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
        LEFT JOIN classes ON classes.id=requested.class_id
        LEFT JOIN class_info ci ON ci.class_id=classes.id
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

        records[requestOrder].parentMissing =
            metadataQuery.value("parent_class_id").isNull();
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
        INNER JOIN classes ON classes.id=requested.class_id
        INNER JOIN class_times schedule
            ON schedule.class_id=classes.id
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
        if (!teacherId.isNull())
        {
            bool teacherIdOk = false;
            const qint64 parsedTeacherId = teacherId.toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return std::unexpected(QObject::tr(
                    "Loading Sub Prep roster output class information failed: the database returned an invalid teacher identifier."
                    ));
            }
            record.teacherId = parsedTeacherId;
        }
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
            if (!teacherId.isNull())
            {
                bool teacherIdValid = false;
                const qint64 parsedTeacherId =
                    teacherId.toLongLong(&teacherIdValid);
                if (!teacherIdValid)
                {
                    return std::unexpected(QObject::tr(
                        "Loading classes navigation failed: a teacher identifier is invalid."
                        ));
                }
                record.teacherId = parsedTeacherId;
            }
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

Result<QList<ClassNavigationParentAwareReadRecord>>
ClassInfoRepository::loadClassesNavigationRecordsWithParentProvenance(
    const QList<int>& classIds
    )
{
    if (classIds.isEmpty())
    {
        return QList<ClassNavigationParentAwareReadRecord>{};
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
                "Loading classes navigation failed: class identifiers must be positive and unique."
                ));
        }

        seenClassIds.insert(classId);
        requestedValues.append(
            QStringLiteral("(%1, %2)").arg(classId).arg(index)
            );
    }

    const QString requestedValuesSql =
        requestedValues.join(QStringLiteral(", "));
    const QString metadataQueryText = QStringLiteral(R"(
        WITH requested(class_id, ordinal) AS (
            VALUES %1
        )
        SELECT
            requested.class_id,
            c.id AS parent_class_id,
            ci.class_id AS class_info_class_id,
            ci.teacher_id,
            ci.class_grade,
            ci.class_level,
            t.teacher_en,
            t.teacher_kr
        FROM requested
        LEFT JOIN classes c
        ON c.id = requested.class_id
        LEFT JOIN class_info ci
        ON ci.class_id = c.id
        LEFT JOIN teachers t
        ON t.id = ci.teacher_id
        ORDER BY requested.ordinal
    )").arg(requestedValuesSql);

    QList<ClassNavigationParentAwareReadRecord> records;
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
        ClassNavigationParentAwareReadRecord parentRecord;
        ClassNavigationReadRecord& record = parentRecord.details;
        record.classId = metadataQuery.value("class_id").toInt();
        parentRecord.parentExists =
            !metadataQuery.value("parent_class_id").isNull();
        record.hasClassInfo = parentRecord.parentExists
            && !metadataQuery.value("class_info_class_id").isNull();
        if (record.hasClassInfo)
        {
            const QVariant teacherId = metadataQuery.value("teacher_id");
            if (!teacherId.isNull())
            {
                bool teacherIdValid = false;
                const qint64 parsedTeacherId =
                    teacherId.toLongLong(&teacherIdValid);
                if (!teacherIdValid)
                {
                    return std::unexpected(QObject::tr(
                        "Loading classes navigation failed: a teacher identifier is invalid."
                        ));
                }
                record.teacherId = parsedTeacherId;
            }
            record.grade = metadataQuery.value("class_grade").toString();
            record.level = metadataQuery.value("class_level").toString();
            record.teacherEnglishName =
                metadataQuery.value("teacher_en").toString();
            record.teacherKoreanName =
                metadataQuery.value("teacher_kr").toString();
        }

        if (!indexByClassId.contains(record.classId))
        {
            indexByClassId.insert(record.classId, records.size());
        }
        records.append(std::move(parentRecord));
    }

    if (metadataQuery.lastError().isValid())
    {
        return std::unexpected(metadataQuery.lastError().text());
    }

    const auto loadTimes = [this,
                            &indexByClassId,
                            &records,
                            &requestedValuesSql](
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
            INNER JOIN classes parent
            ON parent.id = requested.class_id
            INNER JOIN class_info info
            ON info.class_id = parent.id
            INNER JOIN %2 schedule
            ON schedule.class_id = parent.id
            ORDER BY requested.ordinal, schedule.id
        )").arg(requestedValuesSql, table);
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

            ClassNavigationParentAwareReadRecord& parentRecord =
                records[*recordIndex];
            ClassNavigationReadRecord& record = parentRecord.details;
            if (!parentRecord.parentExists || !record.hasClassInfo)
            {
                continue;
            }

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

        if (timesQuery.lastError().isValid())
        {
            return std::unexpected(timesQuery.lastError().text());
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

const ClassNotesPageReadMetrics&
ClassInfoRepository::classNotesPageReadMetrics() const noexcept
{
    return m_classNotesPageReadMetrics;
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

const MyClassesClassSummaryBatchReadMetrics&
ClassInfoRepository::myClassesClassSummaryBatchReadMetrics() const noexcept
{
    return m_myClassesClassSummaryBatchReadMetrics;
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
    const SubPrepClassDetailsDetailedResult detailed =
        loadSubPrepClassDetailsDetailed(classId);
    if (!detailed)
    {
        return std::unexpected(detailed.error().diagnostic);
    }

    if (detailed->parentMissing)
    {
        return std::unexpected(
            QObject::tr(
                "Loading selected Sub Prep class details failed: no matching record exists for class id %1."
                ).arg(classId)
            );
    }

    return detailed->record;
}

SubPrepClassDetailsDetailedResult
ClassInfoRepository::loadSubPrepClassDetailsDetailed(
    const int classId
    )
{
    if (classId <= 0)
    {
        return std::unexpected(
            SubPrepClassDetailsReadFailure{
                .kind = SubPrepClassDetailsReadFailure::Kind::InvalidInput,
                .diagnostic = QObject::tr(
                "Loading selected Sub Prep class details failed: invalid class id %1."
                ).arg(classId)
            }
        );
    }

    const QString action = QObject::tr(
        "Loading selected Sub Prep class details"
        );
    const QString identity = QObject::tr("class id %1").arg(classId);
    QSqlQuery query(m_database);
    query.setForwardOnly(true);
    const QString queryText = QStringLiteral(R"(
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
    )");
    if (!query.prepare(queryText))
    {
        return std::unexpected(SubPrepClassDetailsReadFailure{
            .kind = SubPrepClassDetailsReadFailure::Kind::Technical,
            .diagnostic = SqlQueryUtils::errorFor(
                query,
                action,
                queryText,
                identity
                ).userMessage()
        });
    }
    query.addBindValue(classId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        action,
        identity
        );
    if (!executed)
    {
        return std::unexpected(SubPrepClassDetailsReadFailure{
            .kind = SubPrepClassDetailsReadFailure::Kind::Technical,
            .diagnostic = executed.error().userMessage()
        });
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return std::unexpected(SubPrepClassDetailsReadFailure{
                .kind = SubPrepClassDetailsReadFailure::Kind::Technical,
                .diagnostic = SqlQueryUtils::errorFor(
                    query,
                    action,
                    query.lastQuery(),
                    identity
                    ).userMessage()
            });
        }

        return SubPrepClassDetailsReadOutcome{
            .parentMissing = true
        };
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
    const qint64 assignedTeacherIdValue = assignedTeacherId.toLongLong(
        &assignedTeacherIdOk
        );
    const qint64 resolvedTeacherIdValue = resolvedTeacherId.toLongLong(
        &resolvedTeacherIdOk
        );
    if (assignedTeacherIdOk && resolvedTeacherIdOk
        && assignedTeacherIdValue > 0
        && resolvedTeacherIdValue > 0
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

    return SubPrepClassDetailsReadOutcome{
        .record = std::move(record),
        .parentMissing = false
    };
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
            bool teacherIdOk = false;
            record.teacherId = query.value(
                QStringLiteral("teacher_id")
                ).toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return std::unexpected(
                    QObject::tr(
                        "Unable to load scoped Sub Prep class summaries for %1: a teacher identifier cannot be represented as a signed 64-bit value."
                        ).arg(identity)
                    );
            }
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
            cells.row_index,
            typeof(cells.row_index) AS row_index_storage_type,
            cells.value
        FROM name_positions positions
        INNER JOIN roster_data cells
        ON cells.class_id = positions.class_id
        AND (
            (
                positions.english_position IS NOT NULL
                AND cells.col_index = positions.english_position
            )
            OR
            (
                positions.korean_position IS NOT NULL
                AND cells.col_index = positions.korean_position
            )
        )
        ORDER BY positions.class_id, cells.row_index
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
        int currentClassId = -1;
        qint64 currentRowIndex = -1;
        bool hasCurrentRow = false;
        bool currentRowHasStudent = false;
        bool countsValid = true;
        const auto finishCurrentRow = [&]()
        {
            if (!hasCurrentRow || !currentRowHasStudent)
            {
                return;
            }

            qint64& count = studentCounts[currentClassId];
            if (count == std::numeric_limits<qint64>::max())
            {
                countsValid = false;
                return;
            }
            ++count;
        };

        while (rosterQuery.next())
        {
            if (rosterQuery.value(
                    QStringLiteral("row_index_storage_type")
                    ).toString() != QStringLiteral("integer"))
            {
                countsValid = false;
                break;
            }

            bool classIdOk = false;
            const int classId = rosterQuery.value(
                QStringLiteral("class_id")
                ).toInt(&classIdOk);
            bool rowIndexOk = false;
            const qint64 rowIndex = rosterQuery.value(
                QStringLiteral("row_index")
                ).toLongLong(&rowIndexOk);
            if (!classIdOk || !rowIndexOk)
            {
                countsValid = false;
                break;
            }

            if (!hasCurrentRow
                || classId != currentClassId
                || rowIndex != currentRowIndex)
            {
                finishCurrentRow();
                if (!countsValid)
                {
                    break;
                }
                currentClassId = classId;
                currentRowIndex = rowIndex;
                currentRowHasStudent = false;
                hasCurrentRow = true;
            }

            const QString name = rosterQuery.value(
                QStringLiteral("value")
                ).toString();
            if (!name.trimmed().isEmpty())
            {
                currentRowHasStudent = true;
            }
        }

        if (countsValid
            && rosterQuery.lastError().type() == QSqlError::NoError)
        {
            finishCurrentRow();
        }
        if (!countsValid
            || rosterQuery.lastError().type() != QSqlError::NoError)
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

Result<QList<SubPrepPrintSourceClassInfoReadRecord>>
ClassInfoRepository::loadSubPrepPrintSourceClassInfosForScheduleScope(
    const QList<int>& classIds,
    const QStringList& selectedDays,
    const ScheduleType type,
    const int maxMeetingsPerClass,
    const int maxTotalMeetings
    )
{
    if (classIds.isEmpty() || selectedDays.isEmpty())
    {
        return QList<SubPrepPrintSourceClassInfoReadRecord>{};
    }

    if (type != ScheduleType::Regular && type != ScheduleType::Intensive)
    {
        return std::unexpected(QObject::tr(
            "Loading scoped Sub Prep print source information failed: invalid schedule type."
            ));
    }

    if (maxMeetingsPerClass < 0
        || maxMeetingsPerClass == std::numeric_limits<int>::max()
        || maxTotalMeetings < 0
        || maxTotalMeetings == std::numeric_limits<int>::max())
    {
        return std::unexpected(QObject::tr(
            "Loading scoped Sub Prep print source information failed: invalid meeting limit."
            ));
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
            return std::unexpected(QObject::tr(
                "Loading scoped Sub Prep print source information failed: class identifiers must be positive and unique."
                ));
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
            return std::unexpected(QObject::tr(
                "Loading scoped Sub Prep print source information failed: selected weekdays must be valid and unique."
                ));
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
            SELECT times.class_id, times.id, times.day,
                   times.start_time, times.end_time
            FROM %1 times
            INNER JOIN class_info assigned_info
            ON assigned_info.class_id = times.class_id
            WHERE times.class_id IN (%2)
              AND times.day IN (%3)
              AND assigned_info.teacher_id > 0
              AND EXISTS (
                  SELECT 1
                  FROM teachers assigned_teacher
                  WHERE assigned_teacher.id = assigned_info.teacher_id
              )
        ),
        ranked_times AS (
            SELECT class_id, id, day, start_time, end_time,
                   ROW_NUMBER() OVER (
                       PARTITION BY class_id ORDER BY id
                   ) AS class_schedule_order,
                   ROW_NUMBER() OVER (
                       ORDER BY class_id, id
                   ) AS total_schedule_order
            FROM scoped_times
        )
        SELECT ci.class_id AS info_class_id,
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
        INNER JOIN class_info ci ON ci.class_id = ranked_times.class_id
        WHERE ranked_times.class_schedule_order <= ?
          AND ranked_times.total_schedule_order <= ?
        ORDER BY ranked_times.class_id, ranked_times.id
    )").arg(
        timesTable,
        classIdValues.join(QStringLiteral(", ")),
        dayPlaceholders.join(QStringLiteral(", "))
        );

    const QString identity = QObject::tr("class ids %1, selected days %2")
        .arg(requestedIds.join(QStringLiteral(", ")))
        .arg(selectedDays.join(QStringLiteral(", ")));

    QList<SubPrepPrintSourceClassInfoReadRecord> records;
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
        QObject::tr("Loading scoped Sub Prep print source information"),
        identity
        );
    if (!executed)
    {
        return std::unexpected(executed.error().userMessage());
    }

    while (query.next())
    {
        bool scheduleClassIdValid = false;
        const qint64 scheduleClassId = query.value(
            QStringLiteral("schedule_class_id")
            ).toLongLong(&scheduleClassIdValid);
        const QVariant infoClassIdValue = query.value(
            QStringLiteral("info_class_id")
            );
        bool infoClassIdValid = false;
        const qint64 infoClassId = infoClassIdValue.toLongLong(
            &infoClassIdValid
            );
        if (!scheduleClassIdValid
            || !infoClassIdValid
            || scheduleClassId <= 0
            || scheduleClassId > std::numeric_limits<int>::max()
            || infoClassId != scheduleClassId)
        {
            return std::unexpected(QObject::tr(
                "Loading scoped Sub Prep print source information failed: the database returned an invalid class identifier."
                ));
        }

        const int classId = static_cast<int>(scheduleClassId);
        bool teacherIdValid = false;
        const qint64 teacherId = query.value(
            QStringLiteral("teacher_id")
            ).toLongLong(&teacherIdValid);
        if (!teacherIdValid || teacherId <= 0)
        {
            return std::unexpected(QObject::tr(
                "Loading scoped Sub Prep print source information failed for class id %1: the database returned an invalid teacher identifier."
                ).arg(classId));
        }

        auto index = indexesByClassId.constFind(classId);
        if (index == indexesByClassId.cend())
        {
            SubPrepPrintSourceClassInfoReadRecord record;
            record.classId = classId;
            record.teacherId = teacherId;
            record.classGrade = query.value(
                QStringLiteral("class_grade")
                ).toString();
            record.classLevel = query.value(
                QStringLiteral("class_level")
                ).toString();

            const QString classColor = query.value(
                QStringLiteral("class_color")
                ).toString();
            if (!classColor.isEmpty())
            {
                record.classColor = classColor;
            }

            const QString fontColor = query.value(
                QStringLiteral("font_color")
                ).toString();
            if (!fontColor.isEmpty())
            {
                record.fontColor = fontColor;
            }
            record.notes = query.value(QStringLiteral("notes")).toString();

            indexesByClassId.insert(classId, records.size());
            records.append(std::move(record));
            index = indexesByClassId.constFind(classId);
        }
        else if (records[*index].teacherId != teacherId)
        {
            return std::unexpected(QObject::tr(
                "Loading scoped Sub Prep print source information failed for class id %1: the database returned inconsistent teacher identifiers."
                ).arg(classId));
        }

        ClassTime time{
            query.value(QStringLiteral("day")).toString(),
            query.value(QStringLiteral("start_time")).toString(),
            query.value(QStringLiteral("end_time")).toString()
        };
        SubPrepPrintSourceClassInfoReadRecord& record = records[*index];
        if (type == ScheduleType::Regular)
        {
            record.classTimes.append(std::move(time));
        }
        else
        {
            record.intensiveTimes.append(std::move(time));
        }
    }

    return records;
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
            if (!teacherId.isNull())
            {
                bool teacherIdOk = false;
                const qint64 parsedTeacherId =
                    teacherId.toLongLong(&teacherIdOk);
                if (!teacherIdOk)
                {
                    return std::unexpected(QObject::tr(
                        "Loading scoped Sub Prep roster-output schedule failed: the database returned an invalid teacher identifier."
                        ));
                }
                record.teacherId = parsedTeacherId;
            }
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
        const QVariant storedClassIdValue = query.value(QStringLiteral("class_id"));
        bool storedClassIdOk = false;
        const qint64 storedClassId =
            storedClassIdValue.toLongLong(&storedClassIdOk);
        if (!storedClassIdOk
            || storedClassId <= 0
            || storedClassId > std::numeric_limits<int>::max())
        {
            return std::unexpected(
                QObject::tr(
                    "Loading schedule class information failed: invalid class id %1."
                    ).arg(storedClassIdValue.toString())
                );
        }
        info.classId = static_cast<int>(storedClassId);
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
        if (teacherId.isNull())
        {
            info.teacherId = -1;
        }
        else
        {
            bool teacherIdOk = false;
            info.teacherId = teacherId.toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return std::unexpected(
                    QObject::tr("A class teacher ID could not be read.")
                    );
            }
        }
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

        const qint64 conflictingClassId =
            query.value("class_id").toLongLong();

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
