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
// Preserve existing typed failures; diagnostic-only database failures default
// to Technical. Input and persisted-data checks classify their own branches.
[[nodiscard]] std::unexpected<RepositoryError> repositoryFailure(
    RepositoryError error
    )
{
    return std::unexpected(std::move(error));
}

[[nodiscard]] std::unexpected<RepositoryError> repositoryFailure(
    QString diagnostic,
    const RepositoryErrorCode code = RepositoryErrorCode::Technical
    )
{
    return repositoryFailure(RepositoryError{code, std::move(diagnostic)});
}

RepositoryStatus rollbackSaveFailure(
    DatabaseTransaction& transaction,
    QSqlDatabase& database,
    RepositoryError failure,
    const QString& rollbackDiagnostic
    )
{
    if (!transaction.rollbackChecked())
    {
        return repositoryFailure(
            failure.diagnostic + QLatin1Char('\n')
                + rollbackDiagnostic.arg(database.lastError().text()),
            RepositoryErrorCode::Technical
            );
    }
    return repositoryFailure(std::move(failure));
}

// NULL means unassigned; an empty persisted value historically means zero.
// Parse at full width first: QVariant::toInt() can narrow numeric variants.
template<typename Id>
RepositoryResult<Id> persistedTeacherId(
    const QVariant& value,
    const QString& diagnostic
    )
{
    if (value.isNull())
    {
        return Id{-1};
    }
    if (value.toString().isEmpty())
    {
        return Id{0};
    }
    bool valid = false;
    const qint64 parsed = value.toString().toLongLong(&valid);
    if (!valid || parsed < std::numeric_limits<Id>::min()
        || parsed > std::numeric_limits<Id>::max())
    {
        return repositoryFailure(diagnostic, RepositoryErrorCode::Validation);
    }
    return static_cast<Id>(parsed);
}

template<typename Id = int>
RepositoryResult<Id> persistedClassId(
    const QVariant& value,
    const QString& diagnostic
    )
{
    bool valid = false;
    const qint64 parsed = value.toString().toLongLong(&valid);
    if (!valid || parsed <= 0 || parsed > std::numeric_limits<Id>::max())
    {
        return repositoryFailure(diagnostic, RepositoryErrorCode::Validation);
    }
    return static_cast<Id>(parsed);
}

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

RepositoryResult<Classroom> loadClassById(
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
        return repositoryFailure(executed.error().userMessage());
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return repositoryFailure(SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading class for conflict detection"),
                {},
                QObject::tr("class id %1").arg(classId)
                ).userMessage());
        }

        return repositoryFailure(
            QObject::tr(
                "Loading class for conflict detection failed for class id "
                "%1: no matching record exists."
                ).arg(classId),
            RepositoryErrorCode::NotFound
            );
    }

    Classroom classroom;
    const auto parsedClassId = persistedClassId(
        query.value("id"), QObject::tr(
            "Loading class information failed: the database returned an invalid class identifier."));
    if (!parsedClassId)
    {
        return repositoryFailure(parsedClassId.error());
    }
    classroom.id = *parsedClassId;

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

RepositoryStatus ClassInfoRepository::saveClassInfo(
    const ClassInfo& info
    )
{
    return saveClassInfoWithTeacherId(info, info.teacherId);
}

RepositoryStatus ClassInfoRepository::saveClassDetails(
    const ClassInfo& info,
    const qint64 teacherId
    )
{
    return saveClassInfoWithTeacherId(info, teacherId);
}

RepositoryStatus
ClassInfoRepository::checkClassDetailsParentDetailed(const int classId)
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr(
                "Loading class details failed: invalid class id %1."
                ).arg(classId),
            RepositoryErrorCode::InvalidInput
            );
    }

    const auto parentExists = classDetailsParentExists(classId);
    if (!parentExists)
    {
        return repositoryFailure(parentExists.error());
    }
    if (!*parentExists)
    {
        return repositoryFailure(
            QObject::tr(
                "Saving class information failed: parent class id %1 was not found."
                ).arg(classId),
            RepositoryErrorCode::NotFound
            );
    }

    return {};
}

RepositoryStatus
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

RepositoryStatus ClassInfoRepository::saveClassInfoWithTeacherId(
    const ClassInfo& info,
    const qint64 teacherId
    )
{
    const RepositoryStatus saved =
        saveClassInfoWithTeacherIdDetailed(
            info,
            teacherId,
            ClassInfoSavePath::Legacy
            );
    if (!saved)
    {
        return repositoryFailure(saved.error());
    }

    return {};
}

RepositoryResult<bool>
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
        return repositoryFailure(
            loaded.error().userMessage(),
            RepositoryErrorCode::Technical
            );
    }

    const bool exists = query.next();
    const QSqlError fetchError = query.lastError();
    query.finish();
    if (fetchError.isValid())
    {
        return repositoryFailure(
            QObject::tr(
                "Checking class details parent class failed for %1: %2"
                ).arg(identity, fetchError.text()),
            RepositoryErrorCode::Technical
            );
    }

    return exists;
}

RepositoryStatus
ClassInfoRepository::saveClassInfoWithTeacherIdDetailed(
    const ClassInfo& info,
    const qint64 teacherId,
    const ClassInfoRepository::ClassInfoSavePath savePath
    )
{
    if (info.classId <= 0)
    {
        return repositoryFailure(
            QObject::tr(
                "Saving class information failed: invalid class id %1."
                ).arg(info.classId),
            RepositoryErrorCode::InvalidInput
            );
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
        return repositoryFailure(
            QObject::tr(
                "Starting class information save transaction failed for "
                "class id %1: %2"
                ).arg(info.classId)
                 .arg(startError),
            RepositoryErrorCode::Technical
            );
    }

    const auto failAndRollback = [&](RepositoryError failure) -> RepositoryStatus
    {
        return rollbackSaveFailure(
            transaction, m_database, std::move(failure),
            QObject::tr("Rolling back class information save failed for %1: %2")
                .arg(identity));
    };

    if (typedSave)
    {
        const auto parentExists = classDetailsParentExists(info.classId);
        if (!parentExists)
        {
            return failAndRollback(parentExists.error());
        }
        if (!*parentExists)
        {
            return failAndRollback(RepositoryError{
                RepositoryErrorCode::NotFound,
                QObject::tr(
                    "Saving class information failed: parent class id %1 was not found."
                    ).arg(info.classId)
                });
        }

        const auto checkScheduleConflicts = [this, &info](
            const QList<ClassTime>& times,
            const ScheduleType type
            ) -> RepositoryStatus
        {
            if (times.isEmpty())
            {
                return {};
            }

            const RepositoryResult<QList<ClassConflict>> conflicts =
                getClassTimeConflicts(info.classId, times, type);
            if (!conflicts)
            {
                return repositoryFailure(conflicts.error());
            }
            if (conflicts->isEmpty())
            {
                return {};
            }

            const ClassConflict& first = conflicts->first();
            // The class-details save contract reports overlap rejection as
            // Technical while the separate conflict query returns its data.
            return repositoryFailure(
                QStringLiteral(
                    "Class schedule conflict: %1 %2\u2013%3 conflicts with %4."
                    ).arg(
                        first.day,
                        first.startTime,
                        first.endTime,
                        first.conflictingClassName
                        ),
                RepositoryErrorCode::Technical
                );
        };

        const auto regularConflicts = checkScheduleConflicts(
            info.classTimes,
            ScheduleType::Regular
            );
        if (!regularConflicts)
        {
            return failAndRollback(regularConflicts.error());
        }

        const auto intensiveConflicts = checkScheduleConflicts(
            info.intensiveTimes,
            ScheduleType::Intensive
            );
        if (!intensiveConflicts)
        {
            return failAndRollback(intensiveConflicts.error());
        }
    }

    QSqlQuery query(m_database);
    auto execute = [&](const QString& action) -> RepositoryStatus
    {
        const auto result = SqlQueryUtils::executePrepared(
            query, action, identity);
        return result
            ? RepositoryStatus{}
            : RepositoryStatus(repositoryFailure(result.error().userMessage()));
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

    RepositoryStatus statement = execute(QObject::tr("Saving class information"));
    if (!statement)
    {
        return failAndRollback(statement.error());
    }

    query.prepare(
        "DELETE FROM class_times WHERE class_id=?"
        );

    query.addBindValue(info.classId);
    statement = execute(QObject::tr("Deleting regular class times"));
    if (!statement)
    {
        return failAndRollback(statement.error());
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
            return failAndRollback(statement.error());
        }
    }

    query.prepare(
        "DELETE FROM class_intensive_times WHERE class_id=?"
        );

    query.addBindValue(info.classId);
    statement = execute(QObject::tr("Deleting intensive class times"));
    if (!statement)
    {
        return failAndRollback(statement.error());
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
            return failAndRollback(statement.error());
        }
    }

    if (!transaction.commit())
    {
        return failAndRollback(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr(
                "Committing class information failed for %1: %2"
                ).arg(identity, m_database.lastError().text())
            });
    }

    return {};
}

RepositoryStatus ClassInfoRepository::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr("Saving class notes failed: invalid class id %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
            );
    }

    return writeClassNotes(classId, notes, timeFillerActivities);
}

RepositoryStatus
ClassInfoRepository::saveClassNotesDetailed(
    const int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr(
                "Saving class notes failed: invalid class id %1."
                ).arg(classId),
            RepositoryErrorCode::InvalidInput
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    DatabaseTransaction transaction(
        m_database,
        DatabaseTransaction::Mode::Immediate
        );
    if (!transaction.started())
    {
        return repositoryFailure(
            QObject::tr(
                "Starting class notes save transaction failed for %1: %2"
                ).arg(identity, transaction.startError()),
            RepositoryErrorCode::Technical
            );
    }

    const auto failAndRollback = [&](RepositoryError failure) -> RepositoryStatus
    {
        return rollbackSaveFailure(
            transaction, m_database, std::move(failure),
            QObject::tr("Rolling back class notes save failed for %1: %2")
                .arg(identity));
    };

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
            return failAndRollback(RepositoryError{
                RepositoryErrorCode::Technical, loaded.error().userMessage()});
        }

        parentClassExists = query.next();
        const QSqlError fetchError = query.lastError();
        query.finish();
        if (!parentClassExists && fetchError.isValid())
        {
            return failAndRollback(RepositoryError{
                RepositoryErrorCode::Technical,
                QObject::tr(
                    "Checking class notes parent class failed for %1: %2"
                    ).arg(identity, fetchError.text())
                });
        }
    }

    if (!parentClassExists)
    {
        return failAndRollback(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr(
                "Saving class notes failed: parent class id %1 was not found."
                ).arg(classId)
            });
    }

    const RepositoryStatus saved = writeClassNotes(
        classId,
        notes,
        timeFillerActivities
        );
    if (!saved)
    {
        return failAndRollback(saved.error());
    }

    if (!transaction.commit())
    {
        return failAndRollback(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr(
                "Committing class notes save failed for %1: %2"
                ).arg(identity, m_database.lastError().text())
            });
    }

    return {};
}

RepositoryStatus ClassInfoRepository::writeClassNotes(
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
        return repositoryFailure(executed.error().userMessage());
    }

    return {};
}

RepositoryStatus ClassInfoRepository::saveClassCoTeacherAssignment(
    const int classId,
    const std::optional<qint64> teacherId
    )
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr(
                "Saving class information failed: invalid class id %1."
                ).arg(classId),
            RepositoryErrorCode::InvalidInput
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return repositoryFailure(
            QObject::tr(
                "Starting class information save transaction failed for "
                "class id %1: %2"
                ).arg(classId)
                 .arg(m_database.lastError().text())
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    const auto failAndRollback = [&](RepositoryError failure) -> RepositoryStatus
    {
        return rollbackSaveFailure(
            transaction, m_database, std::move(failure),
            QObject::tr("Rolling back class information save failed for %1: %2")
                .arg(identity));
    };
    const auto parentExists = classDetailsParentExists(classId);
    if (!parentExists)
    {
        return failAndRollback(parentExists.error());
    }

    bool teacherMissing = false;
    if (*parentExists && teacherId.has_value())
    {
        QSqlQuery teacherQuery(m_database);
        teacherQuery.prepare(QStringLiteral("SELECT 1 FROM teachers WHERE id=?"));
        teacherQuery.addBindValue(*teacherId);
        const auto checked = SqlQueryUtils::executePrepared(
            teacherQuery, QObject::tr("Checking class co-teacher assignment teacher"),
            identity);
        if (!checked)
        {
            return failAndRollback(RepositoryError{
                RepositoryErrorCode::Technical, checked.error().userMessage()});
        }
        teacherMissing = !teacherQuery.next();
        const QSqlError fetchError = teacherQuery.lastError();
        teacherQuery.finish();
        if (fetchError.isValid())
        {
            return failAndRollback(RepositoryError{
                RepositoryErrorCode::Technical,
                QObject::tr("Checking class co-teacher assignment teacher failed for %1: %2")
                    .arg(identity, fetchError.text())});
        }
    }

    // Execute the original write to preserve its FK diagnostic. With foreign
    // keys disabled, missing class/teacher writes are explicitly rolled back.
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

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Saving class information"),
        identity
        );
    if (!executed)
    {
        query.finish();
        return failAndRollback(RepositoryError{
            !*parentExists || teacherMissing ? RepositoryErrorCode::NotFound
                                            : RepositoryErrorCode::Technical,
            executed.error().userMessage(),
            !*parentExists ? std::optional<bool>{false}
                : teacherMissing ? std::optional<bool>{true} : std::optional<bool>{}
            });
    }

    if (!*parentExists)
    {
        query.finish();
        return failAndRollback(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr(
                "Saving class information failed: parent class id %1 was not found."
                ).arg(classId),
            false
            });
    }

    if (teacherMissing)
    {
        query.finish();
        return failAndRollback(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr(
                "Saving class information failed: teacher id %1 was not found."
                ).arg(*teacherId),
            true
            });
    }

    if (!transaction.commit())
    {
        return failAndRollback(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr(
                "Committing class information failed for %1: %2"
                ).arg(identity, m_database.lastError().text())
            });
    }

    return {};
}

