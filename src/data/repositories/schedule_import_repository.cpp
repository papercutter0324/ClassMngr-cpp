#include "schedule_import_repository.h"

#include "data/database/database_transaction.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/rules/schedule_import_rules.h"
#include "features/classes/config/class_info_config.h"
#include "features/teacher/import/teacher_import_name_utils.h"

#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTime>

#include <algorithm>

namespace
{
struct Interval
{
    int start = -1;
    int end = -1;
};

QString teacherKey(
    const QString& value
    )
{
    return TeacherImportNameUtils::hangulOnly(value);
}

QString normalized(
    const QString& value
    )
{
    return value.simplified().toCaseFolded();
}

QString queryFailure(
    const QSqlQuery& query,
    const QString& action
    )
{
    return QObject::tr("%1 failed: %2")
        .arg(action, query.lastError().text());
}

int dayIndex(
    const QString& day
    )
{
    static const QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
    return days.indexOf(day);
}

int timeMinutes(
    const QString& value
    )
{
    const QStringList formats{
        QStringLiteral("h:mm AP"),
        QStringLiteral("h:mmAP"),
        QStringLiteral("H:mm"),
        QStringLiteral("HH:mm")
    };

    for (const QString& format : formats)
    {
        const QTime time =
            QTime::fromString(
                value.trimmed(),
                format
                );

        if (time.isValid())
        {
            return time.hour() * 60 + time.minute();
        }
    }

    return -1;
}

bool intervalFor(
    const ClassTime& time,
    Interval* interval
    )
{
    const int day =
        dayIndex(time.day);
    const int start =
        timeMinutes(time.startTime);
    const int end =
        timeMinutes(time.endTime);

    if (
        !interval
        || day < 0
        || start < 0
        || end <= start
        )
    {
        return false;
    }

    interval->start =
        day * 24 * 60 + start;
    interval->end =
        day * 24 * 60 + end;
    return true;
}

bool overlaps(
    const ClassTime& left,
    const ClassTime& right
    )
{
    Interval leftInterval;
    Interval rightInterval;

    return intervalFor(left, &leftInterval)
        && intervalFor(right, &rightInterval)
        && leftInterval.start < rightInterval.end
        && rightInterval.start < leftInterval.end;
}

bool sameTime(
    const ClassTime& left,
    const ClassTime& right
    )
{
    return left.day == right.day
        && left.startTime == right.startTime
        && left.endTime == right.endTime;
}

bool sharesTime(
    const QList<ClassTime>& left,
    const QList<ClassTime>& right
    )
{
    return std::any_of(
        left.cbegin(),
        left.cend(),
        [&right](const ClassTime& first)
        {
            return std::any_of(
                right.cbegin(),
                right.cend(),
                [&first](const ClassTime& second)
                {
                    return sameTime(first, second)
                        || overlaps(first, second);
                }
                );
        }
        );
}

void appendUnique(
    QList<int>* values,
    int value
    )
{
    if (values && value > 0 && !values->contains(value))
    {
        values->append(value);
    }
}

bool validCourse(
    const QString& grade,
    const QString& level
    )
{
    if (!ClassInfoConfig::Grades.contains(grade))
    {
        return false;
    }

    return ClassInfoConfig::levelsForGrade(grade)
        .contains(level);
}

QString normalizedHexColor(
    const QString& value
    )
{
    static const QRegularExpression expression(
        QStringLiteral("^#[0-9A-Fa-f]{6}$")
        );
    const QString color =
        value.trimmed();
    return expression.match(color).hasMatch()
        ? color.toUpper()
        : QString();
}

QList<ClassTime> selectedTimes(
    const ClassInfo& info,
    ScheduleImportKind kind
    )
{
    return kind == ScheduleImportKind::Intensive
        ? info.intensiveTimes
        : info.classTimes;
}

Status validateProjectedSchedule(
    const ScheduleImportPlan& plan,
    const QHash<int, ClassInfo>& existingInfo,
    const QHash<int, ScheduleImportClassResolution>& classResolutions
    )
{
    struct Scheduled
    {
        QString label;
        ClassTime time;
    };
    struct ProjectedClass
    {
        QString label;
        QList<ClassTime> times;
    };

    QHash<int, ProjectedClass> projectedClasses;
    const bool preservesAbsentIntensiveClasses =
        plan.kind == ScheduleImportKind::Intensive
        && plan.intensiveMode
            == ScheduleImportIntensiveMode::UpdateExisting;

    if (preservesAbsentIntensiveClasses)
    {
        for (
            auto iterator = existingInfo.cbegin();
            iterator != existingInfo.cend();
            ++iterator
            )
        {
            const QList<ClassTime> times =
                selectedTimes(iterator.value(), plan.kind);
            if (times.isEmpty())
            {
                continue;
            }
            projectedClasses.insert(
                iterator.key(),
                {
                    QStringLiteral("%1 %2")
                        .arg(
                            iterator.value().classGrade,
                            iterator.value().classLevel
                            )
                        .simplified(),
                    times
                }
                );
        }
    }

    for (int index = 0; index < plan.candidates.size(); ++index)
    {
        if (!classResolutions.contains(index))
        {
            return std::unexpected(
                QObject::tr(
                    "Every imported class requires a resolution."
                    )
                );
        }

        const ScheduleImportClassResolution resolution =
            classResolutions.value(index);
        const ScheduleImportClassCandidate& candidate =
            plan.candidates[index];

        if (resolution.action == ScheduleImportClassAction::Skip)
        {
            if (
                !preservesAbsentIntensiveClasses
                && resolution.targetClassId > 0
                && existingInfo.contains(resolution.targetClassId)
                )
            {
                const ClassInfo info =
                    existingInfo.value(resolution.targetClassId);
                projectedClasses.insert(
                    resolution.targetClassId,
                    {
                        QStringLiteral("%1 %2")
                            .arg(
                                info.classGrade,
                                info.classLevel
                                )
                            .simplified(),
                        selectedTimes(info, plan.kind)
                    }
                    );
            }
            continue;
        }

        const QString label =
            QStringLiteral("%1 %2")
                .arg(
                    candidate.classGrade,
                    candidate.classLevel
                    );
        const int projectedId =
            resolution.action
                == ScheduleImportClassAction::UpdateExisting
                ? resolution.targetClassId
                : -(index + 1);
        projectedClasses.insert(
            projectedId,
            {label, candidate.times}
            );
    }

    QList<Scheduled> projected;
    for (const ProjectedClass& classroom : projectedClasses)
    {
        for (const ClassTime& time : classroom.times)
        {
            Interval interval;
            if (!intervalFor(time, &interval))
            {
                return std::unexpected(
                    QObject::tr(
                        "%1 contains an invalid time: %2 %3–%4"
                        )
                        .arg(
                            classroom.label,
                            time.day,
                            time.startTime,
                            time.endTime
                            )
                    );
            }

            for (const Scheduled& other : projected)
            {
                if (overlaps(time, other.time))
                {
                    return std::unexpected(
                        QObject::tr(
                            "The proposed schedule overlaps: %1 conflicts with %2 on %3."
                            )
                            .arg(
                                classroom.label,
                                other.label,
                                time.day
                                )
                        );
                }
            }

            projected.append({classroom.label, time});
        }
    }

    return {};
}

Status writeTimes(
    QSqlDatabase& database,
    const QString& table,
    int classId,
    const QList<ClassTime>& times
    )
{
    QSqlQuery query(database);
    query.prepare(
        QStringLiteral(
            "INSERT INTO %1 "
            "(class_id, day, start_time, end_time) "
            "VALUES (?, ?, ?, ?)"
            )
            .arg(table)
        );

    for (const ClassTime& time : times)
    {
        query.bindValue(0, classId);
        query.bindValue(1, time.day);
        query.bindValue(2, time.startTime);
        query.bindValue(3, time.endTime);

        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Writing imported class times")
                    )
                );
        }
    }

    return {};
}

