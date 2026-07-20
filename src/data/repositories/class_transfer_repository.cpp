#include "class_transfer_repository.h"

#include "data/database/database_transaction.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/classroom.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace
{
struct TimeInterval
{
    int start = -1;
    int end = -1;
};

struct ScheduledTime
{
    QString classLabel;
    ClassTime time;
};

struct ValidatedPlan
{
    QHash<int, ClassImportResolution> classes;
    QHash<QString, TeacherImportResolution> teachers;
};

constexpr int MinutesPerDay = 24 * 60;
constexpr int MinutesPerWeek = 7 * MinutesPerDay;

QString normalized(
    const QString& value
    )
{
    return value.simplified().toCaseFolded();
}

bool teacherMatches(
    const Teacher& source,
    const Teacher& destination
    )
{
    const QString sourceEnglish = normalized(source.teacherEn);
    const QString sourceKorean = normalized(source.teacherKr);
    const QString destinationEnglish = normalized(destination.teacherEn);
    const QString destinationKorean = normalized(destination.teacherKr);

    if (!sourceEnglish.isEmpty() && !sourceKorean.isEmpty())
    {
        return sourceEnglish == destinationEnglish
            && sourceKorean == destinationKorean;
    }

    if (!sourceEnglish.isEmpty())
    {
        return sourceEnglish == destinationEnglish;
    }

    if (!sourceKorean.isEmpty())
    {
        return sourceKorean == destinationKorean;
    }

    return false;
}

const ClassTransferTeacher* packageTeacher(
    const ClassTransferPackage& package,
    const QString& key
    )
{
    for (const ClassTransferTeacher& teacher : package.teachers)
    {
        if (teacher.key == key)
        {
            return &teacher;
        }
    }

    return nullptr;
}

QList<ClassTransferEvaluation> loadEvaluations(
    QSqlDatabase& database,
    int classId,
    QString* errorMessage
    )
{
    QList<ClassTransferEvaluation> evaluations;
    QSqlQuery evaluationQuery(database);

    evaluationQuery.prepare(R"(
        SELECT id, evaluation_name
        FROM speaking_evaluations
        WHERE class_id=?
        ORDER BY id
    )");
    evaluationQuery.addBindValue(classId);

    if (!evaluationQuery.exec())
    {
        *errorMessage = QObject::tr("Unable to read speaking evaluations: %1")
            .arg(evaluationQuery.lastError().text());
        return {};
    }

    while (evaluationQuery.next())
    {
        ClassTransferEvaluation evaluation;
        evaluation.name = evaluationQuery.value("evaluation_name").toString();
        evaluation.rows = SpeakingEval::emptyRows();

        QSqlQuery rowQuery(database);
        rowQuery.prepare(R"(
            SELECT *
            FROM speaking_eval_data
            WHERE evaluation_id=?
            ORDER BY row_index
        )");
        rowQuery.addBindValue(evaluationQuery.value("id"));

        if (!rowQuery.exec())
        {
            *errorMessage = QObject::tr("Unable to read speaking evaluation rows: %1")
                .arg(rowQuery.lastError().text());
            return {};
        }

        while (rowQuery.next())
        {
            const int rowIndex = rowQuery.value("row_index").toInt();

            if (rowIndex < 0 || rowIndex >= SpeakingEval::RowCount)
            {
                continue;
            }

            for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
            {
                evaluation.rows[rowIndex][column] =
                    rowQuery.value(
                        QStringLiteral("col_%1").arg(column)
                        ).toString();
            }
        }

        evaluations.append(evaluation);
    }

    return evaluations;
}

QString transferClassLabel(
    const ClassTransferClass& transferClass
    )
{
    const QString course = QStringList{
        transferClass.info.classGrade.trimmed(),
        transferClass.info.classLevel.trimmed()
        }.join(QStringLiteral(" ")).trimmed();

    if (!course.isEmpty())
    {
        return course;
    }

    if (!transferClass.name.trimmed().isEmpty())
    {
        return transferClass.name.trimmed();
    }

    return transferClass.key;
}

QString destinationClassLabel(
    const Classroom& classroom,
    const ClassInfo& info
    )
{
    const QString course = QStringList{
        info.classGrade.trimmed(),
        info.classLevel.trimmed()
        }.join(QStringLiteral(" ")).trimmed();

    if (!course.isEmpty())
    {
        return course;
    }

    if (!classroom.name.trimmed().isEmpty())
    {
        return classroom.name.trimmed();
    }

    return QObject::tr("Class %1").arg(classroom.id);
}

int dayIndex(
    const QString& day
    )
{
    static const QStringList Days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };

    return Days.indexOf(day);
}