RepositoryStatus
ClassInfoRepository::saveClassCoTeacherAssignmentDetailed(
    const int classId,
    const std::optional<qint64> teacherId
    )
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr(
                "Saving class information failed: invalid class id %1."
                ).arg(classId),
            RepositoryErrorCode::InvalidInput
            );
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return repositoryFailure(
            QObject::tr(
                "Starting class information save transaction failed for "
                "class id %1: %2"
                ).arg(classId)
                 .arg(m_database.lastError().text()),
            RepositoryErrorCode::Technical
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    const auto failAndRollback = [&](RepositoryError failure) -> RepositoryStatus
    {
        return rollbackSaveFailure(
            transaction, m_database, std::move(failure),
            QObject::tr("Rolling back class information save failed for %1: %2")
                .arg(identity));
    };
    const auto parentExists = classDetailsParentExists(classId);
    if (!parentExists)
    {
        return failAndRollback(parentExists.error());
    }

    // Keep the original FK diagnostic and detailed missing-teacher precedence.
    // A statement for a missing class is always rolled back before returning.
    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO class_info (
            class_id,
            teacher_id
        )
        SELECT ?, ?
        WHERE ? IS NULL OR EXISTS (
            SELECT 1 FROM teachers WHERE id=?
        )

        ON CONFLICT(class_id)
        DO UPDATE SET
            teacher_id=excluded.teacher_id
    )");
    query.addBindValue(classId);
    query.addBindValue(
        teacherId ? QVariant(*teacherId) : QVariant()
        );
    query.addBindValue(
        teacherId ? QVariant(*teacherId) : QVariant()
        );
    query.addBindValue(
        teacherId ? QVariant(*teacherId) : QVariant()
        );

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        QObject::tr("Saving class information"),
        identity
        );
    if (!executed)
    {
        query.finish();
        return failAndRollback(RepositoryError{
            *parentExists ? RepositoryErrorCode::Technical
                          : RepositoryErrorCode::NotFound,
            executed.error().userMessage(),
            *parentExists ? std::optional<bool>{} : std::optional<bool>{false}
            });
    }

    if (query.numRowsAffected() == 0 && teacherId.has_value())
    {
        query.finish();
        return failAndRollback(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr(
                "Saving class information failed: teacher id %1 was not "
                "found."
                ).arg(teacherId.value_or(-1)),
            true
            });
    }

    if (!*parentExists)
    {
        query.finish();
        return failAndRollback(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr(
                "Saving class information failed: parent class id %1 was not found."
                ).arg(classId),
            false
            });
    }

    if (!transaction.commit())
    {
        return failAndRollback(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr(
                "Committing class information failed for %1: %2"
                ).arg(identity, m_database.lastError().text())
            });
    }

    return {};
}

namespace
{
RepositoryStatus assignExportTeacherId(ClassInfo& info, const QVariant& value)
{
    const auto teacherId = persistedTeacherId<int>(value, QObject::tr(
        "Loading class information failed: invalid assigned teacher identifier."));
    if (!teacherId)
    {
        return repositoryFailure(teacherId.error());
    }
    info.teacherId = *teacherId;
    return {};
}

RepositoryStatus assignExportTeacherId(
    ClassTransferExportClassInfoReadRecord& info, const QVariant& value)
{
    if (value.isNull())
    {
        info.teacherId = -1;
        return {};
    }
    bool valid = false;
    info.teacherId = value.toString().toLongLong(&valid);
    if (!valid)
        return repositoryFailure(
            QObject::tr(
                "Loading class information failed: invalid assigned teacher identifier."),
            RepositoryErrorCode::Validation
            );
    return {};
}

template<class Record>
RepositoryResult<Record> loadClassInfoRecord(
    QSqlDatabase& database,
    int classId,
    qint64* exactTeacherId = nullptr
    )
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(loadedInfo.error().userMessage());
    }

    if (query.next())
    {
        const QVariant teacherId = query.value("teacher_id");
        if (exactTeacherId)
        {
            const auto parsedTeacherId = persistedTeacherId<qint64>(
                teacherId, QObject::tr(
                    "Loading class information failed: invalid assigned teacher identifier."));
            if (!parsedTeacherId)
            {
                return repositoryFailure(parsedTeacherId.error());
            }
            *exactTeacherId = *parsedTeacherId;
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
            else
            {
                info.teacherId = static_cast<int>(*exactTeacherId);
            }
        }
        else if (const RepositoryStatus assigned = assignExportTeacherId(info, teacherId); !assigned)
        {
            return repositoryFailure(assigned.error());
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
    else if (query.lastError().isValid())
    {
        return repositoryFailure(
            SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading class information"),
                {},
                identity
                ).userMessage()
            );
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
        return repositoryFailure(loadedRegularTimes.error().userMessage());
    }

    while (query.next())
    {
        ClassTime time;

        time.day =       query.value("day").toString();
        time.startTime = query.value("start_time").toString();
        time.endTime =   query.value("end_time").toString();

        info.classTimes.append(time);
    }
    if (query.lastError().isValid())
    {
        return repositoryFailure(
            SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading regular class times"),
                {},
                identity
                ).userMessage()
            );
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
        return repositoryFailure(loadedIntensiveTimes.error().userMessage());
    }

    while (query.next())
    {
        ClassTime time;

        time.day =       query.value("day").toString();
        time.startTime = query.value("start_time").toString();
        time.endTime =   query.value("end_time").toString();

        info.intensiveTimes.append(time);
    }
    if (query.lastError().isValid())
    {
        return repositoryFailure(
            SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading intensive class times"),
                {},
                identity
                ).userMessage()
            );
    }

    return info;
}

}

RepositoryResult<ClassInfo> ClassInfoRepository::loadClassInfo(int classId)
{
    ++m_scheduleClassInfoReadMetrics.singleClassInfoReadCount;
    return loadClassInfoRecord<ClassInfo>(m_database, classId);
}

RepositoryResult<ClassDetailsSaveReadRecord> ClassInfoRepository::loadClassDetailsSaveInfo(
    const int classId
    )
{
    ClassDetailsSaveReadRecord record;
    const auto loaded = loadClassInfoRecord<ClassInfo>(
        m_database, classId, &record.teacherId);
    if (!loaded)
    {
        return repositoryFailure(loaded.error());
    }
    record.info = *loaded;
    return record;
}

RepositoryResult<ClassTransferExportClassInfoReadRecord>
ClassInfoRepository::loadClassTransferExportClassInfoRecord(int classId)
{
    return loadClassInfoRecord<ClassTransferExportClassInfoReadRecord>(
        m_database, classId);
}

RepositoryResult<ClassCoTeacherAssignmentValidationSnapshotReadRecord>
ClassInfoRepository::loadClassCoTeacherAssignmentValidationSnapshot(
    const int classId
    )
{
    const auto loaded =
        loadClassCoTeacherAssignmentValidationSnapshotDetailed(classId);
    if (!loaded)
    {
        return repositoryFailure(loaded.error());
    }

    return *loaded;
}

RepositoryResult<ClassCoTeacherAssignmentValidationSnapshotReadRecord>
ClassInfoRepository::loadClassCoTeacherAssignmentValidationSnapshotDetailed(
    const int classId
    )
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr(
                "Loading class information failed: invalid class id %1."
                ).arg(classId),
            RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(
            loadedInfo.error().userMessage(),
            RepositoryErrorCode::Technical
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
            return repositoryFailure(
                readError.userMessage(),
                RepositoryErrorCode::Technical
                );
        }

        return repositoryFailure(
            QObject::tr(
                "Loading class information failed for %1: class not found."
                ).arg(identity),
            RepositoryErrorCode::NotFound
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
        return repositoryFailure(
            loadedRegularTimes.error().userMessage(),
            RepositoryErrorCode::Technical
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
    if (query.lastError().isValid())
    {
        return repositoryFailure(
            SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading regular class times"),
                {},
                identity
                ).userMessage(),
            RepositoryErrorCode::Technical
            );
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
        return repositoryFailure(
            loadedIntensiveTimes.error().userMessage(),
            RepositoryErrorCode::Technical
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
    if (query.lastError().isValid())
    {
        return repositoryFailure(
            SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading intensive class times"),
                {},
                identity
                ).userMessage(),
            RepositoryErrorCode::Technical
            );
    }

    return snapshot;
}

RepositoryResult<ScheduleEditorClassInfoReadRecord>
ClassInfoRepository::loadScheduleEditorClassInfoRecord(const int classId)
{
    const auto loaded = loadScheduleEditorClassInfoRecordDetailed(classId);
    if (!loaded)
    {
        return repositoryFailure(loaded.error());
    }

    return std::move(loaded->record);
}

RepositoryResult<ScheduleEditorClassInfoReadOutcome>
ClassInfoRepository::loadScheduleEditorClassInfoRecordDetailed(
    const int classId
    )
{
    ++m_scheduleEditorClassInfoReadMetrics.callCount;
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(loaded.error().userMessage());
    }

    const bool rowFound = query.next();
    if (!rowFound)
    {
        if (query.lastError().isValid())
        {
            return repositoryFailure(SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading class information"),
                {},
                identity
                ).userMessage());
        }

        return repositoryFailure(QObject::tr(
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
        return repositoryFailure(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading class information"),
            {},
            identity
            ).userMessage());
    }

    return outcome;
}

RepositoryResult<SelectedClassGradeReadRecord>
ClassInfoRepository::loadSelectedClassGradeRecord(const int classId)
{
    const auto loaded = loadSelectedClassGradeRecordDetailed(classId);
    if (!loaded)
    {
        return repositoryFailure(loaded.error());
    }

    return std::move(loaded->record);
}