Status writeIntensiveSlotStates(
    QSqlDatabase& database,
    const QList<IntensiveSlotState>& states
    )
{
    static const QSet<QString> validStates{
        QStringLiteral("empty"),
        QStringLiteral("essay"),
        QStringLiteral("lunch")
    };

    QSet<QString> keys;
    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO intensive_slot_states (day, start_time, state)
        VALUES (?, ?, ?)
    )");

    for (const IntensiveSlotState& state : states)
    {
        const int day = dayIndex(state.day);
        const QTime startTime =
            QTime::fromString(
                state.startTime,
                QStringLiteral("HH:mm")
                );
        const QString key =
            state.day + QLatin1Char('\x1f') + state.startTime;
        if (
            day < 0
            || !startTime.isValid()
            || !validStates.contains(state.state)
            || keys.contains(key)
            )
        {
            return std::unexpected(
                QObject::tr("The import contains an invalid intensive slot state.")
                );
        }
        keys.insert(key);

        query.bindValue(0, state.day);
        query.bindValue(1, state.startTime);
        query.bindValue(2, state.state);
        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Writing imported intensive slot states")
                    )
                );
        }
    }

    return {};
}
}

ScheduleImportRepository::ScheduleImportRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<ScheduleImportPreview> ScheduleImportRepository::preview(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(
            QObject::tr("No database is open.")
            );
    }

    TeacherRepository teacherRepository(m_database);
    ClassRepository classRepository(m_database);
    ClassInfoRepository classInfoRepository(m_database);
    const QList<Teacher> teachers =
        teacherRepository.getAllTeachers();
    const QList<Classroom> classrooms =
        classRepository.getClasses();

    QHash<int, ClassInfo> classInfo;
    for (const Classroom& classroom : classrooms)
    {
        classInfo.insert(
            classroom.id,
            classInfoRepository.loadClassInfo(classroom.id)
            );
    }

    ScheduleImportPreview result;
    result.kind = kind;
    result.user = user;
    result.inventory.classCount = classrooms.size();
    for (const ClassInfo& info : classInfo)
    {
        result.inventory.hasRegularHours =
            result.inventory.hasRegularHours
            || !info.classTimes.isEmpty();
        result.inventory.hasIntensiveHours =
            result.inventory.hasIntensiveHours
            || !info.intensiveTimes.isEmpty();
    }

    QSet<QString> seenTeacherKeys;
    for (const ScheduleImportClassCandidate& candidate : user.classes)
    {
        if (seenTeacherKeys.contains(candidate.teacherKey))
        {
            continue;
        }
        seenTeacherKeys.insert(candidate.teacherKey);

        ScheduleImportTeacherPreview teacherPreview;
        teacherPreview.teacherKey = candidate.teacherKey;
        teacherPreview.teacherKr = candidate.teacherKr;

        for (const ScheduleImportClassCandidate& other : user.classes)
        {
            if (other.teacherKey != candidate.teacherKey)
            {
                continue;
            }
            for (const QString& room : other.rooms)
            {
                if (!teacherPreview.importedRooms.contains(room))
                {
                    teacherPreview.importedRooms.append(room);
                }
            }
        }

        for (const Teacher& teacher : teachers)
        {
            if (
                teacherKey(teacher.teacherKr)
                == candidate.teacherKey
                )
            {
                teacherPreview.matchingTeacherIds.append(
                    teacher.id
                    );
            }
        }

        for (const ClassInfo& info : classInfo)
        {
            if (
                teacherPreview.matchingTeacherIds.contains(
                    info.teacherId
                    )
                )
            {
                ++teacherPreview.affectedClassCount;
            }
        }

        result.teachers.append(teacherPreview);
    }

    QSet<int> exactTargets;

    for (int index = 0; index < user.classes.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate =
            user.classes[index];
        ScheduleImportClassPreview classPreview;
        classPreview.candidateIndex = index;

        QList<int> importedTeacherIds;
        for (const ScheduleImportTeacherPreview& teacher : result.teachers)
        {
            if (teacher.teacherKey == candidate.teacherKey)
            {
                importedTeacherIds =
                    teacher.matchingTeacherIds;
                break;
            }
        }

        QList<int> exact;
        QList<int> sameCourseTeacherRoomSameDays;
        QList<int> sameCourseTeacherRoom;
        QList<int> sameCourseTeacherSameDays;
        QList<int> sameCourseTeacher;
        QList<int> sameCourseSameDays;
        QList<int> sameCourse;

        for (const Classroom& classroom : classrooms)
        {
            const ClassInfo info =
                classInfo.value(classroom.id);
            if (
                !scheduleImportClassOptionIsEligible(
                    candidate,
                    info,
                    kind
                    )
                )
            {
                continue;
            }

            const bool teacherMatches =
                importedTeacherIds.contains(info.teacherId);
            const bool roomMatches =
                std::any_of(
                    candidate.rooms.cbegin(),
                    candidate.rooms.cend(),
                    [&info](const QString& room)
                    {
                        return normalized(room)
                            == normalized(info.roomNumber);
                    }
                    );
            const QList<ClassTime> targetTimes =
                scheduleImportTargetTimesForKind(info, kind);
            const bool targetDaysMatch =
                !targetTimes.isEmpty()
                && scheduleImportMeetingDaysMatch(
                    candidate.times,
                    targetTimes
                    );
            const QList<ClassTime> referenceTimes =
                scheduleImportTimesForKind(info, kind);
            const bool referenceDaysMatch =
                !referenceTimes.isEmpty()
                && scheduleImportMeetingDaysMatch(
                    candidate.times,
                    referenceTimes
                    );

            if (
                teacherMatches
                && roomMatches
                && targetDaysMatch
                )
            {
                appendUnique(&exact, classroom.id);
            }
            else if (
                teacherMatches
                && roomMatches
                && referenceDaysMatch
                )
            {
                appendUnique(
                    &sameCourseTeacherRoomSameDays,
                    classroom.id
                    );
            }
            else if (
                teacherMatches
                && roomMatches
                )
            {
                appendUnique(
                    &sameCourseTeacherRoom,
                    classroom.id
                    );
            }
            else if (teacherMatches && referenceDaysMatch)
            {
                appendUnique(
                    &sameCourseTeacherSameDays,
                    classroom.id
                    );
            }
            else if (teacherMatches)
            {
                appendUnique(
                    &sameCourseTeacher,
                    classroom.id
                    );
            }
            else if (referenceDaysMatch)
            {
                appendUnique(
                    &sameCourseSameDays,
                    classroom.id
                    );
            }
            else
            {
                appendUnique(&sameCourse, classroom.id);
            }
        }

        for (int classId : exact)
        {
            appendUnique(&classPreview.matchingClassIds, classId);
        }
        for (int classId : sameCourseTeacherRoomSameDays)
        {
            appendUnique(&classPreview.matchingClassIds, classId);
        }
        for (int classId : sameCourseTeacherRoom)
        {
            appendUnique(&classPreview.matchingClassIds, classId);
        }
        for (int classId : sameCourseTeacherSameDays)
        {
            appendUnique(&classPreview.matchingClassIds, classId);
        }
        for (int classId : sameCourseTeacher)
        {
            appendUnique(&classPreview.matchingClassIds, classId);
        }
        for (int classId : sameCourseSameDays)
        {
            appendUnique(&classPreview.matchingClassIds, classId);
        }
        for (int classId : sameCourse)
        {
            appendUnique(&classPreview.matchingClassIds, classId);
        }

        if (exact.size() == 1)
        {
            classPreview.suggestedClassId =
                exact.first();
            classPreview.exactMatch = true;
            classPreview.matchConfidence =
                ScheduleImportClassMatchConfidence::Confident;
            classPreview.matchExplanation =
                QObject::tr(
                    "One existing class matches the imported grade, level, Korean teacher, room, and meeting days."
                    );
            exactTargets.insert(exact.first());
        }
        else if (!classPreview.matchingClassIds.isEmpty())
        {
            classPreview.suggestedClassId =
                classPreview.matchingClassIds.first();
            classPreview.matchConfidence =
                ScheduleImportClassMatchConfidence::Possible;

            bool hasTargetHours = false;
            bool hasOtherHours = false;
            for (int classId : classPreview.matchingClassIds)
            {
                const ClassInfo info = classInfo.value(classId);
                hasTargetHours =
                    hasTargetHours
                    || !scheduleImportTargetTimesForKind(
                        info,
                        kind
                        ).isEmpty();
                hasOtherHours =
                    hasOtherHours
                    || !scheduleImportTimesForKind(
                        info,
                        kind
                        ).isEmpty();
            }

            classPreview.matchExplanation =
                hasTargetHours
                    ? QObject::tr(
                        "Possible existing classes share the imported grade and level and have a compatible weekday group."
                        )
                    : hasOtherHours
                        ? QObject::tr(
                            "Possible existing classes have hours only in the other schedule type; their grade, level, and weekday group are compatible."
                            )
                        : QObject::tr(
                            "Possible existing classes share the imported grade and level but have no schedule hours to compare."
                            );
        }
        else
        {
            classPreview.matchExplanation =
                QObject::tr(
                    "No existing class has the same grade and level with a compatible weekday group."
                    );
        }

        result.classes.append(classPreview);
    }

    for (const Classroom& classroom : classrooms)
    {
        if (!exactTargets.contains(classroom.id))
        {
            result.initiallyAbsentClassIds.append(
                classroom.id
                );
        }
    }

    return result;
}