int timeToMinutes(
    const QString& value
    )
{
    const QStringList parts = value.trimmed().split(
        QLatin1Char(' '), Qt::SkipEmptyParts);

    if (parts.size() != 2)
    {
        return -1;
    }

    const QStringList timeParts = parts[0].split(QLatin1Char(':'));

    if (timeParts.size() != 2)
    {
        return -1;
    }

    bool hourOk = false;
    bool minuteOk = false;
    int hour = timeParts[0].toInt(&hourOk);
    const int minute = timeParts[1].toInt(&minuteOk);
    const QString period = parts[1].toUpper();

    if (!hourOk || !minuteOk || hour < 1 || hour > 12
        || minute < 0 || minute > 59
        || (period != QStringLiteral("AM")
            && period != QStringLiteral("PM")))
    {
        return -1;
    }

    if (period == QStringLiteral("AM"))
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

bool intervalForTime(
    const ClassTime& time,
    TimeInterval* interval
    )
{
    const int day = dayIndex(time.day);
    const int start = timeToMinutes(time.startTime);
    const int end = timeToMinutes(time.endTime);

    if (day < 0 || start < 0 || end < 0)
    {
        return false;
    }

    interval->start = day * MinutesPerDay + start;
    interval->end = day * MinutesPerDay + end;

    if (interval->end <= interval->start)
    {
        interval->end += MinutesPerDay;
    }

    return true;
}

bool intervalsOverlap(
    const TimeInterval& first,
    const TimeInterval& second
    )
{
    for (int offset : {-MinutesPerWeek, 0, MinutesPerWeek})
    {
        if (first.start < second.end + offset
            && second.start + offset < first.end)
        {
            return true;
        }
    }

    return false;
}

QString timeDescription(
    const ScheduledTime& scheduled
    )
{
    return QObject::tr("%1 — %2 %3–%4")
        .arg(
            scheduled.classLabel,
            scheduled.time.day,
            scheduled.time.startTime,
            scheduled.time.endTime
            );
}

Status appendAndValidateTimes(
    QList<ScheduledTime>* destination,
    const QString& classLabel,
    const QList<ClassTime>& times,
    const QString& scheduleLabel
    )
{
    for (const ClassTime& time : times)
    {
        TimeInterval interval;

        if (!intervalForTime(time, &interval))
        {
            return std::unexpected(
                QObject::tr("%1 contains an invalid %2 schedule entry: %3 %4–%5")
                    .arg(
                        classLabel,
                        scheduleLabel,
                        time.day,
                        time.startTime,
                        time.endTime
                        )
                );
        }

        destination->append({classLabel, time});
    }

    return {};
}

QStringList findScheduleConflicts(
    const QList<ScheduledTime>& imported,
    const QList<ScheduledTime>& existing,
    const QString& scheduleLabel
    )
{
    QStringList conflicts;

    const auto addConflict = [&conflicts, &scheduleLabel](
        const ScheduledTime& first,
        const ScheduledTime& second)
    {
        const QString message = QObject::tr("%1: %2 conflicts with %3")
            .arg(
                scheduleLabel,
                timeDescription(first),
                timeDescription(second)
                );

        if (!conflicts.contains(message))
        {
            conflicts.append(message);
        }
    };

    for (int first = 0; first < imported.size(); ++first)
    {
        TimeInterval firstInterval;
        intervalForTime(imported[first].time, &firstInterval);

        for (int second = first + 1; second < imported.size(); ++second)
        {
            TimeInterval secondInterval;
            intervalForTime(imported[second].time, &secondInterval);

            if (intervalsOverlap(firstInterval, secondInterval))
            {
                addConflict(imported[first], imported[second]);
            }
        }

        for (const ScheduledTime& destination : existing)
        {
            TimeInterval destinationInterval;
            intervalForTime(destination.time, &destinationInterval);

            if (intervalsOverlap(firstInterval, destinationInterval))
            {
                addConflict(imported[first], destination);
            }
        }
    }

    return conflicts;
}

Result<ClassImportPreview> buildPreview(
    QSqlDatabase& database,
    const ClassTransferPackage& package
    )
{
    TeacherRepository teacherRepository(database);
    ClassRepository classRepository(database);
    ClassInfoRepository classInfoRepository(database);

    const QList<Teacher> destinationTeachers =
        teacherRepository.getAllTeachers();
    const QList<Classroom> destinationClasses =
        classRepository.getClasses();

    ClassImportPreview preview;

    for (const ClassTransferTeacher& packageEntry : package.teachers)
    {
        ClassImportTeacherPreview teacherPreview;
        teacherPreview.teacherKey = packageEntry.key;

        for (const Teacher& destination : destinationTeachers)
        {
            if (teacherMatches(packageEntry.teacher, destination))
            {
                teacherPreview.matchingTeacherIds.append(destination.id);
            }
        }

        preview.teachers.append(teacherPreview);
    }

    for (int index = 0; index < package.classes.size(); ++index)
    {
        const ClassTransferClass& source = package.classes[index];
        ClassImportClassPreview classPreview;
        classPreview.packageClassIndex = index;

        const QString sourceGrade = normalized(source.info.classGrade);
        const QString sourceLevel = normalized(source.info.classLevel);

        if (sourceGrade.isEmpty() || sourceLevel.isEmpty())
        {
            preview.classes.append(classPreview);
            continue;
        }

        const ClassTransferTeacher* sourceTeacher =
            packageTeacher(package, source.teacherKey);

        for (const Classroom& destination : destinationClasses)
        {
            const ClassInfo destinationInfo =
                classInfoRepository.loadClassInfo(destination.id);

            if (normalized(destinationInfo.classGrade) != sourceGrade
                || normalized(destinationInfo.classLevel) != sourceLevel)
            {
                continue;
            }

            bool sameTeacher = false;

            if (!sourceTeacher)
            {
                sameTeacher = destinationInfo.teacherId <= 0;
            }
            else if (destinationInfo.teacherId > 0)
            {
                sameTeacher = teacherMatches(
                    sourceTeacher->teacher,
                    teacherRepository.getTeacher(destinationInfo.teacherId)
                    );
            }

            if (sameTeacher)
            {
                classPreview.matchingClassIds.append(destination.id);
            }
        }

        preview.classes.append(classPreview);
    }

    return preview;
}

Result<ValidatedPlan> validatePlan(
    QSqlDatabase& database,
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    )
{
    const auto previewResult = buildPreview(database, package);

    if (!previewResult)
    {
        return std::unexpected(previewResult.error());
    }

    QHash<int, QList<int>> classMatches;
    QHash<QString, QList<int>> teacherMatchesByKey;

    for (const ClassImportClassPreview& entry : previewResult->classes)
    {
        classMatches.insert(entry.packageClassIndex, entry.matchingClassIds);
    }

    for (const ClassImportTeacherPreview& entry : previewResult->teachers)
    {
        teacherMatchesByKey.insert(entry.teacherKey, entry.matchingTeacherIds);
    }

    ValidatedPlan validated;
    QSet<int> replacementTargets;
    QSet<int> teacherReplacementTargets;

    for (const ClassImportResolution& resolution : plan.classes)
    {
        const int index = resolution.packageClassIndex;

        if (index < 0 || index >= package.classes.size()
            || validated.classes.contains(index))
        {
            return std::unexpected(
                QObject::tr("The class import plan contains an invalid or duplicate class entry.")
                );
        }

        const QList<int> matches = classMatches.value(index);

        if (resolution.action == ClassImportAction::Replace)
        {
            if (!matches.contains(resolution.targetClassId))
            {
                return std::unexpected(
                    QObject::tr("A replacement class is not one of the inferred matches.")
                    );
            }

            if (replacementTargets.contains(resolution.targetClassId))
            {
                return std::unexpected(
                    QObject::tr("Two package classes cannot replace the same destination class.")
                    );
            }

            replacementTargets.insert(resolution.targetClassId);
        }
        else if (resolution.targetClassId > 0)
        {
            return std::unexpected(
                QObject::tr("Only replacement actions may specify a destination class.")
                );
        }

        validated.classes.insert(index, resolution);
    }

    if (validated.classes.size() != package.classes.size())
    {
        return std::unexpected(
            QObject::tr("Every package class must have an import action.")
            );
    }

    for (const TeacherImportResolution& resolution : plan.teachers)
    {
        if (resolution.teacherKey.trimmed().isEmpty()
            || !teacherMatchesByKey.contains(resolution.teacherKey)
            || validated.teachers.contains(resolution.teacherKey))
        {
            return std::unexpected(
                QObject::tr("The teacher import plan contains an invalid or duplicate teacher entry.")
                );
        }

        const QList<int> matches =
            teacherMatchesByKey.value(resolution.teacherKey);

        if (resolution.action == TeacherImportAction::Create)
        {
            if (matches.size() == 1 || resolution.targetTeacherId > 0)
            {
                return std::unexpected(
                    QObject::tr("An unambiguous teacher match must reuse the local teacher.")
                    );
            }
        }
        else
        {
            if (!matches.contains(resolution.targetTeacherId))
            {
                return std::unexpected(
                    QObject::tr("A selected teacher is not one of the inferred matches.")
                    );
            }

            if (resolution.action == TeacherImportAction::ReplaceExisting)
            {
                if (teacherReplacementTargets.contains(
                        resolution.targetTeacherId))
                {
                    return std::unexpected(
                        QObject::tr("Two different package teachers cannot replace the same local teacher.")
                        );
                }

                teacherReplacementTargets.insert(
                    resolution.targetTeacherId);
            }
        }

        validated.teachers.insert(resolution.teacherKey, resolution);
    }

    if (validated.teachers.size() != package.teachers.size())
    {
        return std::unexpected(
            QObject::tr("Every package teacher must have an import action.")
            );
    }

    return validated;
}

Status preflightSchedules(
    QSqlDatabase& database,
    const ClassTransferPackage& package,
    const ValidatedPlan& plan
    )
{
    QList<ScheduledTime> importedRegular;
    QList<ScheduledTime> importedIntensive;
    QSet<int> replacedClassIds;

    for (auto iterator = plan.classes.cbegin();
         iterator != plan.classes.cend(); ++iterator)
    {
        if (iterator->action == ClassImportAction::Replace)
        {
            replacedClassIds.insert(iterator->targetClassId);
        }
    }

    for (int index = 0; index < package.classes.size(); ++index)
    {
        const ClassImportResolution resolution = plan.classes.value(index);

        if (resolution.action == ClassImportAction::Skip)
        {
            continue;
        }

        const ClassTransferClass& transferClass = package.classes[index];
        const QString label = transferClassLabel(transferClass);
        Status status = appendAndValidateTimes(
            &importedRegular,
            label,
            transferClass.info.classTimes,
            QObject::tr("regular")
            );

        if (!status)
        {
            return status;
        }

        status = appendAndValidateTimes(
            &importedIntensive,
            label,
            transferClass.info.intensiveTimes,
            QObject::tr("intensive")
            );

        if (!status)
        {
            return status;
        }
    }

    ClassRepository classRepository(database);
    ClassInfoRepository classInfoRepository(database);
    QList<ScheduledTime> existingRegular;
    QList<ScheduledTime> existingIntensive;

    for (const Classroom& classroom : classRepository.getClasses())
    {
        if (replacedClassIds.contains(classroom.id))
        {
            continue;
        }

        const ClassInfo info = classInfoRepository.loadClassInfo(classroom.id);
        const QString label = destinationClassLabel(classroom, info);
        Status status = appendAndValidateTimes(
            &existingRegular,
            label,
            info.classTimes,
            QObject::tr("regular")
            );

        if (!status)
        {
            return status;
        }

        status = appendAndValidateTimes(
            &existingIntensive,
            label,
            info.intensiveTimes,
            QObject::tr("intensive")
            );

        if (!status)
        {
            return status;
        }
    }

    QStringList conflicts = findScheduleConflicts(
        importedRegular, existingRegular, QObject::tr("Regular schedule"));
    conflicts.append(findScheduleConflicts(
        importedIntensive,
        existingIntensive,
        QObject::tr("Intensive schedule")
        ));

    if (!conflicts.isEmpty())
    {
        return std::unexpected(
            QObject::tr("Schedule conflicts prevent this import:\n\n%1")
                .arg(conflicts.join(QLatin1Char('\n')))
            );
    }

    return {};
}

Status queryFailure(
    const QSqlQuery& query,
    const QString& operation
    )
{
    return std::unexpected(
        QObject::tr("%1 failed: %2")
            .arg(operation, query.lastError().text())
        );
}

Result<int> insertTeacher(
    QSqlDatabase& database,
    const Teacher& teacher
    )
{
    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO teachers (
            teacher_kr, teacher_en, preferred_romanization,
            room_number, birthday, phone_number,
            wifi_name, wifi_password, internet_type,
            zoom_id, zoom_password, projection_type, notes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.preferredRomanization);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.birthday);
    query.addBindValue(teacher.phoneNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(teacher.internetType);
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
    query.addBindValue(teacher.projectionType);
    query.addBindValue(teacher.notes);

    if (!query.exec())
    {
        return std::unexpected(queryFailure(
            query, QObject::tr("Creating an imported teacher")).error());
    }

    const int id = query.lastInsertId().toInt();

    if (id <= 0)
    {
        return std::unexpected(
            QObject::tr("The imported teacher did not receive a database ID.")
            );
    }

    return id;
}

Status updateTeacher(
    QSqlDatabase& database,
    int teacherId,
    const Teacher& teacher
    )
{
    QSqlQuery query(database);
    query.prepare(R"(
        UPDATE teachers SET
            teacher_kr=?, teacher_en=?, preferred_romanization=?,
            room_number=?, birthday=?, phone_number=?,
            wifi_name=?, wifi_password=?, internet_type=?,
            zoom_id=?, zoom_password=?, projection_type=?, notes=?
        WHERE id=?
    )");
    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.preferredRomanization);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.birthday);
    query.addBindValue(teacher.phoneNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(teacher.internetType);
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
    query.addBindValue(teacher.projectionType);
    query.addBindValue(teacher.notes);
    query.addBindValue(teacherId);

    if (!query.exec())
    {
        return queryFailure(query, QObject::tr("Updating a matched teacher"));
    }

    if (query.numRowsAffected() != 1)
    {
        return std::unexpected(
            QObject::tr("The matched teacher no longer exists.")
            );
    }

    return {};
}