RepositoryResult<SelectedClassGradeReadOutcome>
ClassInfoRepository::loadSelectedClassGradeRecordDetailed(const int classId)
{
    ++m_selectedClassGradeReadMetrics.callCount;
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(loaded.error().userMessage());
    }

    const bool parentRowFound = query.next();
    if (query.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
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

RepositoryResult<ClassPageDetailsReadRecord> ClassInfoRepository::loadClassPageDetails(
    const int classId
    )
{
    ++m_classPageDetailsReadMetrics.callCount;
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    if (metadataQuery.next())
    {
        const QVariant teacherId = metadataQuery.value("teacher_id");
        const auto parsedTeacherId = persistedTeacherId<qint64>(
            teacherId, QObject::tr(
                "Loading class information failed: invalid assigned teacher identifier."));
        if (!parsedTeacherId)
        {
            return repositoryFailure(parsedTeacherId.error());
        }
        record.teacherId = *parsedTeacherId;
        record.classGrade = metadataQuery.value("class_grade").toString();
        record.classLevel = metadataQuery.value("class_level").toString();
        record.notes = metadataQuery.value("notes").toString();
        record.timeFillerActivities = metadataQuery.value(
            "time_filler_activities"
            ).toString();
    }
    if (metadataQuery.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
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
    ++m_classPageDetailsReadMetrics.regularScheduleStatementCount;
    const auto loadedRegularSchedule = SqlQueryUtils::executePrepared(
        regularScheduleQuery,
        QObject::tr("Loading regular class times"),
        identity
        );
    if (!loadedRegularSchedule)
    {
        return repositoryFailure(loadedRegularSchedule.error().userMessage());
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
        return repositoryFailure(SqlQueryUtils::errorFor(
            regularScheduleQuery,
            QObject::tr("Loading regular class times"),
            {},
            identity
            ).userMessage());
    }

    return record;
}

RepositoryResult<ClassPageDetailsReadOutcome>
ClassInfoRepository::loadClassPageDetailsDetailed(const int classId)
{
    ++m_classPageDetailsReadMetrics.callCount;
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    const bool parentRowFound = metadataQuery.next();
    if (metadataQuery.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
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
    const auto parsedTeacherId = persistedTeacherId<qint64>(
        teacherId, QObject::tr(
            "Loading class information failed: invalid assigned teacher identifier."));
    if (!parsedTeacherId)
    {
        return repositoryFailure(parsedTeacherId.error());
    }
    record.teacherId = *parsedTeacherId;
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
        return repositoryFailure(loadedRegularSchedule.error().userMessage());
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
        return repositoryFailure(SqlQueryUtils::errorFor(
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

RepositoryResult<ClassCoTeacherPageDetailsReadRecord>
ClassInfoRepository::loadClassCoTeacherPageDetails(const int classId)
{
    const auto loaded = loadClassCoTeacherPageDetailsDetailed(classId);
    if (!loaded)
    {
        return repositoryFailure(loaded.error());
    }

    return std::move(loaded->record);
}

RepositoryResult<ClassCoTeacherPageDetailsReadOutcome>
ClassInfoRepository::loadClassCoTeacherPageDetailsDetailed(const int classId)
{
    return loadBoundedClassCoTeacherPageDetailsDetailed(
        classId,
        std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max());
}

RepositoryResult<ClassCoTeacherPageDetailsReadOutcome>
ClassInfoRepository::loadBoundedClassCoTeacherPageDetailsDetailed(
    const int classId,
    const int maximumRows,
    const int maximumScheduleTokenLength
    )
{
    ++m_classPageDetailsReadMetrics.callCount;
    if (classId <= 0 || maximumRows <= 0 || maximumScheduleTokenLength < 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class information failed: invalid class id or read bound for %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    const bool parentRowFound = metadataQuery.next();
    if (metadataQuery.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
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
            record.teacherId = teacherId.toString().toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return repositoryFailure(
                    QObject::tr(
                        "Loading class information failed: invalid co-teacher id for %1."
                        ).arg(identity),
                    RepositoryErrorCode::Validation
                    );
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
        WITH limits AS (
            SELECT ? AS max_token_length, ? AS max_token_bytes
        ), schedule_rows AS (
            SELECT day, start_time
            FROM class_times
            WHERE class_id = ?
            ORDER BY id
            LIMIT ?
        ), sized_rows AS (
            SELECT schedule_rows.*,
                   length(COALESCE(day, '')) AS day_length,
                   length(COALESCE(start_time, '')) AS start_length,
                   length(CAST(COALESCE(day, '') AS BLOB)) AS day_bytes,
                   length(CAST(COALESCE(start_time, '') AS BLOB)) AS start_bytes
            FROM schedule_rows
        )
        SELECT CASE WHEN day_length <= limits.max_token_length
                          AND day_bytes <= limits.max_token_bytes
                    THEN day ELSE NULL END AS day,
               CASE WHEN start_length <= limits.max_token_length
                          AND start_bytes <= limits.max_token_bytes
                    THEN start_time ELSE NULL END AS start_time,
               (day_length > limits.max_token_length
                OR start_length > limits.max_token_length
                OR day_bytes > limits.max_token_bytes
                OR start_bytes > limits.max_token_bytes) AS token_exceeds_bound
        FROM sized_rows CROSS JOIN limits
    )");
    regularScheduleQuery.addBindValue(maximumScheduleTokenLength);
    regularScheduleQuery.addBindValue(
        static_cast<qint64>(maximumScheduleTokenLength) * 4);
    regularScheduleQuery.addBindValue(classId);
    regularScheduleQuery.addBindValue(static_cast<qint64>(maximumRows) + 1);
    ++m_classPageDetailsReadMetrics.regularScheduleStatementCount;
    const auto loadedRegularSchedule = SqlQueryUtils::executePrepared(
        regularScheduleQuery,
        QObject::tr("Loading regular class times"),
        identity
        );
    if (!loadedRegularSchedule)
    {
        return repositoryFailure(loadedRegularSchedule.error().userMessage());
    }

    bool regularScheduleLimitExceeded = false;
    bool scheduleTokenLimitExceeded = false;
    while (regularScheduleQuery.next())
    {
        if (regularScheduleQuery.value(
                QStringLiteral("token_exceeds_bound")).toBool())
        {
            scheduleTokenLimitExceeded = true;
            continue;
        }
        const QString day = regularScheduleQuery.value("day").toString();
        const QString startTime = regularScheduleQuery.value("start_time").toString();
        if (day.size() > maximumScheduleTokenLength
            || startTime.size() > maximumScheduleTokenLength)
        {
            scheduleTokenLimitExceeded = true;
            continue;
        }
        if (record.regularTimes.size() >= maximumRows)
        {
            regularScheduleLimitExceeded = true;
            continue;
        }
        record.regularTimes.append({
            day,
            startTime
        });
    }

    if (regularScheduleQuery.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            regularScheduleQuery,
            QObject::tr("Loading regular class times"),
            {},
            identity
            ).userMessage());
    }

    return ClassCoTeacherPageDetailsReadOutcome{
        std::move(record),
        parentMissing,
        regularScheduleLimitExceeded,
        scheduleTokenLimitExceeded
    };
}

RepositoryResult<ClassNotesPageReadRecord>
ClassInfoRepository::loadClassNotesPageRecord(const int classId)
{
    const auto loaded = loadClassNotesPageRecordDetailed(classId);
    if (!loaded)
    {
        return repositoryFailure(loaded.error());
    }

    return std::move(loaded->record);
}

RepositoryResult<ClassNotesPageReadOutcome>
ClassInfoRepository::loadClassNotesPageRecordDetailed(const int classId)
{
    ++m_classNotesPageReadMetrics.callCount;
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    const bool parentRowFound = metadataQuery.next();
    if (!parentRowFound && metadataQuery.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
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
            const qint64 parsedTeacherId = teacherId.toString().toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return repositoryFailure(
                    QObject::tr(
                        "Loading class information failed: teacher id for class id %1 is not representable as a 64-bit integer."
                        ).arg(classId),
                    RepositoryErrorCode::Validation
                    );
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
        return repositoryFailure(SqlQueryUtils::errorFor(
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
        return repositoryFailure(loadedRegularSchedule.error().userMessage());
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
        return repositoryFailure(SqlQueryUtils::errorFor(
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

RepositoryResult<ClassNotesPageReadOutcome>
ClassInfoRepository::loadBoundedClassNotesPageRecordDetailed(
    const int classId,
    const int maximumTextCodeUnits,
    const int maximumMetadataFieldCodeUnits,
    const int maximumScheduleTokenLength
    )
{
    ++m_classNotesPageReadMetrics.callCount;
    if (classId <= 0 || maximumTextCodeUnits <= 0
        || maximumMetadataFieldCodeUnits <= 0
        || maximumScheduleTokenLength < 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class notes failed: one or more read bounds are invalid."),
            RepositoryErrorCode::InvalidInput);
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    ClassNotesPageReadRecord record;
    record.classId = classId;
    QSqlQuery metadataQuery(m_database);
    const QString metadataStatement = QStringLiteral(R"(
        WITH bounds AS (
            SELECT ? AS max_text_units, ? AS max_text_bytes,
                   ? AS max_metadata_units, ? AS max_metadata_bytes
        )
        SELECT class_info.teacher_id AS teacher_id,
               CASE WHEN length(COALESCE(class_info.class_grade, ''))
                              <= bounds.max_metadata_units
                          AND length(CAST(COALESCE(class_info.class_grade, '') AS BLOB))
                              <= bounds.max_metadata_bytes
                    THEN COALESCE(class_info.class_grade, '') ELSE NULL END AS class_grade,
               CASE WHEN length(COALESCE(class_info.class_level, ''))
                              <= bounds.max_metadata_units
                          AND length(CAST(COALESCE(class_info.class_level, '') AS BLOB))
                              <= bounds.max_metadata_bytes
                    THEN COALESCE(class_info.class_level, '') ELSE NULL END AS class_level,
               CASE WHEN length(COALESCE(class_info.notes, ''))
                              <= bounds.max_text_units
                          AND length(CAST(COALESCE(class_info.notes, '') AS BLOB))
                              <= bounds.max_text_bytes
                    THEN COALESCE(class_info.notes, '') ELSE NULL END AS notes,
               CASE WHEN length(COALESCE(class_info.time_filler_activities, ''))
                              <= bounds.max_text_units
                          AND length(CAST(COALESCE(class_info.time_filler_activities, '') AS BLOB))
                              <= bounds.max_text_bytes
                    THEN COALESCE(class_info.time_filler_activities, '')
                    ELSE NULL END AS time_filler_activities,
               (length(COALESCE(class_info.class_grade, ''))
                    > bounds.max_metadata_units
                OR length(CAST(COALESCE(class_info.class_grade, '') AS BLOB))
                    > bounds.max_metadata_bytes
                OR length(COALESCE(class_info.class_level, ''))
                    > bounds.max_metadata_units
                OR length(CAST(COALESCE(class_info.class_level, '') AS BLOB))
                    > bounds.max_metadata_bytes) AS metadata_limit_exceeded,
               (length(COALESCE(class_info.notes, ''))
                    > bounds.max_text_units
                OR length(CAST(COALESCE(class_info.notes, '') AS BLOB))
                    > bounds.max_text_bytes) AS notes_limit_exceeded,
               (length(COALESCE(class_info.time_filler_activities, ''))
                    > bounds.max_text_units
                OR length(CAST(COALESCE(class_info.time_filler_activities, '') AS BLOB))
                    > bounds.max_text_bytes) AS activities_limit_exceeded
        FROM classes
        LEFT JOIN class_info ON class_info.class_id = classes.id
        CROSS JOIN bounds
        WHERE classes.id = ?
    )");
    if (!metadataQuery.prepare(metadataStatement))
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            metadataQuery, QObject::tr("Loading class notes"),
            metadataStatement, identity).userMessage());
    }
    metadataQuery.addBindValue(maximumTextCodeUnits);
    metadataQuery.addBindValue(static_cast<qint64>(maximumTextCodeUnits) * 4);
    metadataQuery.addBindValue(maximumMetadataFieldCodeUnits);
    metadataQuery.addBindValue(
        static_cast<qint64>(maximumMetadataFieldCodeUnits) * 4);
    metadataQuery.addBindValue(classId);
    ++m_classNotesPageReadMetrics.metadataStatementCount;
    const auto loadedMetadata = SqlQueryUtils::executePrepared(
        metadataQuery, QObject::tr("Loading class notes"), identity);
    if (!loadedMetadata)
    {
        return repositoryFailure(loadedMetadata.error().userMessage());
    }
    if (!metadataQuery.next())
    {
        if (metadataQuery.lastError().isValid())
        {
            return repositoryFailure(SqlQueryUtils::errorFor(
                metadataQuery, QObject::tr("Loading class notes"),
                metadataStatement, identity).userMessage());
        }
        return ClassNotesPageReadOutcome{.record = std::move(record),
            .parentMissing = true};
    }

    bool metadataLimitExceeded = metadataQuery.value(
        QStringLiteral("metadata_limit_exceeded")).toBool();
    bool notesLimitExceeded = metadataQuery.value(
        QStringLiteral("notes_limit_exceeded")).toBool();
    bool activitiesLimitExceeded = metadataQuery.value(
        QStringLiteral("activities_limit_exceeded")).toBool();
    const QVariant persistedTeacherId = metadataQuery.value(
        QStringLiteral("teacher_id"));
    if (persistedTeacherId.isNull())
    {
        record.teacherId = -1;
    }
    else
    {
        bool teacherIdOk = false;
        record.teacherId = persistedTeacherId.toString().toLongLong(&teacherIdOk);
        if (!teacherIdOk)
        {
            return repositoryFailure(
                QObject::tr("Loading class notes failed: teacher id for %1 is invalid.")
                    .arg(identity), RepositoryErrorCode::Validation);
        }
    }

    if (!metadataLimitExceeded)
    {
        record.classGrade = metadataQuery.value(
            QStringLiteral("class_grade")).toString();
        record.classLevel = metadataQuery.value(
            QStringLiteral("class_level")).toString();
        metadataLimitExceeded = record.classGrade.size()
                > maximumMetadataFieldCodeUnits
            || record.classLevel.size() > maximumMetadataFieldCodeUnits;
    }
    if (!notesLimitExceeded)
    {
        record.notes = metadataQuery.value(QStringLiteral("notes")).toString();
        notesLimitExceeded = record.notes.size() > maximumTextCodeUnits;
        if (notesLimitExceeded)
        {
            record.notes.clear();
        }
    }
    if (!activitiesLimitExceeded)
    {
        record.timeFillerActivities = metadataQuery.value(
            QStringLiteral("time_filler_activities")).toString();
        activitiesLimitExceeded =
            record.timeFillerActivities.size() > maximumTextCodeUnits;
        if (activitiesLimitExceeded)
        {
            record.timeFillerActivities.clear();
        }
    }

    bool scheduleTokenLimitExceeded = false;
    if (!metadataLimitExceeded)
    {
        QSqlQuery scheduleQuery(m_database);
        const QString scheduleStatement = QStringLiteral(R"(
            WITH bounds AS (SELECT ? AS max_token_units, ? AS max_token_bytes)
            SELECT CASE WHEN length(COALESCE(day, '')) <= bounds.max_token_units
                             AND length(CAST(COALESCE(day, '') AS BLOB))
                                 <= bounds.max_token_bytes
                        THEN COALESCE(day, '') ELSE NULL END AS day,
                   CASE WHEN length(COALESCE(start_time, '')) <= bounds.max_token_units
                             AND length(CAST(COALESCE(start_time, '') AS BLOB))
                                 <= bounds.max_token_bytes
                        THEN COALESCE(start_time, '') ELSE NULL END AS start_time,
                   (length(COALESCE(day, '')) > bounds.max_token_units
                    OR length(CAST(COALESCE(day, '') AS BLOB))
                        > bounds.max_token_bytes
                    OR length(COALESCE(start_time, '')) > bounds.max_token_units
                    OR length(CAST(COALESCE(start_time, '') AS BLOB))
                        > bounds.max_token_bytes) AS token_limit_exceeded
            FROM class_times CROSS JOIN bounds
            WHERE class_id = ?
            ORDER BY id
            LIMIT 1
        )");
        if (!scheduleQuery.prepare(scheduleStatement))
        {
            return repositoryFailure(SqlQueryUtils::errorFor(
                scheduleQuery, QObject::tr("Loading regular class times"),
                scheduleStatement, identity).userMessage());
        }
        scheduleQuery.addBindValue(maximumScheduleTokenLength);
        scheduleQuery.addBindValue(
            static_cast<qint64>(maximumScheduleTokenLength) * 4);
        scheduleQuery.addBindValue(classId);
        ++m_classNotesPageReadMetrics.regularScheduleStatementCount;
        const auto loadedSchedule = SqlQueryUtils::executePrepared(
            scheduleQuery, QObject::tr("Loading regular class times"), identity);
        if (!loadedSchedule)
        {
            return repositoryFailure(loadedSchedule.error().userMessage());
        }
        if (scheduleQuery.next())
        {
            scheduleTokenLimitExceeded = scheduleQuery.value(
                QStringLiteral("token_limit_exceeded")).toBool();
            if (!scheduleTokenLimitExceeded)
            {
                ClassNotesPageRegularTime firstTime{
                    scheduleQuery.value(QStringLiteral("day")).toString(),
                    scheduleQuery.value(QStringLiteral("start_time")).toString()
                };
                scheduleTokenLimitExceeded = firstTime.day.size()
                        > maximumScheduleTokenLength
                    || firstTime.startTime.size() > maximumScheduleTokenLength;
                if (!scheduleTokenLimitExceeded)
                {
                    record.regularTimes.append(std::move(firstTime));
                }
            }
        }
        if (scheduleQuery.lastError().isValid())
        {
            return repositoryFailure(SqlQueryUtils::errorFor(
                scheduleQuery, QObject::tr("Loading regular class times"),
                scheduleStatement, identity).userMessage());
        }
    }

    return ClassNotesPageReadOutcome{
        std::move(record), false, metadataLimitExceeded, notesLimitExceeded,
        activitiesLimitExceeded, scheduleTokenLimitExceeded
    };
}

RepositoryResult<ClassDetailsPageReadRecord>
ClassInfoRepository::loadClassDetailsPageRecord(const int classId)
{
    const auto loaded = loadClassDetailsPageRecordDetailed(classId);
    if (!loaded)
    {
        return repositoryFailure(loaded.error());
    }

    return std::move(loaded->record);
}

RepositoryResult<ClassDetailsPageReadOutcome>
ClassInfoRepository::loadClassDetailsPageRecordDetailed(const int classId)
{
    return loadBoundedClassDetailsPageRecordDetailed(
        classId, std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max());
}

RepositoryResult<ClassDetailsPageReadOutcome>
ClassInfoRepository::loadBoundedClassDetailsPageRecordDetailed(
    const int classId,
    const int maximumRowsPerSource,
    const int maximumScheduleTokenLength
    )
{
    ++m_classDetailsPageReadMetrics.callCount;
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class information failed: invalid class id %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
            );
    }
    if (maximumRowsPerSource <= 0 || maximumScheduleTokenLength < 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class information failed: the schedule row bound must be positive."),
            RepositoryErrorCode::InvalidInput);
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
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    const bool parentRowFound = metadataQuery.next();
    if (!parentRowFound && metadataQuery.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
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
                teacherId.toString().toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return repositoryFailure(
                    QObject::tr(
                        "Loading class information failed: teacher id for class id %1 is not representable as a 64-bit integer."
                        ).arg(classId),
                    RepositoryErrorCode::Validation
                    );
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
        WITH limits AS (
            SELECT ? AS max_token_length, ? AS max_token_bytes
        ),
        regular_rows AS (
            SELECT 0 AS schedule_type, id, day, start_time, end_time
            FROM class_times
            WHERE class_id = ?
            ORDER BY id
            LIMIT ?
        ),
        intensive_rows AS (
            SELECT 1 AS schedule_type, id, day, start_time, end_time
            FROM class_intensive_times
            WHERE class_id = ?
            ORDER BY id
            LIMIT ?
        ),
        schedule_rows AS (
            SELECT * FROM regular_rows
            UNION ALL
            SELECT * FROM intensive_rows
        ),
        sized_rows AS (
            SELECT schedule_rows.*,
                   length(COALESCE(day, '')) AS day_length,
                   length(COALESCE(start_time, '')) AS start_length,
                   length(COALESCE(end_time, '')) AS end_length,
                   length(CAST(COALESCE(day, '') AS BLOB)) AS day_bytes,
                   length(CAST(COALESCE(start_time, '') AS BLOB)) AS start_bytes,
                   length(CAST(COALESCE(end_time, '') AS BLOB)) AS end_bytes
            FROM schedule_rows
        )
        SELECT schedule_type, id,
               CASE WHEN day_length <= limits.max_token_length
                          AND day_bytes <= limits.max_token_bytes
                    THEN day ELSE NULL END AS day,
               CASE WHEN start_length <= limits.max_token_length
                          AND start_bytes <= limits.max_token_bytes
                    THEN start_time ELSE NULL END AS start_time,
               CASE WHEN end_length <= limits.max_token_length
                          AND end_bytes <= limits.max_token_bytes
                    THEN end_time ELSE NULL END AS end_time,
               (day_length > limits.max_token_length
                OR start_length > limits.max_token_length
                OR end_length > limits.max_token_length
                OR day_bytes > limits.max_token_bytes
                OR start_bytes > limits.max_token_bytes
                OR end_bytes > limits.max_token_bytes) AS token_exceeds_bound
        FROM sized_rows CROSS JOIN limits
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
        return repositoryFailure(scheduleErrorMessage(error));
    }
    scheduleQuery.addBindValue(maximumScheduleTokenLength);
    scheduleQuery.addBindValue(static_cast<qint64>(maximumScheduleTokenLength) * 4);
    scheduleQuery.addBindValue(classId);
    scheduleQuery.addBindValue(static_cast<qint64>(maximumRowsPerSource) + 1);
    scheduleQuery.addBindValue(classId);
    scheduleQuery.addBindValue(static_cast<qint64>(maximumRowsPerSource) + 1);
    const auto loadedSchedules = SqlQueryUtils::executePrepared(
        scheduleQuery,
        QObject::tr("Loading regular class times"),
        identity
        );
    if (!loadedSchedules)
    {
        return repositoryFailure(scheduleErrorMessage(loadedSchedules.error()));
    }

    bool regularScheduleLimitExceeded = false;
    bool intensiveScheduleLimitExceeded = false;
    bool scheduleTokenLimitExceeded = false;
    while (scheduleQuery.next())
    {
        if (scheduleQuery.value(QStringLiteral("token_exceeds_bound")).toBool())
        {
            scheduleTokenLimitExceeded = true;
            continue;
        }
        ClassTime time;
        time.day = scheduleQuery.value("day").toString();
        time.startTime = scheduleQuery.value("start_time").toString();
        time.endTime = scheduleQuery.value("end_time").toString();
        if (time.day.size() > maximumScheduleTokenLength
            || time.startTime.size() > maximumScheduleTokenLength
            || time.endTime.size() > maximumScheduleTokenLength)
        {
            scheduleTokenLimitExceeded = true;
            continue;
        }
        if (scheduleQuery.value("schedule_type").toInt() == 0)
        {
            if (record.regularTimes.size() >= maximumRowsPerSource)
            {
                regularScheduleLimitExceeded = true;
            }
            else
            {
                record.regularTimes.append(time);
            }
        }
        else
        {
            if (record.intensiveTimes.size() >= maximumRowsPerSource)
            {
                intensiveScheduleLimitExceeded = true;
            }
            else
            {
                record.intensiveTimes.append(time);
            }
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
        return repositoryFailure(scheduleErrorMessage(error));
    }

    return ClassDetailsPageReadOutcome{
        std::move(record),
        parentMissing,
        regularScheduleLimitExceeded,
        intensiveScheduleLimitExceeded,
        scheduleTokenLimitExceeded
    };
}

namespace
{
template<class Record>
RepositoryResult<QList<Record>> loadClassInfoRecords(
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
            return repositoryFailure(
                QObject::tr(
                    "Loading class information records failed: class identifiers must be positive and unique."
                    ),
                RepositoryErrorCode::InvalidInput
                );
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
        return repositoryFailure(loadedMetadata.error().userMessage());
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
            return repositoryFailure(
                QObject::tr(
                    "Loading class information records failed: returned class order did not match the request."
                    ),
                RepositoryErrorCode::Validation
                );
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
        if (const RepositoryStatus assigned = assignExportTeacherId(info, teacherId); !assigned)
            return repositoryFailure(assigned.error());
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
        return repositoryFailure(metadataQuery.lastError().text());
    }
    if (metadataCount != classIds.size())
    {
        return repositoryFailure(
            QObject::tr(
                "Loading class information records failed: metadata returned an incomplete class list."
                ),
            RepositoryErrorCode::Validation
            );
    }

    const auto loadTimes = [&database, &classIds, &records, &requestedTable](
        const QString& table,
        const QString& action,
        const bool intensive
        ) -> RepositoryStatus
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
            return repositoryFailure(loaded.error().userMessage());
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
                return repositoryFailure(
                    QObject::tr(
                        "Loading class information records failed: returned schedule order did not match the request."
                        ),
                    RepositoryErrorCode::Validation
                    );
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
            return repositoryFailure(scheduleQuery.lastError().text());
        }
        return {};
    };

    if (const RepositoryStatus loadedRegular = loadTimes(
            QStringLiteral("class_times"),
            QObject::tr("Loading regular class information schedules"),
            false
            ); !loadedRegular)
    {
        return repositoryFailure(loadedRegular.error());
    }

    if (const RepositoryStatus loadedIntensive = loadTimes(
            QStringLiteral("class_intensive_times"),
            QObject::tr("Loading intensive class information schedules"),
            true
            ); !loadedIntensive)
    {
        return repositoryFailure(loadedIntensive.error());
    }

    return records;
}

}

RepositoryResult<QList<ClassInfo>> ClassInfoRepository::loadClassInfoRecords(
    const QList<int>& classIds)
{
    return ::loadClassInfoRecords<ClassInfo>(m_database, classIds);
}

RepositoryResult<QList<ClassTransferExportClassInfoReadRecord>>
ClassInfoRepository::loadClassTransferExportClassInfoRecords(
    const QList<int>& classIds)
{
    return ::loadClassInfoRecords<ClassTransferExportClassInfoReadRecord>(
        m_database, classIds);
}

RepositoryResult<ClassSubtitleReadRecord> ClassInfoRepository::loadClassSubtitleRecord(
    int classId
    )
{
    const auto loaded = loadClassSubtitleRecordDetailed(classId);
    if (!loaded)
    {
        return repositoryFailure(loaded.error());
    }

    return std::move(loaded->record);
}

RepositoryResult<ClassSubtitleReadOutcome>
ClassInfoRepository::loadClassSubtitleRecordDetailed(const int classId)
{
    return loadClassSubtitleRecordDetailed(classId, false);
}

RepositoryResult<ClassSubtitleReadOutcome>
ClassInfoRepository::loadClassSubtitleRecordDetailed(const int classId, bool validatePrintProjection)
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class subtitle failed: invalid class id %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(loadedFields.error().userMessage());
    }

    const bool parentRowFound = query.next();
    if (query.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading class subtitle details"),
            {},
            identity
            ).userMessage());
    }

    if (!parentRowFound)
    {
        return repositoryFailure(QObject::tr(
            "Loading class subtitle failed: no result was returned for class id %1."
            ).arg(classId));
    }

    outcome.parentMissing = query.value("parent_class_id").isNull();
    outcome.record.classId = query.value("requested_class_id").toInt();

    const QVariant teacherId = query.value("teacher_id");
    if (!teacherId.isNull())
    {
        bool teacherIdOk = false;
        outcome.record.teacherId = teacherId.toString().toLongLong(&teacherIdOk);
        if (!teacherIdOk)
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading class subtitle failed: the database returned an invalid teacher identifier."
                    ),
                RepositoryErrorCode::Validation
                );
        }
    }
    if (validatePrintProjection)
        for (const auto* field : {"class_grade", "class_level"})
        {
            const auto value = query.value(QLatin1String(field));
            if (!value.isNull() && value.metaType().id() != QMetaType::QString)
                return repositoryFailure(QObject::tr("Roster print data is malformed."), RepositoryErrorCode::Validation);
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
        return repositoryFailure(loadedTimes.error().userMessage());
    }

    while (query.next())
    {
        if (validatePrintProjection)
            for (int index = 0; index < 2; ++index)
            {
                const auto value = query.value(index);
                if (!value.isNull() && value.metaType().id() != QMetaType::QString)
                    return repositoryFailure(QObject::tr("Roster print data is malformed."), RepositoryErrorCode::Validation);
            }
        ClassTime time;
        time.day = query.value("day").toString();
        time.startTime = query.value("start_time").toString();
        time.endTime.clear();
        outcome.record.regularTimes.append(std::move(time));
    }

    if (query.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading class subtitle schedule"),
            {},
            identity
            ).userMessage());
    }

    return outcome;
}