Result<ScheduleImportSummary> ScheduleImportRepository::apply(
    const ScheduleImportPlan& plan
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(
            QObject::tr("No database is open.")
            );
    }

    if (
        plan.kind == ScheduleImportKind::Intensive
        && plan.intensiveMode
            != ScheduleImportIntensiveMode::UpdateExisting
        && plan.intensiveMode
            != ScheduleImportIntensiveMode::ReplaceWithNew
        )
    {
        return std::unexpected(
            QObject::tr(
                "Choose how the existing intensive schedule should be handled."
                )
            );
    }

    if (
        !plan.diagnostics.isEmpty()
        && !plan.unknownCellsAcknowledged
        )
    {
        return std::unexpected(
            QObject::tr(
                "Unrecognized timetable cells must be acknowledged before importing."
                )
            );
    }

    QHash<QString, ScheduleImportTeacherResolution>
        teacherResolutions;
    for (const ScheduleImportTeacherResolution& resolution : plan.teachers)
    {
        const bool validAction =
            resolution.action == ScheduleImportTeacherAction::Reuse
            || resolution.action == ScheduleImportTeacherAction::UpdateRoom
            || resolution.action == ScheduleImportTeacherAction::Create
            || resolution.action == ScheduleImportTeacherAction::Skip;
        if (
            !validAction
            ||
            resolution.teacherKey.isEmpty()
            || teacherResolutions.contains(resolution.teacherKey)
            )
        {
            return std::unexpected(
                QObject::tr(
                    "The teacher import plan contains an invalid or duplicate resolution."
                    )
                );
        }
        teacherResolutions.insert(
            resolution.teacherKey,
            resolution
            );
    }

    QHash<int, ScheduleImportClassResolution>
        classResolutions;
    QSet<int> claimedTargets;
    for (const ScheduleImportClassResolution& resolution : plan.classes)
    {
        const bool validAction =
            resolution.action == ScheduleImportClassAction::UpdateExisting
            || resolution.action == ScheduleImportClassAction::CreateNew
            || resolution.action == ScheduleImportClassAction::Skip;
        if (
            !validAction
            ||
            resolution.candidateIndex < 0
            || resolution.candidateIndex >= plan.candidates.size()
            || classResolutions.contains(resolution.candidateIndex)
            )
        {
            return std::unexpected(
                QObject::tr(
                    "The class import plan contains an invalid or duplicate resolution."
                    )
                );
        }
        if (
            resolution.action
                == ScheduleImportClassAction::UpdateExisting
            && (
                resolution.targetClassId <= 0
                || claimedTargets.contains(
                    resolution.targetClassId
                    )
                )
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Each updated class must have a unique existing target."
                    )
                );
        }
        if (
            resolution.targetClassId > 0
            && (
                resolution.action
                    == ScheduleImportClassAction::UpdateExisting
                || resolution.action
                    == ScheduleImportClassAction::Skip
                )
        )
        {
            if (claimedTargets.contains(resolution.targetClassId))
            {
                return std::unexpected(
                    QObject::tr(
                        "Each imported class must resolve to a unique existing target."
                        )
                    );
            }
            claimedTargets.insert(resolution.targetClassId);
        }
        if (
            resolution.action == ScheduleImportClassAction::CreateNew
            && resolution.targetClassId > 0
            )
        {
            return std::unexpected(
                QObject::tr(
                    "A newly created class cannot have an existing target."
                    )
                );
        }
        classResolutions.insert(
            resolution.candidateIndex,
            resolution
            );
    }

    QSet<QString> candidateTeacherKeys;
    QHash<QString, QStringList> importedRooms;
    for (const ScheduleImportClassCandidate& candidate : plan.candidates)
    {
        if (
            !validCourse(
                candidate.classGrade,
                candidate.classLevel
                )
            || candidate.teacherKey.isEmpty()
            || teacherKey(candidate.teacherKr)
                != candidate.teacherKey
            || candidate.times.isEmpty()
            )
        {
            return std::unexpected(
                QObject::tr(
                    "The import contains an invalid class."
                    )
                );
        }
        candidateTeacherKeys.insert(candidate.teacherKey);
        for (const QString& room : candidate.rooms)
        {
            const QString trimmedRoom =
                room.trimmed();
            if (
                !trimmedRoom.isEmpty()
                && !importedRooms[candidate.teacherKey]
                    .contains(trimmedRoom)
                )
            {
                importedRooms[candidate.teacherKey]
                    .append(trimmedRoom);
            }
        }
    }

    if (
        teacherResolutions.size()
        != candidateTeacherKeys.size()
        || classResolutions.size()
            != plan.candidates.size()
        )
    {
        return std::unexpected(
            QObject::tr(
                "Every imported teacher and class requires a resolution."
                )
            );
    }

    for (const QString& key : candidateTeacherKeys)
    {
        if (!teacherResolutions.contains(key))
        {
            return std::unexpected(
                QObject::tr(
                    "Every imported teacher requires a matching resolution."
                    )
                );
        }

        const ScheduleImportTeacherResolution resolution =
            teacherResolutions.value(key);
        const QString room =
            resolution.selectedRoom.trimmed();
        const bool roomMustBeSelected =
            resolution.action == ScheduleImportTeacherAction::Create
            || resolution.action == ScheduleImportTeacherAction::UpdateRoom
            || (
                resolution.action != ScheduleImportTeacherAction::Skip
                && importedRooms.value(key).size() > 1
                );
        if (
            roomMustBeSelected
            && (
                room.isEmpty()
                || !importedRooms.value(key).contains(room)
                )
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Choose one of the imported rooms for every unresolved Korean teacher."
                    )
                );
        }
    }

    for (int index = 0; index < plan.candidates.size(); ++index)
    {
        const ScheduleImportTeacherResolution teacherResolution =
            teacherResolutions.value(
                plan.candidates[index].teacherKey
                );
        if (
            teacherResolution.action
                == ScheduleImportTeacherAction::Skip
            && classResolutions.value(index).action
                != ScheduleImportClassAction::Skip
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Classes assigned to a skipped Korean teacher must also be skipped."
                    )
                );
        }

        const ScheduleImportClassResolution classResolution =
            classResolutions.value(index);
        const QString meetingPatternError =
            scheduleImportMeetingPatternError(
                plan.candidates[index]
                );
        if (
            classResolution.action
                != ScheduleImportClassAction::Skip
            && !meetingPatternError.isEmpty()
            )
        {
            return std::unexpected(
                QObject::tr(
                    "The meeting pattern for %1 %2 is invalid: %3"
                    )
                    .arg(
                        plan.candidates[index].classGrade,
                        plan.candidates[index].classLevel,
                        meetingPatternError
                        )
                );
        }
        if (
            classResolution.action
                != ScheduleImportClassAction::Skip
            && (
                normalizedHexColor(
                    classResolution.classColor
                    ).isEmpty()
                || normalizedHexColor(
                    classResolution.fontColor
                    ).isEmpty()
                )
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Choose a valid class color for every imported class."
                    )
                );
        }
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr(
                "Unable to start the schedule import transaction."
                )
            );
    }

    TeacherRepository teacherRepository(m_database);
    ClassRepository classRepository(m_database);
    ClassInfoRepository classInfoRepository(m_database);
    const QList<Teacher> existingTeachers =
        teacherRepository.getAllTeachers();
    const QList<Classroom> existingClasses =
        classRepository.getClasses();

    QHash<int, Teacher> teachersById;
    for (const Teacher& teacher : existingTeachers)
    {
        teachersById.insert(teacher.id, teacher);
    }

    QHash<int, ClassInfo> existingInfo;
    QSet<int> existingClassIds;
    for (const Classroom& classroom : existingClasses)
    {
        existingClassIds.insert(classroom.id);
        existingInfo.insert(
            classroom.id,
            classInfoRepository.loadClassInfo(classroom.id)
            );
    }

    for (
        auto iterator = teacherResolutions.cbegin();
        iterator != teacherResolutions.cend();
        ++iterator
        )
    {
        const ScheduleImportTeacherResolution& resolution =
            iterator.value();
        if (
            resolution.action
                == ScheduleImportTeacherAction::Reuse
            || resolution.action
                == ScheduleImportTeacherAction::UpdateRoom
            )
        {
            if (
                !teachersById.contains(resolution.targetTeacherId)
                || teacherKey(
                    teachersById.value(
                        resolution.targetTeacherId
                        ).teacherKr
                    )
                    != resolution.teacherKey
                )
            {
                return std::unexpected(
                    QObject::tr(
                        "A selected Korean teacher is no longer available."
                        )
                );
            }
        }
        else if (resolution.targetTeacherId > 0)
        {
            return std::unexpected(
                QObject::tr(
                    "A created or skipped Korean teacher cannot have an existing target."
                    )
                );
        }
        if (
            resolution.action
                == ScheduleImportTeacherAction::UpdateRoom
            && resolution.selectedRoom.trimmed().isEmpty()
            )
        {
            return std::unexpected(
                QObject::tr(
                    "Choose a room before updating a Korean teacher."
                    )
                );
        }
    }

    for (
        auto iterator = classResolutions.cbegin();
        iterator != classResolutions.cend();
        ++iterator
        )
    {
        const ScheduleImportClassResolution& resolution =
            iterator.value();
        if (
            resolution.action
                == ScheduleImportClassAction::UpdateExisting
            && !existingClassIds.contains(
                resolution.targetClassId
                )
            )
        {
            return std::unexpected(
                QObject::tr(
                    "A selected class is no longer available."
                    )
                );
        }
    }

    for (
        auto iterator = classResolutions.cbegin();
        iterator != classResolutions.cend();
        ++iterator
        )
    {
        const ScheduleImportClassResolution& resolution =
            iterator.value();
        if (
            resolution.action != ScheduleImportClassAction::Skip
            || resolution.targetClassId <= 0
            )
        {
            continue;
        }

        const ScheduleImportClassCandidate& candidate =
            plan.candidates[resolution.candidateIndex];
        QList<int> exactTargets;
        for (const Classroom& classroom : existingClasses)
        {
            const ClassInfo info =
                existingInfo.value(classroom.id);
            if (
                normalized(info.classGrade)
                    == normalized(candidate.classGrade)
                && normalized(info.classLevel)
                    == normalized(candidate.classLevel)
                && teachersById.contains(info.teacherId)
                && teacherKey(
                    teachersById.value(info.teacherId).teacherKr
                    ) == candidate.teacherKey
                )
            {
                exactTargets.append(classroom.id);
            }
        }

        if (
            exactTargets.size() != 1
            || exactTargets.first() != resolution.targetClassId
            )
        {
            return std::unexpected(
                QObject::tr(
                    "A skipped imported class can preserve only its unique exact existing match."
                    )
                );
        }
    }

    const Status projected =
        validateProjectedSchedule(
            plan,
            existingInfo,
            classResolutions
            );
    if (!projected)
    {
        return std::unexpected(projected.error());
    }

    ScheduleImportSummary summary;
    summary.ignoredCells =
        plan.diagnostics.size();
    QHash<QString, int> resolvedTeacherIds;
    QSqlQuery query(m_database);

    for (
        auto iterator = teacherResolutions.cbegin();
        iterator != teacherResolutions.cend();
        ++iterator
        )
    {
        const ScheduleImportTeacherResolution& resolution =
            iterator.value();

        if (
            resolution.action
                == ScheduleImportTeacherAction::Skip
            )
        {
            resolvedTeacherIds.insert(iterator.key(), -1);
            continue;
        }

        if (
            resolution.action
                == ScheduleImportTeacherAction::Create
            )
        {
            QString teacherName;
            for (const ScheduleImportClassCandidate& candidate : plan.candidates)
            {
                if (candidate.teacherKey == iterator.key())
                {
                    teacherName = candidate.teacherKr;
                    break;
                }
            }
            teacherName =
                teacherKey(teacherName);

            query.prepare(R"(
                INSERT INTO teachers (
                    teacher_kr,
                    room_number
                )
                VALUES (?, ?)
            )");
            query.addBindValue(teacherName);
            query.addBindValue(
                resolution.selectedRoom.trimmed()
                );

            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Creating a Korean teacher")
                        )
                    );
            }

            const int teacherId =
                query.lastInsertId().toInt();
            if (teacherId <= 0)
            {
                return std::unexpected(
                    QObject::tr(
                        "A Korean teacher could not be created."
                        )
                    );
            }
            resolvedTeacherIds.insert(iterator.key(), teacherId);
            ++summary.teachersCreated;
            continue;
        }

        resolvedTeacherIds.insert(
            iterator.key(),
            resolution.targetTeacherId
            );

        if (
            resolution.action
                == ScheduleImportTeacherAction::UpdateRoom
            )
        {
            query.prepare(R"(
                UPDATE teachers
                SET room_number=?
                WHERE id=?
            )");
            query.addBindValue(
                resolution.selectedRoom.trimmed()
                );
            query.addBindValue(resolution.targetTeacherId);

            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Updating a Korean teacher room")
                        )
                    );
            }
            ++summary.teachersUpdated;
        }
    }

    const QString timeTable =
        plan.kind == ScheduleImportKind::Intensive
            ? QStringLiteral("class_intensive_times")
            : QStringLiteral("class_times");
    const bool preservesAbsentIntensiveClasses =
        plan.kind == ScheduleImportKind::Intensive
        && plan.intensiveMode
            == ScheduleImportIntensiveMode::UpdateExisting;
    QHash<int, QList<ClassTime>> finalTimes;
    for (int index = 0; index < plan.candidates.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate =
            plan.candidates[index];
        const ScheduleImportClassResolution resolution =
            classResolutions.value(index);

        if (
            resolution.action
                == ScheduleImportClassAction::Skip
            )
        {
            ++summary.classesSkipped;
            if (
                !preservesAbsentIntensiveClasses
                && resolution.targetClassId > 0
                && existingInfo.contains(resolution.targetClassId)
                )
            {
                finalTimes.insert(
                    resolution.targetClassId,
                    selectedTimes(
                        existingInfo.value(
                            resolution.targetClassId
                            ),
                        plan.kind
                        )
                    );
            }
            continue;
        }

        const int teacherId =
            resolvedTeacherIds.value(
                candidate.teacherKey,
                -1
                );
        if (teacherId <= 0)
        {
            return std::unexpected(
                QObject::tr(
                    "A class cannot be imported because its Korean teacher was skipped."
                    )
                );
        }

        int classId =
            resolution.targetClassId;

        if (
            resolution.action
                == ScheduleImportClassAction::CreateNew
            )
        {
            query.prepare(
                QStringLiteral(
                    "INSERT INTO classes (name) VALUES (?)"
                    )
                );
            query.addBindValue(
                QStringLiteral("%1 %2")
                    .arg(
                        candidate.classGrade,
                        candidate.classLevel
                        )
                    .simplified()
                );
            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Creating a class")
                        )
                    );
            }
            classId =
                query.lastInsertId().toInt();
            ++summary.classesCreated;
        }
        else
        {
            ++summary.classesUpdated;
        }

        query.prepare(R"(
            INSERT INTO class_info (
                class_id,
                teacher_id,
                class_grade,
                class_level,
                class_color,
                font_color
            )
            VALUES (?, ?, ?, ?, ?, ?)
            ON CONFLICT(class_id)
            DO UPDATE SET
                teacher_id=excluded.teacher_id,
                class_grade=excluded.class_grade,
                class_level=excluded.class_level,
                class_color=excluded.class_color,
                font_color=excluded.font_color
        )");
        query.addBindValue(classId);
        query.addBindValue(teacherId);
        query.addBindValue(candidate.classGrade);
        query.addBindValue(candidate.classLevel);
        query.addBindValue(
            normalizedHexColor(
                resolution.classColor
                )
            );
        query.addBindValue(
            normalizedHexColor(
                resolution.fontColor
                )
            );

        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Updating imported class information")
                    )
                );
        }

        finalTimes.insert(classId, candidate.times);
    }

    if (!preservesAbsentIntensiveClasses)
    {
        for (const Classroom& classroom : existingClasses)
        {
            const bool hadTimes =
                !selectedTimes(
                    existingInfo.value(classroom.id),
                    plan.kind
                    ).isEmpty();
            if (
                hadTimes
                && !finalTimes.contains(classroom.id)
                )
            {
                ++summary.schedulesCleared;
            }
        }
    }

    if (preservesAbsentIntensiveClasses)
    {
        query.prepare(
            QStringLiteral(
                "DELETE FROM %1 WHERE class_id=?"
                )
                .arg(timeTable)
            );
        for (
            auto iterator = finalTimes.cbegin();
            iterator != finalTimes.cend();
            ++iterator
            )
        {
            query.bindValue(0, iterator.key());
            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr(
                            "Clearing an existing intensive class schedule"
                            )
                        )
                    );
            }
        }
    }
    else if (
        !query.exec(
            QStringLiteral("DELETE FROM %1")
                .arg(timeTable)
            )
        )
    {
        return std::unexpected(
            queryFailure(
                query,
                QObject::tr("Clearing the previous schedule snapshot")
                )
            );
    }

    for (
        auto iterator = finalTimes.cbegin();
        iterator != finalTimes.cend();
        ++iterator
        )
    {
        const Status written =
            writeTimes(
                m_database,
                timeTable,
                iterator.key(),
                iterator.value()
                );
        if (!written)
        {
            return std::unexpected(written.error());
        }
    }

    if (plan.kind == ScheduleImportKind::Intensive)
    {
        if (!query.exec(QStringLiteral("DELETE FROM intensive_slot_states")))
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Clearing the previous intensive slot states")
                    )
                );
        }

        const Status statesWritten =
            writeIntensiveSlotStates(
                m_database,
                plan.intensiveSlotStates
                );
        if (!statesWritten)
        {
            return std::unexpected(statesWritten.error());
        }
    }

    if (plan.saveProfileNameIfBlank)
    {
        query.prepare(
            QStringLiteral(
                "SELECT value FROM app_settings WHERE key='myInfo/name'"
                )
            );
        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Reading My Information name")
                    )
                );
        }

        QString existingName;
        if (query.next())
        {
            existingName =
                query.value(0).toString().trimmed();
        }

        if (
            existingName.isEmpty()
            && !plan.selectedUserName.trimmed().isEmpty()
            )
        {
            query.prepare(R"(
                INSERT INTO app_settings (key, value)
                VALUES ('myInfo/name', ?)
                ON CONFLICT(key)
                DO UPDATE SET value=excluded.value
            )");
            query.addBindValue(
                plan.selectedUserName.trimmed()
                );
            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Saving My Information name")
                        )
                    );
            }
            summary.profileNameUpdated = true;
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr(
                "Unable to commit the schedule import transaction: %1"
                )
                .arg(m_database.lastError().text())
            );
    }

    return summary;
}