Status clearClassData(
    QSqlDatabase& database,
    int classId
    )
{
    QSqlQuery query(database);
    query.prepare(R"(
        DELETE FROM speaking_eval_data
        WHERE evaluation_id IN (
            SELECT id FROM speaking_evaluations WHERE class_id=?
        )
    )");
    query.addBindValue(classId);

    if (!query.exec())
    {
        return queryFailure(query, QObject::tr("Clearing speaking evaluation rows"));
    }

    for (const QString& table : {
             QStringLiteral("speaking_evaluations"),
             QStringLiteral("roster_columns"),
             QStringLiteral("roster_data"),
             QStringLiteral("class_info"),
             QStringLiteral("class_times"),
             QStringLiteral("class_intensive_times")
         })
    {
        query.prepare(
            QStringLiteral("DELETE FROM %1 WHERE class_id=?").arg(table));
        query.addBindValue(classId);

        if (!query.exec())
        {
            return queryFailure(
                query,
                QObject::tr("Clearing imported class data from %1").arg(table)
                );
        }
    }

    return {};
}

Status writeClassData(
    QSqlDatabase& database,
    int classId,
    int teacherId,
    const ClassTransferClass& transferClass
    )
{
    QSqlQuery query(database);
    const ClassInfo& info = transferClass.info;
    query.prepare(R"(
        INSERT INTO class_info (
            class_id, teacher_id, class_grade, class_level,
            reading_book, essay_book, class_color, font_color,
            notes, time_filler_activities
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(classId);
    query.addBindValue(teacherId > 0 ? QVariant(teacherId) : QVariant());
    query.addBindValue(info.classGrade);
    query.addBindValue(info.classLevel);
    query.addBindValue(info.readingBook);
    query.addBindValue(info.essayBook);
    query.addBindValue(info.classColor);
    query.addBindValue(info.fontColor);
    query.addBindValue(info.notes);
    query.addBindValue(info.timeFillerActivities);

    if (!query.exec())
    {
        return queryFailure(query, QObject::tr("Writing imported class information"));
    }

    const auto writeTimes = [&database, classId](
        const QString& table,
        const QList<ClassTime>& times) -> Status
    {
        for (const ClassTime& time : times)
        {
            QSqlQuery timeQuery(database);
            timeQuery.prepare(
                QStringLiteral(
                    "INSERT INTO %1 (class_id, day, start_time, end_time) "
                    "VALUES (?, ?, ?, ?)").arg(table));
            timeQuery.addBindValue(classId);
            timeQuery.addBindValue(time.day);
            timeQuery.addBindValue(time.startTime);
            timeQuery.addBindValue(time.endTime);

            if (!timeQuery.exec())
            {
                return queryFailure(
                    timeQuery,
                    QObject::tr("Writing imported schedule data")
                    );
            }
        }

        return {};
    };

    Status status = writeTimes(
        QStringLiteral("class_times"), info.classTimes);

    if (!status)
    {
        return status;
    }

    status = writeTimes(
        QStringLiteral("class_intensive_times"), info.intensiveTimes);

    if (!status)
    {
        return status;
    }

    for (int column = 0; column < transferClass.roster.columns.size(); ++column)
    {
        query.prepare(R"(
            INSERT INTO roster_columns (class_id, name, position, width)
            VALUES (?, ?, ?, ?)
        )");
        query.addBindValue(classId);
        query.addBindValue(transferClass.roster.columns[column]);
        query.addBindValue(column);
        query.addBindValue(
            column < transferClass.roster.columnWidths.size()
                ? transferClass.roster.columnWidths[column]
                : 0
            );

        if (!query.exec())
        {
            return queryFailure(query, QObject::tr("Writing imported roster columns"));
        }
    }

    for (int row = 0; row < transferClass.roster.rows.size(); ++row)
    {
        for (int column = 0;
             column < transferClass.roster.columns.size(); ++column)
        {
            const QString value =
                column < transferClass.roster.rows[row].size()
                    ? transferClass.roster.rows[row][column]
                    : QString();

            if (value.isEmpty())
            {
                continue;
            }

            query.prepare(R"(
                INSERT INTO roster_data (class_id, row_index, col_index, value)
                VALUES (?, ?, ?, ?)
            )");
            query.addBindValue(classId);
            query.addBindValue(row);
            query.addBindValue(column);
            query.addBindValue(value);

            if (!query.exec())
            {
                return queryFailure(query, QObject::tr("Writing imported roster data"));
            }
        }
    }

    for (const ClassTransferEvaluation& evaluation : transferClass.evaluations)
    {
        query.prepare(R"(
            INSERT INTO speaking_evaluations (class_id, evaluation_name)
            VALUES (?, ?)
        )");
        query.addBindValue(classId);
        query.addBindValue(evaluation.name);

        if (!query.exec())
        {
            return queryFailure(query, QObject::tr("Creating an imported speaking evaluation"));
        }

        const int evaluationId = query.lastInsertId().toInt();

        for (int row = 0; row < SpeakingEval::RowCount; ++row)
        {
            query.prepare(R"(
                INSERT INTO speaking_eval_data (
                    evaluation_id, row_index,
                    col_0, col_1, col_2, col_3, col_4, col_5,
                    col_6, col_7, col_8, col_9, col_10
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )");
            query.addBindValue(evaluationId);
            query.addBindValue(row);

            for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
            {
                query.addBindValue(
                    row < evaluation.rows.size()
                    && column < evaluation.rows[row].size()
                        ? evaluation.rows[row][column]
                        : QString()
                    );
            }

            if (!query.exec())
            {
                return queryFailure(query, QObject::tr("Writing imported speaking evaluation rows"));
            }
        }
    }

    return {};
}
}

ClassTransferRepository::ClassTransferRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<ClassTransferPackage> ClassTransferRepository::buildPackage(
    const QList<int>& classIds
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(QObject::tr("No database is open."));
    }

    if (classIds.isEmpty())
    {
        return std::unexpected(QObject::tr("No classes were selected."));
    }

    DatabaseTransaction transaction(m_database);

    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Unable to start the class export transaction: %1")
                .arg(m_database.lastError().text())
            );
    }

    ClassRepository classRepository(m_database);
    ClassInfoRepository classInfoRepository(m_database);
    RosterRepository rosterRepository(m_database);
    TeacherRepository teacherRepository(m_database);
    ClassTransferPackage package;
    package.exportedAtUtc = QDateTime::currentDateTimeUtc();
    QSet<int> seenClasses;
    QHash<int, QString> teacherKeys;

    for (int index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds[index];

        if (classId <= 0 || seenClasses.contains(classId))
        {
            return std::unexpected(
                QObject::tr("The class selection contains an invalid or duplicate class.")
                );
        }

        const Classroom classroom = classRepository.getClassById(classId);

        if (classroom.id <= 0)
        {
            return std::unexpected(
                QObject::tr("Class %1 no longer exists.").arg(classId)
                );
        }

        seenClasses.insert(classId);
        ClassTransferClass transferClass;
        transferClass.key = QStringLiteral("class-%1").arg(index + 1);
        transferClass.name = classroom.name;
        transferClass.info = classInfoRepository.loadClassInfo(classId);
        transferClass.roster = rosterRepository.loadRoster(classId);

        if (transferClass.info.teacherId > 0)
        {
            const int teacherId = transferClass.info.teacherId;

            if (!teacherKeys.contains(teacherId))
            {
                const Teacher teacher = teacherRepository.getTeacher(teacherId);

                if (teacher.id <= 0)
                {
                    return std::unexpected(
                        QObject::tr("The assigned teacher for %1 no longer exists.")
                            .arg(transferClassLabel(transferClass))
                        );
                }

                const QString key =
                    QStringLiteral("teacher-%1").arg(teacherKeys.size() + 1);
                teacherKeys.insert(teacherId, key);
                package.teachers.append({key, teacher});
            }

            transferClass.teacherKey = teacherKeys.value(teacherId);
        }

        QString evaluationError;
        transferClass.evaluations = loadEvaluations(
            m_database, classId, &evaluationError);

        if (!evaluationError.isEmpty())
        {
            return std::unexpected(evaluationError);
        }

        transferClass.info.classId = -1;
        transferClass.info.teacherId = -1;
        transferClass.info.teacherKr.clear();
        transferClass.info.teacherEn.clear();
        transferClass.info.roomNumber.clear();
        transferClass.info.wifiName.clear();
        transferClass.info.wifiPassword.clear();
        transferClass.info.internetType.clear();
        transferClass.info.zoomId.clear();
        transferClass.info.zoomPassword.clear();
        transferClass.info.projectionType.clear();
        package.classes.append(transferClass);
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Unable to finish the class export transaction: %1")
                .arg(m_database.lastError().text())
            );
    }

    return package;
}