RepositoryResult<QList<ClassSubtitleBatchReadRecord>>
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
            return repositoryFailure(
                QObject::tr(
                    "Loading class subtitles failed: class identifiers must be positive and unique."
                    ),
                RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    while (metadataQuery.next())
    {
        ClassSubtitleBatchReadRecord record;
        const auto parsedClassId = persistedClassId(
            metadataQuery.value("class_id"), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        record.classId = *parsedClassId;
        record.parentMissing = metadataQuery.value("parent_class_id").isNull();
        if (!record.parentMissing
            && !metadataQuery.value("class_info_class_id").isNull())
        {
            const QVariant teacherId = metadataQuery.value("teacher_id");
            if (!teacherId.isNull())
            {
                bool teacherIdOk = false;
                record.teacherId = teacherId.toString().toLongLong(&teacherIdOk);
                if (!teacherIdOk)
                {
                    return repositoryFailure(
                        QObject::tr(
                            "Loading class subtitles failed: the database returned an invalid teacher identifier."
                            ),
                        RepositoryErrorCode::Validation
                        );
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
        return repositoryFailure(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading class subtitle metadata")
            ).userMessage());
    }

    if (records.size() != classIds.size())
    {
        return repositoryFailure(
            QObject::tr(
                "Loading class subtitles failed: the metadata query returned an incomplete class list."
                ),
            RepositoryErrorCode::Validation
            );
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
        return repositoryFailure(loadedSchedule.error().userMessage());
    }

    while (scheduleQuery.next())
    {
        const auto parsedClassId = persistedClassId(
            scheduleQuery.value("class_id"), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        const int classId = *parsedClassId;
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
        return repositoryFailure(SqlQueryUtils::errorFor(
            scheduleQuery,
            QObject::tr("Loading class subtitle regular schedules")
            ).userMessage());
    }

    return records;
}

RepositoryResult<std::vector<MyClassesClassInformationBatchReadEntry>>
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
                .information = repositoryFailure(
                    QObject::tr(
                        "Loading My Classes class information failed: invalid class id %1."
                        ).arg(classId),
                    RepositoryErrorCode::InvalidInput
                    )
            });
            continue;
        }

        if (entryIndexByClassId.contains(classId))
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading My Classes class information failed: class identifiers must be unique."
                    ),
                RepositoryErrorCode::InvalidInput
                );
        }

        const std::size_t entryIndex = entries.size();
        entryIndexByClassId.insert(classId, entryIndex);
        validEntryIndexes.push_back(entryIndex);
        entries.push_back({
            .classId = classId,
            .information = repositoryFailure(QString{})
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
                entries[index].information = repositoryFailure(loaded.error());
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
        const auto parsedClassId = persistedClassId(
            metadataQuery.value("class_id"), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        const int classId = *parsedClassId;
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
                record.teacherId = teacherId.toString().toLongLong(&teacherIdOk);
            }

            if (!teacherIdOk)
            {
                entries[*entryIndex].information = repositoryFailure(
                    QObject::tr(
                        "Loading My Classes class information failed: teacher id for class id %1 cannot be represented as a signed 64-bit value."
                        ).arg(classId),
                    RepositoryErrorCode::Validation
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
        const auto parsedClassId = persistedClassId(
            regularScheduleQuery.value("class_id"), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        const int classId = *parsedClassId;
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
        const auto parsedClassId = persistedClassId(
            intensiveScheduleQuery.value("class_id"), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        const int classId = *parsedClassId;
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

RepositoryResult<std::vector<MyClassesClassSummaryBatchReadEntry>>
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
                .summary = repositoryFailure(
                    QObject::tr(
                        "Loading My Classes class summaries failed: invalid class id %1."
                        ).arg(classId),
                    RepositoryErrorCode::InvalidInput
                    )
            });
            continue;
        }

        if (entryIndexByClassId.contains(classId))
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading My Classes class summaries failed: class identifiers must be unique."
                    ),
                RepositoryErrorCode::InvalidInput
                );
        }

        const std::size_t entryIndex = entries.size();
        entryIndexByClassId.insert(classId, entryIndex);
        validEntryIndexes.push_back(entryIndex);
        entries.push_back({
            .classId = classId,
            .summary = repositoryFailure(QString{})
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
                entries[index].summary = repositoryFailure(loaded.error());
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
        const auto parsedClassId = persistedClassId(
            metadataQuery.value("class_id"), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        const int classId = *parsedClassId;
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
                record.teacherId = teacherId.toString().toLongLong(&teacherIdOk);
            }

            if (!teacherIdOk)
            {
                entries[*entryIndex].summary = repositoryFailure(
                    QObject::tr(
                        "Loading My Classes class summary failed: teacher id for class id %1 cannot be represented as a signed 64-bit value."
                        ).arg(classId),
                    RepositoryErrorCode::Validation
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
        const auto parsedClassId = persistedClassId(
            scheduleQuery.value("class_id"), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        const int classId = *parsedClassId;
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

RepositoryResult<ClassInfoRepository::MyClassesClassSummaryReadOutcome>
ClassInfoRepository::loadMyClassesClassSummaryRecord(
    const int classId
    )
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr(
                "Loading My Classes class summary failed: invalid class id %1."
                ).arg(classId),
            RepositoryErrorCode::InvalidInput
            );
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
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    const bool parentFound = metadataQuery.next();
    if (metadataQuery.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
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
                teacherId.toString().toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return repositoryFailure(
                    QObject::tr(
                        "Loading My Classes class summary failed: teacher id for class id %1 cannot be represented as a signed 64-bit value."
                        ).arg(classId),
                    RepositoryErrorCode::Validation
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
        return repositoryFailure(loadedSchedule.error().userMessage());
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
        return repositoryFailure(SqlQueryUtils::errorFor(
            scheduleQuery,
            QObject::tr("Loading My Classes class-summary schedules"),
            QObject::tr("class id %1").arg(classId)
            ).userMessage());
    }

    return outcome;
}

RepositoryResult<ClassInfoRepository::MyClassesClassInformationReadOutcome>
ClassInfoRepository::loadMyClassesClassInformationRecord(
    const int classId
    )
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr(
                "Loading My Classes class information failed: invalid class id %1."
                ).arg(classId),
            RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    const bool parentFound = metadataQuery.next();
    if (metadataQuery.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
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
                teacherId.toString().toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return repositoryFailure(
                    QObject::tr(
                        "Loading My Classes class information failed: teacher id for class id %1 cannot be represented as a signed 64-bit value."
                        ).arg(classId),
                    RepositoryErrorCode::Validation
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
        return repositoryFailure(loadedRegularSchedule.error().userMessage());
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
        return repositoryFailure(SqlQueryUtils::errorFor(
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
        return repositoryFailure(loadedIntensiveSchedule.error().userMessage());
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
        return repositoryFailure(SqlQueryUtils::errorFor(
            intensiveScheduleQuery,
            QObject::tr("Loading My Classes intensive schedules"),
            identity
            ).userMessage());
    }

    return outcome;
}

RepositoryResult<RosterPrintClassInfoReadRecord>
ClassInfoRepository::loadRosterPrintClassInfoRecord(
    int classId
    )
{
    const auto loaded = loadRosterPrintClassInfoRecordDetailed(classId);
    if (!loaded)
    {
        return repositoryFailure(loaded.error());
    }

    return std::move(loaded->record);
}

RepositoryResult<RosterPrintClassInfoReadOutcome>
ClassInfoRepository::loadRosterPrintClassInfoRecordDetailed(int classId)
{
    return loadRosterPrintClassInfoRecordDetailed(classId, true, true);
}

RepositoryResult<RosterPrintClassInfoReadOutcome>
ClassInfoRepository::loadRosterPrintClassInfoRecordDetailed(int classId, bool includeWifi, bool includeZoom, bool includeRegularSchedule)
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr("Loading roster print class information failed: invalid class id %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
            );
    }

    const QString identity = QObject::tr("class id %1").arg(classId);
    RosterPrintClassInfoReadOutcome outcome;
    outcome.record.classId = classId;

    QSqlQuery metadataQuery(m_database);
    metadataQuery.prepare(QStringLiteral(R"(
        WITH requested(class_id) AS (VALUES (?))
        SELECT
            requested.class_id AS requested_class_id,
            classes.id AS parent_class_id,
            ci.class_grade,
            ci.class_level,
            t.teacher_en,
            t.teacher_kr,
            t.room_number,
            %1 AS wifi_name,
            %2 AS wifi_password,
            %3 AS zoom_id,
            %4 AS zoom_password
        FROM requested
        LEFT JOIN classes ON classes.id = requested.class_id
        LEFT JOIN class_info ci ON ci.class_id = requested.class_id
        LEFT JOIN teachers t ON ci.teacher_id = t.id
    )").arg(includeWifi ? QStringLiteral("t.wifi_name") : QStringLiteral("NULL"),
        includeWifi ? QStringLiteral("t.wifi_password") : QStringLiteral("NULL"),
        includeZoom ? QStringLiteral("t.zoom_id") : QStringLiteral("NULL"),
        includeZoom ? QStringLiteral("t.zoom_password") : QStringLiteral("NULL")));
    metadataQuery.addBindValue(classId);

    const auto loadedMetadata = SqlQueryUtils::executePrepared(
        metadataQuery,
        QObject::tr("Loading roster print class information"),
        identity
        );
    if (!loadedMetadata)
    {
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    const bool parentRowFound = metadataQuery.next();
    if (metadataQuery.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading roster print class information"),
            {},
            identity
            ).userMessage());
    }

    if (!parentRowFound)
    {
        return repositoryFailure(QObject::tr(
            "Loading roster print class information failed: no result was returned for class id %1."
            ).arg(classId));
    }

    for (int index = 2; index < 11; ++index)
    {
        const auto value = metadataQuery.value(index);
        if (!value.isNull() && value.metaType().id() != QMetaType::QString)
            return repositoryFailure(QObject::tr("Roster print data is malformed."), RepositoryErrorCode::Validation);
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

    if (!includeRegularSchedule) return outcome;

    QSqlQuery scheduleQuery(m_database);
    scheduleQuery.prepare(R"(
        WITH requested(class_id) AS (VALUES (?))
        SELECT schedule.day, schedule.start_time, schedule.end_time
        FROM requested
        INNER JOIN class_times schedule
            ON schedule.class_id = requested.class_id
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
        return repositoryFailure(loadedSchedule.error().userMessage());
    }

    while (scheduleQuery.next())
    {
        for (int index = 0; index < 3; ++index)
        {
            const auto value = scheduleQuery.value(index);
            if (!value.isNull() && value.metaType().id() != QMetaType::QString)
                return repositoryFailure(QObject::tr("Roster print data is malformed."), RepositoryErrorCode::Validation);
        }
        outcome.record.regularTimes.append({
            scheduleQuery.value("day").toString(),
            scheduleQuery.value("start_time").toString(),
            scheduleQuery.value("end_time").toString()
        });
    }

    if (scheduleQuery.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            scheduleQuery,
            QObject::tr("Loading roster print regular class times"),
            {},
            identity
            ).userMessage());
    }

    return outcome;
}

RepositoryResult<QList<RosterPrintClassInfoReadRecord>>
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
            return repositoryFailure(
                QObject::tr(
                    "Loading roster print class information failed: class ids must be positive and unique."
                    ),
                RepositoryErrorCode::InvalidInput
                );
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
        return repositoryFailure(loadedMetadata.error().userMessage());
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
        bool infoIdentityValid = false;
        const qint64 infoIdentity = metadataQuery.value("class_info_class_id")
            .toString().toLongLong(&infoIdentityValid);
        if (!infoIdentityValid || infoIdentity != classId)
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
        return repositoryFailure(
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
        return repositoryFailure(
            QObject::tr(
                "Loading roster print class information returned an invalid class identity."
                ),
            RepositoryErrorCode::Validation
            );
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
        return repositoryFailure(loadedSchedule.error().userMessage());
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
        return repositoryFailure(
            QObject::tr("Reading roster print regular schedules failed: %1")
                .arg(scheduleQuery.lastError().text())
            );
    }
    if (invalidScheduleIdentity)
    {
        return repositoryFailure(
            QObject::tr(
                "Loading roster print regular schedules returned an invalid class identity."
                ),
            RepositoryErrorCode::Validation
            );
    }

    return records;
}

RepositoryResult<QList<SubPrepRosterOutputClassInfoReadRecord>>
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
            return repositoryFailure(
                QObject::tr(
                    "Loading Sub Prep roster output class information failed: class identifiers must be positive and unique."
                    ),
                RepositoryErrorCode::InvalidInput
                );
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
        return repositoryFailure(loaded.error().userMessage());
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
            return repositoryFailure(
                QObject::tr(
                    "Loading Sub Prep roster output class information failed: returned class order did not match the request."
                    ),
                RepositoryErrorCode::Validation
                );
        }

        SubPrepRosterOutputClassInfoReadRecord record;
        const QVariant classInfoClassId = query.value(
            QStringLiteral("class_info_class_id")
            );
        if (classInfoClassId.isNull())
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading Sub Prep roster output class information failed for class id %1: no matching record exists."
                    ).arg(requestedClassId),
                RepositoryErrorCode::NotFound
                );
        }

        const auto parsedClassId = persistedClassId(
            classInfoClassId, QObject::tr(
                "Loading Sub Prep roster output class information failed: returned class identity did not match the request."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        record.classId = *parsedClassId;
        if (record.classId != requestedClassId)
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading Sub Prep roster output class information failed: returned class identity did not match the request."
                    ),
                RepositoryErrorCode::Validation
                );
        }

        const QVariant teacherId = query.value(
            QStringLiteral("teacher_id")
            );
        if (!teacherId.isNull())
        {
            bool teacherIdOk = false;
            const qint64 parsedTeacherId = teacherId.toString().toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return repositoryFailure(
                    QObject::tr(
                        "Loading Sub Prep roster output class information failed: the database returned an invalid teacher identifier."
                        ),
                    RepositoryErrorCode::Validation
                    );
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
        return repositoryFailure(query.lastError().text());
    }
    if (records.size() != classIds.size())
    {
        return repositoryFailure(
            QObject::tr(
                "Loading Sub Prep roster output class information failed: the database returned an incomplete result."
                ),
            RepositoryErrorCode::Validation
            );
    }

    return records;
}