Result<ClassImportPreview> ClassTransferRepository::previewImport(
    const ClassTransferPackage& package
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(QObject::tr("No database is open."));
    }

    return buildPreview(m_database, package);
}

Result<ClassImportSummary> ClassTransferRepository::importClasses(
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(QObject::tr("No database is open."));
    }

    const auto validatedResult = validatePlan(m_database, package, plan);

    if (!validatedResult)
    {
        return std::unexpected(validatedResult.error());
    }

    const Status schedulesReady = preflightSchedules(
        m_database, package, *validatedResult);

    if (!schedulesReady)
    {
        return std::unexpected(schedulesReady.error());
    }

    QSet<QString> usedTeacherKeys;

    for (int index = 0; index < package.classes.size(); ++index)
    {
        if (validatedResult->classes.value(index).action
                != ClassImportAction::Skip
            && !package.classes[index].teacherKey.isEmpty())
        {
            usedTeacherKeys.insert(package.classes[index].teacherKey);
        }
    }

    DatabaseTransaction transaction(m_database);

    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Unable to start the class import transaction: %1")
                .arg(m_database.lastError().text())
            );
    }

    QHash<QString, int> teacherIds;

    for (const ClassTransferTeacher& transferTeacher : package.teachers)
    {
        if (!usedTeacherKeys.contains(transferTeacher.key))
        {
            continue;
        }

        const TeacherImportResolution resolution =
            validatedResult->teachers.value(transferTeacher.key);

        if (resolution.action == TeacherImportAction::Create)
        {
            const auto teacherId = insertTeacher(
                m_database, transferTeacher.teacher);

            if (!teacherId)
            {
                return std::unexpected(teacherId.error());
            }

            teacherIds.insert(transferTeacher.key, *teacherId);
        }
        else
        {
            if (resolution.action == TeacherImportAction::ReplaceExisting)
            {
                const Status updated = updateTeacher(
                    m_database,
                    resolution.targetTeacherId,
                    transferTeacher.teacher
                    );

                if (!updated)
                {
                    return std::unexpected(updated.error());
                }
            }

            teacherIds.insert(
                transferTeacher.key, resolution.targetTeacherId);
        }
    }

    ClassImportSummary summary;

    for (int index = 0; index < package.classes.size(); ++index)
    {
        const ClassImportResolution resolution =
            validatedResult->classes.value(index);
        const ClassTransferClass& transferClass = package.classes[index];

        if (resolution.action == ClassImportAction::Skip)
        {
            ++summary.skippedClassCount;
            continue;
        }

        int classId = resolution.targetClassId;
        QSqlQuery query(m_database);

        if (resolution.action == ClassImportAction::Create)
        {
            query.prepare(QStringLiteral("INSERT INTO classes (name) VALUES (?)"));
            query.addBindValue(transferClass.name);

            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(query, QObject::tr("Creating an imported class")).error()
                    );
            }

            classId = query.lastInsertId().toInt();

            if (classId <= 0)
            {
                return std::unexpected(
                    QObject::tr("The imported class did not receive a database ID.")
                    );
            }

            summary.createdClassIds.append(classId);
        }
        else
        {
            query.prepare(QStringLiteral("UPDATE classes SET name=? WHERE id=?"));
            query.addBindValue(transferClass.name);
            query.addBindValue(classId);

            if (!query.exec() || query.numRowsAffected() != 1)
            {
                return std::unexpected(
                    queryFailure(query, QObject::tr("Updating a replaced class")).error()
                    );
            }

            const Status cleared = clearClassData(m_database, classId);

            if (!cleared)
            {
                return std::unexpected(cleared.error());
            }

            summary.replacedClassIds.append(classId);
        }

        const int teacherId = transferClass.teacherKey.isEmpty()
            ? -1
            : teacherIds.value(transferClass.teacherKey, -1);
        const Status written = writeClassData(
            m_database, classId, teacherId, transferClass);

        if (!written)
        {
            return std::unexpected(written.error());
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Unable to commit the class import transaction: %1")
                .arg(m_database.lastError().text())
            );
    }

    return summary;
}