RepositoryResult<QList<ClassNavigationReadRecord>>
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
            return repositoryFailure(
                QObject::tr(
                    "Loading classes navigation failed: class identifiers must be positive and unique."
                    ),
                RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    while (metadataQuery.next())
    {
        ClassNavigationReadRecord record;
        const auto parsedClassId = persistedClassId(
            metadataQuery.value("class_id"), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        record.classId = *parsedClassId;
        record.hasClassInfo =
            !metadataQuery.value("class_info_class_id").isNull();
        if (record.hasClassInfo)
        {
            const QVariant teacherId = metadataQuery.value("teacher_id");
            if (!teacherId.isNull())
            {
                bool teacherIdValid = false;
                const qint64 parsedTeacherId =
                    teacherId.toString().toLongLong(&teacherIdValid);
                if (!teacherIdValid)
                {
                    return repositoryFailure(
                        QObject::tr(
                            "Loading classes navigation failed: a teacher identifier is invalid."
                            ),
                        RepositoryErrorCode::Validation
                        );
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

    if (metadataQuery.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            metadataQuery,
            QObject::tr("Loading classes navigation metadata"),
            metadataQueryText
            ).userMessage());
    }

    if (records.size() != classIds.size())
    {
        return repositoryFailure(
            QObject::tr(
                "Loading classes navigation failed: the metadata query returned an incomplete class list."
                ),
            RepositoryErrorCode::Validation
            );
    }

    const auto loadTimes = [this, &indexByClassId, &records, &requestedValues](
        const QString& table,
        const QString& action,
        const bool intensive
        ) -> RepositoryStatus
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
            return repositoryFailure(loadedTimes.error().userMessage());
        }

        while (timesQuery.next())
        {
            const auto parsedClassId = persistedClassId(
                timesQuery.value("class_id"), QObject::tr(
                    "Loading class information failed: the database returned an invalid class identifier."));
            if (!parsedClassId)
            {
                return repositoryFailure(parsedClassId.error());
            }
            const int classId = *parsedClassId;
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

        if (timesQuery.lastError().isValid())
        {
            return repositoryFailure(SqlQueryUtils::errorFor(
                timesQuery,
                action,
                queryText
                ).userMessage());
        }

        return {};
    };

    if (const RepositoryStatus loadedRegular = loadTimes(
            QStringLiteral("class_times"),
            QObject::tr("Loading regular classes navigation schedules"),
            false
            ); !loadedRegular)
    {
        return repositoryFailure(loadedRegular.error());
    }

    if (const RepositoryStatus loadedIntensive = loadTimes(
            QStringLiteral("class_intensive_times"),
            QObject::tr("Loading intensive classes navigation schedules"),
            true
            ); !loadedIntensive)
    {
        return repositoryFailure(loadedIntensive.error());
    }

    return records;
}

RepositoryResult<QList<ClassNavigationParentAwareReadRecord>>
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
            return repositoryFailure(
                QObject::tr(
                    "Loading classes navigation failed: class identifiers must be positive and unique."
                    ),
                RepositoryErrorCode::InvalidInput
                );
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
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    while (metadataQuery.next())
    {
        ClassNavigationParentAwareReadRecord parentRecord;
        ClassNavigationReadRecord& record = parentRecord.details;
        const auto parsedClassId = persistedClassId(
            metadataQuery.value("class_id"), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        record.classId = *parsedClassId;
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
                    teacherId.toString().toLongLong(&teacherIdValid);
                if (!teacherIdValid)
                {
                    return repositoryFailure(
                        QObject::tr(
                            "Loading classes navigation failed: a teacher identifier is invalid."
                            ),
                        RepositoryErrorCode::Validation
                        );
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
        return repositoryFailure(metadataQuery.lastError().text());
    }

    const auto loadTimes = [this,
                            &indexByClassId,
                            &records,
                            &requestedValuesSql](
        const QString& table,
        const QString& action,
        const bool intensive
        ) -> RepositoryStatus
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
            return repositoryFailure(loadedTimes.error().userMessage());
        }

        while (timesQuery.next())
        {
            const auto parsedClassId = persistedClassId(
                timesQuery.value("class_id"), QObject::tr(
                    "Loading class information failed: the database returned an invalid class identifier."));
            if (!parsedClassId)
            {
                return repositoryFailure(parsedClassId.error());
            }
            const int classId = *parsedClassId;
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
            return repositoryFailure(timesQuery.lastError().text());
        }

        return {};
    };

    if (const RepositoryStatus loadedRegular = loadTimes(
            QStringLiteral("class_times"),
            QObject::tr("Loading regular classes navigation schedules"),
            false
            ); !loadedRegular)
    {
        return repositoryFailure(loadedRegular.error());
    }

    if (const RepositoryStatus loadedIntensive = loadTimes(
            QStringLiteral("class_intensive_times"),
            QObject::tr("Loading intensive classes navigation schedules"),
            true
            ); !loadedIntensive)
    {
        return repositoryFailure(loadedIntensive.error());
    }

    return records;
}

RepositoryResult<QList<ClassNavigationBoundedReadRecord>>
ClassInfoRepository::loadBoundedClassesNavigationRecords(
    const QList<ClassNavigationBoundedReadRequestEntry>& requestedClasses,
    const ClassNavigationBoundedReadLimits& limits
    )
{
    if (limits.maximumClassCount <= 0
        || limits.maximumClassCount == std::numeric_limits<int>::max()
        || limits.maximumTextLength < 0
        || limits.maximumScheduleRowsPerSourceAndClass < 0
        || limits.maximumScheduleTokenLength < 0)
    {
        return repositoryFailure(
            QObject::tr(
                "Loading bounded classes navigation failed: invalid read limits."
                ),
            RepositoryErrorCode::InvalidInput
            );
    }
    if (requestedClasses.size() > limits.maximumClassCount)
    {
        return repositoryFailure(
            QObject::tr(
                "Loading bounded classes navigation failed: the request exceeds its class limit."
                ),
            RepositoryErrorCode::Validation
            );
    }
    if (requestedClasses.isEmpty())
    {
        return QList<ClassNavigationBoundedReadRecord>{};
    }

    QSet<int> seenClassIds;
    QStringList requestedValues;
    requestedValues.reserve(requestedClasses.size());
    for (qsizetype index = 0; index < requestedClasses.size(); ++index)
    {
        const auto& requested = requestedClasses[index];
        if (requested.classId <= 0 || seenClassIds.contains(requested.classId))
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading bounded classes navigation failed: class identifiers must be positive and unique."
                    ),
                RepositoryErrorCode::InvalidInput
                );
        }
        if (requested.className.size() > limits.maximumTextLength)
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading bounded classes navigation failed: a requested class name exceeds its text limit."
                    ),
                RepositoryErrorCode::Validation
                );
        }

        seenClassIds.insert(requested.classId);
        requestedValues.append(QStringLiteral("(%1, %2)")
            .arg(requested.classId)
            .arg(index));
    }

    const QString requestedValuesSql =
        requestedValues.join(QStringLiteral(", "));
    QList<ClassNavigationBoundedReadRecord> records;
    records.reserve(requestedClasses.size());
    QHash<int, qsizetype> indexByClassId;
    indexByClassId.reserve(requestedClasses.size());

    QSqlQuery metadataQuery(m_database);
    metadataQuery.setForwardOnly(true);
    metadataQuery.prepare(QStringLiteral(R"(
        SELECT c.id AS class_id,
               c.name AS class_name,
               ci.class_id AS class_info_class_id,
               ci.teacher_id,
               ci.class_grade,
               ci.class_level,
               t.teacher_en,
               t.teacher_kr
        FROM classes c
        LEFT JOIN testing_classes tc
            ON tc.class_id = c.id
        LEFT JOIN class_info ci
            ON ci.class_id = c.id
        LEFT JOIN teachers t
            ON t.id = ci.teacher_id
        WHERE tc.class_id IS NULL
        ORDER BY c.name, c.id
        LIMIT ?
    )"));
    metadataQuery.addBindValue(limits.maximumClassCount + 1);
    const auto loadedMetadata = SqlQueryUtils::executePrepared(
        metadataQuery,
        QObject::tr("Loading bounded classes navigation metadata")
        );
    if (!loadedMetadata)
    {
        return repositoryFailure(loadedMetadata.error().userMessage());
    }

    qsizetype metadataIndex = 0;
    while (metadataQuery.next())
    {
        if (metadataIndex >= requestedClasses.size())
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading bounded classes navigation failed: the class list exceeds its limit or changed during the read."
                    ),
                RepositoryErrorCode::Validation
                );
        }

        const auto parsedClassId = persistedClassId(
            metadataQuery.value(QStringLiteral("class_id")),
            QObject::tr(
                "Loading bounded classes navigation failed: the database returned an invalid class identifier."
                )
            );
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        const QString className =
            metadataQuery.value(QStringLiteral("class_name")).toString();
        const auto& requested = requestedClasses[metadataIndex];
        if (className.size() > limits.maximumTextLength
            || requested.classId != *parsedClassId
            || requested.className != className)
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading bounded classes navigation failed: class identity or order changed during the read."
                    ),
                RepositoryErrorCode::Validation
                );
        }

        ClassNavigationBoundedReadRecord result;
        result.className = className;
        ClassNavigationReadRecord& details = result.details;
        details.classId = *parsedClassId;
        details.hasClassInfo = !metadataQuery.value(
            QStringLiteral("class_info_class_id")).isNull();
        if (details.hasClassInfo)
        {
            const QVariant teacherId =
                metadataQuery.value(QStringLiteral("teacher_id"));
            if (!teacherId.isNull())
            {
                bool teacherIdValid = false;
                const qint64 parsedTeacherId =
                    teacherId.toString().toLongLong(&teacherIdValid);
                if (!teacherIdValid)
                {
                    return repositoryFailure(
                        QObject::tr(
                            "Loading bounded classes navigation failed: a teacher identifier is invalid."
                            ),
                        RepositoryErrorCode::Validation
                        );
                }
                details.teacherId = parsedTeacherId;
            }
            details.grade = metadataQuery.value(
                QStringLiteral("class_grade")).toString();
            details.level = metadataQuery.value(
                QStringLiteral("class_level")).toString();
            details.teacherEnglishName = metadataQuery.value(
                QStringLiteral("teacher_en")).toString();
            details.teacherKoreanName = metadataQuery.value(
                QStringLiteral("teacher_kr")).toString();
            if (details.grade.size() > limits.maximumTextLength
                || details.level.size() > limits.maximumTextLength
                || details.teacherEnglishName.size() > limits.maximumTextLength
                || details.teacherKoreanName.size() > limits.maximumTextLength)
            {
                return repositoryFailure(
                    QObject::tr(
                        "Loading bounded classes navigation failed: summary text exceeds its limit."
                        ),
                    RepositoryErrorCode::Validation
                    );
            }
        }

        indexByClassId.insert(details.classId, records.size());
        records.append(std::move(result));
        ++metadataIndex;
    }
    if (metadataQuery.lastError().isValid())
    {
        return repositoryFailure(metadataQuery.lastError().text());
    }
    if (metadataIndex != requestedClasses.size())
    {
        return repositoryFailure(
            QObject::tr(
                "Loading bounded classes navigation failed: the class list changed during the read."
                ),
            RepositoryErrorCode::Validation
            );
    }

    const auto loadTimes = [this,
                            &records,
                            &indexByClassId,
                            &requestedValuesSql,
                            &limits,
                            &requestedClasses](
        const QString& table,
        const QString& action,
        const bool intensive
        ) -> RepositoryStatus
    {
        QSqlQuery timesQuery(m_database);
        timesQuery.setForwardOnly(true);
        timesQuery.prepare(QStringLiteral(R"(
            WITH requested(class_id, ordinal) AS (
                VALUES %1
            )
            SELECT schedule.class_id,
                   schedule.day,
                   schedule.start_time,
                   schedule.end_time
            FROM requested
            INNER JOIN classes parent
                ON parent.id = requested.class_id
            INNER JOIN class_info info
                ON info.class_id = parent.id
            INNER JOIN %2 schedule
                ON schedule.class_id = parent.id
            LEFT JOIN testing_classes tc
                ON tc.class_id = parent.id
            WHERE tc.class_id IS NULL
            ORDER BY requested.ordinal, schedule.id
            LIMIT ?
        )").arg(requestedValuesSql, table));
        const qint64 totalLimit =
            static_cast<qint64>(requestedClasses.size())
                * static_cast<qint64>(
                    limits.maximumScheduleRowsPerSourceAndClass)
            + 1;
        timesQuery.addBindValue(totalLimit);
        const auto loadedTimes = SqlQueryUtils::executePrepared(
            timesQuery,
            action
            );
        if (!loadedTimes)
        {
            return repositoryFailure(loadedTimes.error().userMessage());
        }

        QHash<int, int> rowsByClass;
        rowsByClass.reserve(requestedClasses.size());
        while (timesQuery.next())
        {
            const auto parsedClassId = persistedClassId(
                timesQuery.value(QStringLiteral("class_id")),
                QObject::tr(
                    "Loading bounded classes navigation failed: the database returned an invalid schedule class identifier."
                    )
                );
            if (!parsedClassId)
            {
                return repositoryFailure(parsedClassId.error());
            }
            const auto found = indexByClassId.constFind(*parsedClassId);
            if (found == indexByClassId.cend())
            {
                return repositoryFailure(
                    QObject::tr(
                        "Loading bounded classes navigation failed: schedule class identity changed during the read."
                        ),
                    RepositoryErrorCode::Validation
                    );
            }
            ClassNavigationReadRecord& details = records[*found].details;
            if (!details.hasClassInfo)
            {
                return repositoryFailure(
                    QObject::tr(
                        "Loading bounded classes navigation failed: schedule rows require class information."
                        ),
                    RepositoryErrorCode::Validation
                    );
            }
            const int rows = rowsByClass.value(*parsedClassId) + 1;
            rowsByClass.insert(*parsedClassId, rows);
            if (rows > limits.maximumScheduleRowsPerSourceAndClass)
            {
                return repositoryFailure(
                    QObject::tr(
                        "Loading bounded classes navigation failed: a class schedule exceeds its per-source row limit."
                        ),
                    RepositoryErrorCode::Validation
                    );
            }

            ClassTime time;
            time.day = timesQuery.value(QStringLiteral("day")).toString();
            time.startTime = timesQuery.value(
                QStringLiteral("start_time")).toString();
            time.endTime = timesQuery.value(
                QStringLiteral("end_time")).toString();
            if (time.day.size() > limits.maximumScheduleTokenLength
                || time.startTime.size() > limits.maximumScheduleTokenLength
                || time.endTime.size() > limits.maximumScheduleTokenLength)
            {
                return repositoryFailure(
                    QObject::tr(
                        "Loading bounded classes navigation failed: a schedule token exceeds its limit."
                        ),
                    RepositoryErrorCode::Validation
                    );
            }
            if (intensive)
            {
                details.intensiveTimes.append(std::move(time));
            }
            else
            {
                details.regularTimes.append(std::move(time));
            }
        }
        if (timesQuery.lastError().isValid())
        {
            return repositoryFailure(timesQuery.lastError().text());
        }
        return {};
    };

    if (const RepositoryStatus loadedRegular = loadTimes(
            QStringLiteral("class_times"),
            QObject::tr("Loading bounded regular classes navigation schedules"),
            false
            ); !loadedRegular)
    {
        return repositoryFailure(loadedRegular.error());
    }
    if (const RepositoryStatus loadedIntensive = loadTimes(
            QStringLiteral("class_intensive_times"),
            QObject::tr("Loading bounded intensive classes navigation schedules"),
            true
            ); !loadedIntensive)
    {
        return repositoryFailure(loadedIntensive.error());
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

RepositoryResult<SubPrepClassDetailsRecord>
ClassInfoRepository::loadSubPrepClassDetails(
    const int classId
    )
{
    const RepositoryResult<SubPrepClassDetailsReadOutcome> detailed =
        loadSubPrepClassDetailsDetailed(classId);
    if (!detailed)
    {
        return repositoryFailure(detailed.error());
    }

    if (detailed->parentMissing)
    {
        return repositoryFailure(
            QObject::tr(
                "Loading selected Sub Prep class details failed: no matching record exists for class id %1."
                ).arg(classId),
            RepositoryErrorCode::NotFound
            );
    }

    return detailed->record;
}

RepositoryResult<SubPrepClassDetailsReadOutcome>
ClassInfoRepository::loadSubPrepClassDetailsDetailed(
    const int classId
    )
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr(
                "Loading selected Sub Prep class details failed: invalid class id %1."
                ).arg(classId),
            RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(
            SqlQueryUtils::errorFor(
                query,
                action,
                queryText,
                identity
                ).userMessage(),
            RepositoryErrorCode::Technical
            );
    }
    query.addBindValue(classId);

    const auto executed = SqlQueryUtils::executePrepared(
        query,
        action,
        identity
        );
    if (!executed)
    {
        return repositoryFailure(
            executed.error().userMessage(),
            RepositoryErrorCode::Technical
            );
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return repositoryFailure(
                SqlQueryUtils::errorFor(
                    query,
                    action,
                    query.lastQuery(),
                    identity
                    ).userMessage(),
                RepositoryErrorCode::Technical
                );
        }

        return SubPrepClassDetailsReadOutcome{
            .parentMissing = true
        };
    }

    SubPrepClassDetailsRecord record;
    const auto parsedClassId = persistedClassId(
        query.value(QStringLiteral("class_id")), QObject::tr(
            "Loading class information failed: the database returned an invalid class identifier."));
    if (!parsedClassId)
    {
        return repositoryFailure(parsedClassId.error());
    }
    record.classId = *parsedClassId;
    record.classNotes = query.value(QStringLiteral("class_notes")).toString();

    // A missing assignment, or a stale assignment whose teacher row no longer
    // exists, follows the legacy empty-teacher details fallback.
    const QVariant assignedTeacherId =
        query.value(QStringLiteral("assigned_teacher_id"));
    const QVariant resolvedTeacherId =
        query.value(QStringLiteral("resolved_teacher_id"));
    const QString invalidTeacherDiagnostic = QObject::tr(
        "Loading selected Sub Prep class details failed: invalid assigned teacher identifier.");
    const auto assigned = persistedTeacherId<qint64>(
        assignedTeacherId, invalidTeacherDiagnostic);
    if (!assigned)
    {
        return repositoryFailure(assigned.error());
    }
    const auto resolved = persistedTeacherId<qint64>(
        resolvedTeacherId, invalidTeacherDiagnostic);
    if (!resolved)
    {
        return repositoryFailure(resolved.error());
    }
    const qint64 assignedTeacherIdValue = *assigned;
    const qint64 resolvedTeacherIdValue = *resolved;
    if (assignedTeacherIdValue > 0 && resolvedTeacherIdValue > 0
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

RepositoryResult<QList<SubPrepClassSummaryRecord>>
ClassInfoRepository::loadSubPrepClassSummaries(
    const QList<int>& classIds,
    const QStringList& selectedDays,
    const ScheduleType type,
    const int maxMeetingsPerClass,
    const int maxTotalMeetings
    )
{
    return loadSubPrepClassSummariesImpl(
        classIds,
        selectedDays,
        type,
        maxMeetingsPerClass,
        maxTotalMeetings,
        true
        );
}

RepositoryResult<QList<SubPrepClassSummaryNavigationReadRecord>>
ClassInfoRepository::loadSubPrepClassNavigationSummaries(
    const QList<int>& classIds,
    const QStringList& selectedDays,
    const ScheduleType type,
    const int maxMeetingsPerClass,
    const int maxTotalMeetings
    )
{
    RepositoryResult<QList<SubPrepClassSummaryRecord>> summaries =
        loadSubPrepClassSummariesImpl(
            classIds,
            selectedDays,
            type,
            maxMeetingsPerClass,
            maxTotalMeetings,
            false
            );
    if (!summaries)
    {
        return repositoryFailure(summaries.error());
    }

    QList<SubPrepClassSummaryNavigationReadRecord> navigationRecords;
    navigationRecords.reserve(summaries->size());
    for (SubPrepClassSummaryRecord& summary : *summaries)
    {
        navigationRecords.append({
            .classId = summary.classId,
            .teacherId = summary.teacherId,
            .classGrade = std::move(summary.classGrade),
            .classLevel = std::move(summary.classLevel),
            .teacherKr = std::move(summary.teacherKr),
            .teacherEn = std::move(summary.teacherEn),
            .teacherPreferredRomanization =
                std::move(summary.teacherPreferredRomanization),
            .teacherPreferredName = std::move(summary.teacherPreferredName),
            .studentCount = std::nullopt,
            .meetings = std::move(summary.meetings)
        });
    }
    return navigationRecords;
}

RepositoryResult<QList<SubPrepClassSummaryRecord>>
ClassInfoRepository::loadSubPrepClassSummariesImpl(
    const QList<int>& classIds,
    const QStringList& selectedDays,
    const ScheduleType type,
    const int maxMeetingsPerClass,
    const int maxTotalMeetings,
    const bool includeStudentCounts
    )
{
    if (classIds.isEmpty() || selectedDays.isEmpty())
    {
        return QList<SubPrepClassSummaryRecord>{};
    }

    constexpr qsizetype MaxClassIds = 4'096;
    if (classIds.size() > MaxClassIds)
    {
        return repositoryFailure(
            QObject::tr("Loading Sub Prep class summaries failed: class scope exceeds its limit."),
            RepositoryErrorCode::InvalidInput
            );
    }

    if (type != ScheduleType::Regular && type != ScheduleType::Intensive)
    {
        return repositoryFailure(
            QObject::tr("Loading Sub Prep class summaries failed: invalid schedule type."),
            RepositoryErrorCode::InvalidInput
            );
    }

    if (maxMeetingsPerClass < 0
        || maxMeetingsPerClass == std::numeric_limits<int>::max()
        || maxTotalMeetings < 0
        || maxTotalMeetings == std::numeric_limits<int>::max())
    {
        return repositoryFailure(
            QObject::tr("Loading Sub Prep class summaries failed: invalid meeting limit."),
            RepositoryErrorCode::InvalidInput
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
            return repositoryFailure(
                QObject::tr(
                    "Loading Sub Prep class summaries failed: class identifiers must be positive and unique."
                    ),
                RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(
            QObject::tr("Loading Sub Prep class summaries failed: invalid weekday scope."),
            RepositoryErrorCode::InvalidInput
            );
    }

    QSet<QString> seenDays;
    for (const QString& day : selectedDays)
    {
        if (!validDays.contains(day) || seenDays.contains(day))
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading Sub Prep class summaries failed: weekdays must be valid and unique."
                    ),
                RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(executed.error().userMessage());
    }

    while (query.next())
    {
        const auto parsedClassId = persistedClassId(
            query.value(QStringLiteral("class_id")), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        const int classId = *parsedClassId;
        auto index = indexesByClassId.constFind(classId);
        if (index == indexesByClassId.cend())
        {
            SubPrepClassSummaryRecord record;
            record.classId = classId;
            bool teacherIdOk = false;
            record.teacherId = query.value(
                QStringLiteral("teacher_id")
                ).toString().toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return repositoryFailure(
                    QObject::tr(
                        "Unable to load scoped Sub Prep class summaries for %1: a teacher identifier cannot be represented as a signed 64-bit value."
                        ).arg(identity),
                    RepositoryErrorCode::Validation
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
        return repositoryFailure(
            QObject::tr("Loading scoped Sub Prep class summaries failed for %1: %2")
                .arg(identity, query.lastError().text())
            );
    }

    if (summaries.isEmpty())
    {
        return summaries;
    }

    if (!includeStudentCounts)
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
            const qint64 storedClassId = rosterQuery.value(
                QStringLiteral("class_id")
                ).toString().toLongLong(&classIdOk);
            classIdOk = classIdOk && storedClassId > 0
                && storedClassId <= std::numeric_limits<int>::max();
            const int classId = classIdOk ? static_cast<int>(storedClassId) : -1;
            bool rowIndexOk = false;
            const qint64 rowIndex = rosterQuery.value(
                QStringLiteral("row_index")
                ).toString().toLongLong(&rowIndexOk);
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

RepositoryResult<QList<ClassInfo>> ClassInfoRepository::loadClassInfosForScheduleScope(
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
        return repositoryFailure(
            QObject::tr("Loading scoped class information failed: invalid schedule type."),
            RepositoryErrorCode::InvalidInput
            );
    }

    if (maxMeetingsPerClass < 0
        || maxMeetingsPerClass == std::numeric_limits<int>::max()
        || maxTotalMeetings < 0
        || maxTotalMeetings == std::numeric_limits<int>::max())
    {
        return repositoryFailure(
            QObject::tr("Loading scoped class information failed: invalid meeting limit."),
            RepositoryErrorCode::InvalidInput
            );
    }

    QSet<int> seenClassIds;
    for (const int classId : classIds)
    {
        if (classId <= 0 || seenClassIds.contains(classId))
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading scoped class information failed: class identifiers must be positive and unique."
                    ),
                RepositoryErrorCode::InvalidInput
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
            return repositoryFailure(
                QObject::tr(
                    "Loading scoped class information failed: selected weekdays must be valid and unique."
                    ),
                RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(executed.error().userMessage());
    }

    while (query.next())
    {
        const auto parsedClassId = persistedClassId(
            query.value(QStringLiteral("schedule_class_id")), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        const int classId = *parsedClassId;
        const QVariant infoClassId =
            query.value(QStringLiteral("info_class_id"));
        if (infoClassId.isNull())
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading scoped Sub Prep class information failed for class id %1: no matching class information record exists."
                    ).arg(classId),
                RepositoryErrorCode::NotFound
                );
        }

        auto index = indexesByClassId.constFind(classId);
        if (index == indexesByClassId.cend())
        {
            ClassInfo info;
            const auto parsedInfoClassId = persistedClassId(
                infoClassId, QObject::tr(
                    "Loading class information failed: the database returned an invalid class identifier."));
            if (!parsedInfoClassId)
            {
                return repositoryFailure(parsedInfoClassId.error());
            }
            info.classId = *parsedInfoClassId;
            const QVariant teacherId =
                query.value(QStringLiteral("teacher_id"));
            const auto parsedTeacherId = persistedTeacherId<int>(
                teacherId, QObject::tr(
                    "Loading scoped Sub Prep class information failed: invalid assigned teacher identifier."));
            if (!parsedTeacherId)
            {
                return repositoryFailure(parsedTeacherId.error());
            }
            info.teacherId = *parsedTeacherId;
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

    if (query.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading scoped Sub Prep class information"),
            {},
            identity
            ).userMessage());
    }

    return infos;
}

RepositoryResult<QList<SubPrepPrintSourceClassInfoReadRecord>>
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
        return repositoryFailure(
            QObject::tr(
                "Loading scoped Sub Prep print source information failed: invalid schedule type."
                ),
            RepositoryErrorCode::InvalidInput
            );
    }

    if (maxMeetingsPerClass < 0
        || maxMeetingsPerClass == std::numeric_limits<int>::max()
        || maxTotalMeetings < 0
        || maxTotalMeetings == std::numeric_limits<int>::max())
    {
        return repositoryFailure(
            QObject::tr(
                "Loading scoped Sub Prep print source information failed: invalid meeting limit."
                ),
            RepositoryErrorCode::InvalidInput
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
            return repositoryFailure(
                QObject::tr(
                    "Loading scoped Sub Prep print source information failed: class identifiers must be positive and unique."
                    ),
                RepositoryErrorCode::InvalidInput
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
            return repositoryFailure(
                QObject::tr(
                    "Loading scoped Sub Prep print source information failed: selected weekdays must be valid and unique."
                    ),
                RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(executed.error().userMessage());
    }

    while (query.next())
    {
        bool scheduleClassIdValid = false;
        const qint64 scheduleClassId = query.value(
            QStringLiteral("schedule_class_id")
            ).toString().toLongLong(&scheduleClassIdValid);
        const QVariant infoClassIdValue = query.value(
            QStringLiteral("info_class_id")
            );
        bool infoClassIdValid = false;
        const qint64 infoClassId = infoClassIdValue.toString().toLongLong(
            &infoClassIdValid
            );
        if (!scheduleClassIdValid
            || !infoClassIdValid
            || scheduleClassId <= 0
            || scheduleClassId > std::numeric_limits<int>::max()
            || infoClassId != scheduleClassId)
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading scoped Sub Prep print source information failed: the database returned an invalid class identifier."
                    ),
                RepositoryErrorCode::Validation
                );
        }

        const int classId = static_cast<int>(scheduleClassId);
        bool teacherIdValid = false;
        const qint64 teacherId = query.value(
            QStringLiteral("teacher_id")
            ).toString().toLongLong(&teacherIdValid);
        if (!teacherIdValid || teacherId <= 0)
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading scoped Sub Prep print source information failed for class id %1: the database returned an invalid teacher identifier."
                    ).arg(classId),
                RepositoryErrorCode::Validation
                );
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
            return repositoryFailure(
                QObject::tr(
                    "Loading scoped Sub Prep print source information failed for class id %1: the database returned inconsistent teacher identifiers."
                    ).arg(classId),
                RepositoryErrorCode::Validation
                );
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

    if (query.lastError().isValid())
    {
        return repositoryFailure(
            SqlQueryUtils::errorFor(
                query,
                QObject::tr("Loading scoped Sub Prep print source information"),
                identity
                ).userMessage()
            );
    }

    return records;
}

RepositoryResult<QList<SubPrepRosterOutputScheduleReadRecord>>
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
        return repositoryFailure(
            QObject::tr("Loading scoped Sub Prep roster-output schedule failed: invalid schedule type."),
            RepositoryErrorCode::InvalidInput
            );
    }

    if (maxMeetingsPerClass < 0
        || maxMeetingsPerClass == std::numeric_limits<int>::max()
        || maxTotalMeetings < 0
        || maxTotalMeetings == std::numeric_limits<int>::max())
    {
        return repositoryFailure(
            QObject::tr("Loading scoped Sub Prep roster-output schedule failed: invalid meeting limit."),
            RepositoryErrorCode::InvalidInput
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
            return repositoryFailure(
                QObject::tr(
                    "Loading scoped Sub Prep roster-output schedule failed: class identifiers must be positive and unique."
                    ),
                RepositoryErrorCode::InvalidInput
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
            return repositoryFailure(
                QObject::tr(
                    "Loading scoped Sub Prep roster-output schedule failed: selected weekdays must be valid and unique."
                    ),
                RepositoryErrorCode::InvalidInput
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
        return repositoryFailure(executed.error().userMessage());
    }

    while (query.next())
    {
        const auto parsedClassId = persistedClassId(
            query.value(QStringLiteral("schedule_class_id")), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        const int classId = *parsedClassId;
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
                    teacherId.toString().toLongLong(&teacherIdOk);
                if (!teacherIdOk)
                {
                    return repositoryFailure(
                        QObject::tr(
                            "Loading scoped Sub Prep roster-output schedule failed: the database returned an invalid teacher identifier."
                            ),
                        RepositoryErrorCode::Validation
                        );
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

    if (query.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading scoped Sub Prep roster-output schedule"),
            identity
            ).userMessage());
    }

    return records;
}

RepositoryResult<QList<ClassTeacherAssignment>>
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
        return repositoryFailure(executed.error().userMessage());
    }

    while (query.next())
    {
        const QVariant teacherId = query.value(QStringLiteral("teacher_id"));
        const auto parsedTeacherId = persistedTeacherId<int>(
            teacherId, QObject::tr(
                "Loading class teacher assignments failed: invalid assigned teacher identifier."));
        if (!parsedTeacherId)
        {
            return repositoryFailure(parsedTeacherId.error());
        }
        const auto parsedClassId = persistedClassId(
            query.value(QStringLiteral("class_id")), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        assignments.append({
            *parsedClassId,
            *parsedTeacherId
        });
    }

    if (query.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading class teacher assignments")
            ).userMessage());
    }

    return assignments;
}

RepositoryResult<QList<ClassTeacherAssignmentReadRecord>>
ClassInfoRepository::loadClassTeacherAssignmentRecords()
{
    QList<ClassTeacherAssignmentReadRecord> assignments;
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
        return repositoryFailure(executed.error().userMessage());
    }

    while (query.next())
    {
        const QVariant teacherId = query.value(QStringLiteral("teacher_id"));
        const auto parsedTeacherId = persistedTeacherId<qint64>(
            teacherId, QObject::tr(
                "Loading class teacher assignments failed: invalid assigned teacher identifier."));
        if (!parsedTeacherId)
        {
            return repositoryFailure(parsedTeacherId.error());
        }
        const auto parsedClassId = persistedClassId(
            query.value(QStringLiteral("class_id")), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        assignments.append({
            *parsedClassId,
            *parsedTeacherId
        });
    }

    if (query.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading class teacher assignments")
            ).userMessage());
    }

    return assignments;
}

RepositoryResult<QList<ClassInfo>> ClassInfoRepository::loadScheduleClassInfos()
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
        return repositoryFailure(loadedClasses.error().userMessage());
    }

    while (query.next())
    {
        ClassInfo info;
        const QVariant storedClassIdValue = query.value(QStringLiteral("class_id"));
        bool storedClassIdOk = false;
        const qint64 storedClassId =
            storedClassIdValue.toString().toLongLong(&storedClassIdOk);
        if (!storedClassIdOk
            || storedClassId <= 0
            || storedClassId > std::numeric_limits<int>::max())
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading schedule class information failed: invalid class id %1."
                    ).arg(storedClassIdValue.toString()),
                RepositoryErrorCode::Validation
                );
        }
        info.classId = static_cast<int>(storedClassId);
        const QVariant teacherId = query.value(QStringLiteral("teacher_id"));
        const auto parsedTeacherId = persistedTeacherId<int>(
            teacherId, QObject::tr(
                "Loading schedule class information failed: invalid assigned teacher identifier."));
        if (!parsedTeacherId)
        {
            return repositoryFailure(parsedTeacherId.error());
        }
        info.teacherId = *parsedTeacherId;
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
    if (query.lastError().isValid())
    {
        return repositoryFailure(
            QObject::tr("Loading schedule class information failed: %1")
                .arg(query.lastError().text())
            );
    }

    auto loadTimes = [&]<typename Times>(
        const QString& tableName,
        Times ClassInfo::* times,
        const bool intensive
        )
        -> RepositoryStatus
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
            return repositoryFailure(executed.error().userMessage());
        }

        while (timesQuery.next())
        {
            const auto parsedClassId = persistedClassId(
                timesQuery.value(QStringLiteral("class_id")), QObject::tr(
                    "Loading class information failed: the database returned an invalid class identifier."));
            if (!parsedClassId)
            {
                return repositoryFailure(parsedClassId.error());
            }
            const auto index = indexesByClassId.constFind(*parsedClassId);
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
        if (timesQuery.lastError().isValid())
        {
            return repositoryFailure(
                QObject::tr("Loading schedule class times from %1 failed: %2")
                    .arg(tableName, timesQuery.lastError().text())
                );
        }

        return {};
    };

    if (const RepositoryStatus status = loadTimes(
            QStringLiteral("class_times"),
            &ClassInfo::classTimes,
            false
            ); !status)
    {
        return repositoryFailure(status.error());
    }
    if (const RepositoryStatus status = loadTimes(
            QStringLiteral("class_intensive_times"),
            &ClassInfo::intensiveTimes,
            true
            ); !status)
    {
        return repositoryFailure(status.error());
    }

    qint64 regularScheduleRowCount = 0;
    qint64 intensiveScheduleRowCount = 0;
    for (const ClassInfo& info : infos)
    {
        regularScheduleRowCount += info.classTimes.size();
        intensiveScheduleRowCount += info.intensiveTimes.size();
    }
    m_scheduleClassInfoReadMetrics.completedClassCount += infos.size();
    m_scheduleClassInfoReadMetrics.regularScheduleRowCount +=
        regularScheduleRowCount;
    m_scheduleClassInfoReadMetrics.intensiveScheduleRowCount +=
        intensiveScheduleRowCount;

    return infos;
}

RepositoryStatus ClassInfoRepository::visitScheduleClassInfoRows(
    const ScheduleClassInfoVisitor& classInfoVisitor,
    const ScheduleClassTimeVisitor& scheduleTimeVisitor
    )
{
    ++m_scheduleClassInfoReadMetrics.scheduleClassInfosCallCount;
    if (!classInfoVisitor || !scheduleTimeVisitor)
    {
        return repositoryFailure(
            QObject::tr("Loading schedule class information failed: both visitors are required."),
            RepositoryErrorCode::InvalidInput
            );
    }
    qint64 completedClassCount = 0;
    qint64 regularScheduleRowCount = 0;
    qint64 intensiveScheduleRowCount = 0;

    QSqlQuery query(m_database);
    query.setForwardOnly(true);

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
        return repositoryFailure(loadedClasses.error().userMessage());
    }

    while (query.next())
    {
        ClassInfo info;
        const QVariant storedClassIdValue = query.value(QStringLiteral("class_id"));
        bool storedClassIdOk = false;
        const qint64 storedClassId =
            storedClassIdValue.toString().toLongLong(&storedClassIdOk);
        if (!storedClassIdOk
            || storedClassId <= 0
            || storedClassId > std::numeric_limits<int>::max())
        {
            return repositoryFailure(
                QObject::tr(
                    "Loading schedule class information failed: invalid class id %1."
                    ).arg(storedClassIdValue.toString()),
                RepositoryErrorCode::Validation
                );
        }
        info.classId = static_cast<int>(storedClassId);
        const QVariant teacherId = query.value(QStringLiteral("teacher_id"));
        const auto parsedTeacherId = persistedTeacherId<int>(
            teacherId, QObject::tr(
                "Loading schedule class information failed: invalid assigned teacher identifier."));
        if (!parsedTeacherId)
        {
            return repositoryFailure(parsedTeacherId.error());
        }
        info.teacherId = *parsedTeacherId;
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

        if (const RepositoryStatus visited = classInfoVisitor(info); !visited)
        {
            return repositoryFailure(visited.error());
        }
        ++completedClassCount;
    }
    if (query.lastError().isValid())
    {
        return repositoryFailure(
            QObject::tr("Loading schedule class information failed: %1")
                .arg(query.lastError().text())
            );
    }

    auto visitTimes = [&](
        const QString& tableName,
        const bool intensive
        ) -> RepositoryStatus
    {
        QSqlQuery timesQuery(m_database);
        timesQuery.setForwardOnly(true);
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
            return repositoryFailure(executed.error().userMessage());
        }

        while (timesQuery.next())
        {
            const auto parsedClassId = persistedClassId(
                timesQuery.value(QStringLiteral("class_id")), QObject::tr(
                    "Loading class information failed: the database returned an invalid class identifier."));
            if (!parsedClassId)
            {
                return repositoryFailure(parsedClassId.error());
            }
            const int classId = *parsedClassId;
            const ClassTime time{
                timesQuery.value(QStringLiteral("day")).toString(),
                timesQuery.value(QStringLiteral("start_time")).toString(),
                timesQuery.value(QStringLiteral("end_time")).toString()
            };
            if (const RepositoryStatus visited =
                    scheduleTimeVisitor(classId, time, intensive);
                !visited)
            {
                return repositoryFailure(visited.error());
            }
            if (intensive)
            {
                ++intensiveScheduleRowCount;
            }
            else
            {
                ++regularScheduleRowCount;
            }
        }
        if (timesQuery.lastError().isValid())
        {
            return repositoryFailure(
                QObject::tr("Loading schedule class times from %1 failed: %2")
                    .arg(tableName, timesQuery.lastError().text())
                );
        }

        return {};
    };

    if (const RepositoryStatus loadedRegular = visitTimes(
            QStringLiteral("class_times"),
            false
            ); !loadedRegular)
    {
        return repositoryFailure(loadedRegular.error());
    }
    if (const RepositoryStatus loadedIntensive = visitTimes(
            QStringLiteral("class_intensive_times"),
            true
            ); !loadedIntensive)
    {
        return repositoryFailure(loadedIntensive.error());
    }

    m_scheduleClassInfoReadMetrics.completedClassCount += completedClassCount;
    m_scheduleClassInfoReadMetrics.regularScheduleRowCount +=
        regularScheduleRowCount;
    m_scheduleClassInfoReadMetrics.intensiveScheduleRowCount +=
        intensiveScheduleRowCount;
    return {};
}

RepositoryResult<QList<ScheduleImportStateSnapshotClassInfoReadRecord>>
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
        return repositoryFailure(loadedClasses.error().userMessage());
    }

    while (query.next())
    {
        ScheduleImportStateSnapshotClassInfoReadRecord info;
        const auto parsedClassId = persistedClassId(
            query.value(QStringLiteral("class_id")), QObject::tr(
                "Loading class information failed: the database returned an invalid class identifier."));
        if (!parsedClassId)
        {
            return repositoryFailure(parsedClassId.error());
        }
        info.classId = *parsedClassId;
        const QVariant teacherId = query.value(QStringLiteral("teacher_id"));
        if (teacherId.isNull())
        {
            info.teacherId = -1;
        }
        else
        {
            bool teacherIdOk = false;
            info.teacherId = teacherId.toString().toLongLong(&teacherIdOk);
            if (!teacherIdOk)
            {
                return repositoryFailure(
                    QObject::tr("A class teacher ID could not be read."),
                    RepositoryErrorCode::Validation
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
    if (query.lastError().isValid())
    {
        return repositoryFailure(
            QObject::tr("Loading schedule class information failed: %1")
                .arg(query.lastError().text())
            );
    }
    m_scheduleImportStateSnapshotClassInfoReadMetrics.metadataReturnedRowCount
        += infos.size();

    const auto loadTimes = [&]<typename Times>(
        const QString& tableName,
        Times ScheduleImportStateSnapshotClassInfoReadRecord::* times,
        const bool intensive
        ) -> RepositoryStatus
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
            return repositoryFailure(executed.error().userMessage());
        }

        int returnedRowCount = 0;
        while (timesQuery.next())
        {
            ++returnedRowCount;
            const auto parsedClassId = persistedClassId(
                timesQuery.value(QStringLiteral("class_id")), QObject::tr(
                    "Loading class information failed: the database returned an invalid class identifier."));
            if (!parsedClassId)
            {
                return repositoryFailure(parsedClassId.error());
            }
            const auto index = indexesByClassId.constFind(*parsedClassId);
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
        if (timesQuery.lastError().isValid())
        {
            return repositoryFailure(
                QObject::tr("Loading schedule class times from %1 failed: %2")
                    .arg(tableName, timesQuery.lastError().text())
                );
        }
        if (intensive)
        {
            m_scheduleImportStateSnapshotClassInfoReadMetrics
                .intensiveScheduleReturnedRowCount += returnedRowCount;
        }
        else
        {
            m_scheduleImportStateSnapshotClassInfoReadMetrics
                .regularScheduleReturnedRowCount += returnedRowCount;
        }

        return {};
    };

    if (const RepositoryStatus status = loadTimes(
            QStringLiteral("class_times"),
            &ScheduleImportStateSnapshotClassInfoReadRecord::regularTimes,
            false
            ); !status)
    {
        return repositoryFailure(status.error());
    }
    if (const RepositoryStatus status = loadTimes(
            QStringLiteral("class_intensive_times"),
            &ScheduleImportStateSnapshotClassInfoReadRecord::intensiveTimes,
            true
            ); !status)
    {
        return repositoryFailure(status.error());
    }

    return infos;
}

RepositoryResult<QList<ClassConflict>> ClassInfoRepository::getClassTimeConflicts(
    int classId,
    const QList<ClassTime>& times,
    ScheduleType type
    )
{
    if (classId <= 0)
    {
        return repositoryFailure(
            QObject::tr("Loading class time conflicts failed: invalid class id %1.")
                .arg(classId),
            RepositoryErrorCode::InvalidInput
            );
    }

    QList<ClassConflict> conflicts;

    const RepositoryResult<Classroom> currentClass =
        loadClassById(
            m_database,
            classId
            );
    if (!currentClass)
    {
        return repositoryFailure(currentClass.error());
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
        return repositoryFailure(executed.error().userMessage());
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

        const auto parsedConflictingClassId = persistedClassId<qint64>(
            query.value("class_id"), QObject::tr(
                "Loading class time conflicts failed: the database returned an invalid class identifier."));
        if (!parsedConflictingClassId)
        {
            return repositoryFailure(parsedConflictingClassId.error());
        }
        const qint64 conflictingClassId = *parsedConflictingClassId;

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

    if (query.lastError().isValid())
    {
        return repositoryFailure(SqlQueryUtils::errorFor(
            query,
            QObject::tr("Loading class time conflicts"),
            {},
            QObject::tr("class id %1").arg(classId)
            ).userMessage());
    }

    return conflicts;
}
