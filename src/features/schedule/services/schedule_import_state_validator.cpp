#include "schedule_import_state_validator.h"

#include "features/teacher/import/teacher_import_name_utils.h"

#include <QObject>
#include <QSet>
#include <QTime>

namespace
{
QString teacherKey(const QString& value)
{
    return TeacherImportNameUtils::hangulOnly(value);
}

QString normalized(const QString& value)
{
    return value.simplified().toCaseFolded();
}

int dayIndex(const QString& day)
{
    static const QStringList days{
        QStringLiteral("Monday"), QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"), QStringLiteral("Thursday"),
        QStringLiteral("Friday"), QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
    return days.indexOf(day);
}

int timeMinutes(const QString& value)
{
    for (const QString& format : {
             QStringLiteral("h:mm AP"), QStringLiteral("h:mmAP"),
             QStringLiteral("H:mm"), QStringLiteral("HH:mm")
             })
    {
        const QTime time = QTime::fromString(value.trimmed(), format);
        if (time.isValid())
        {
            return time.hour() * 60 + time.minute();
        }
    }
    return -1;
}

struct Interval
{
    int start = -1;
    int end = -1;
};

bool intervalFor(const ClassTime& time, Interval* interval)
{
    const int day = dayIndex(time.day);
    const int start = timeMinutes(time.startTime);
    const int end = timeMinutes(time.endTime);
    if (!interval || day < 0 || start < 0 || end <= start)
    {
        return false;
    }
    interval->start = day * 24 * 60 + start;
    interval->end = day * 24 * 60 + end;
    return true;
}

bool overlaps(const ClassTime& left, const ClassTime& right)
{
    Interval leftInterval;
    Interval rightInterval;
    return intervalFor(left, &leftInterval)
        && intervalFor(right, &rightInterval)
        && leftInterval.start < rightInterval.end
        && rightInterval.start < leftInterval.end;
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
    const QHash<int, ScheduleImportClassResolution>& resolutions
    )
{
    struct ProjectedClass
    {
        QString label;
        QList<ClassTime> times;
    };
    struct Scheduled
    {
        QString label;
        ClassTime time;
    };

    QHash<int, ProjectedClass> projectedClasses;
    const bool preserveAbsent = plan.kind == ScheduleImportKind::Intensive
        && plan.intensiveMode == ScheduleImportIntensiveMode::UpdateExisting;
    if (preserveAbsent)
    {
        for (auto iterator = existingInfo.cbegin();
             iterator != existingInfo.cend();
             ++iterator)
        {
            const QList<ClassTime> times = selectedTimes(
                iterator.value(),
                plan.kind
                );
            if (!times.isEmpty())
            {
                projectedClasses.insert(
                    iterator.key(),
                    {
                        QStringLiteral("%1 %2")
                            .arg(
                                iterator.value().classGrade,
                                iterator.value().classLevel
                                ).simplified(),
                        times
                    }
                    );
            }
        }
    }

    for (int index = 0; index < plan.candidates.size(); ++index)
    {
        const auto resolution = resolutions.value(index);
        const auto& candidate = plan.candidates.at(index);
        if (resolution.action == ScheduleImportClassAction::Skip)
        {
            if (!preserveAbsent
                && resolution.targetClassId > 0
                && existingInfo.contains(resolution.targetClassId))
            {
                const ClassInfo info = existingInfo.value(
                    resolution.targetClassId
                    );
                projectedClasses.insert(
                    resolution.targetClassId,
                    {
                        QStringLiteral("%1 %2")
                            .arg(info.classGrade, info.classLevel).simplified(),
                        selectedTimes(info, plan.kind)
                    }
                    );
            }
            continue;
        }

        const int projectedId =
            resolution.action == ScheduleImportClassAction::UpdateExisting
                ? resolution.targetClassId
                : -(index + 1);
        projectedClasses.insert(
            projectedId,
            {
                QStringLiteral("%1 %2")
                    .arg(candidate.classGrade, candidate.classLevel),
                candidate.times
            }
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
                return std::unexpected(QObject::tr(
                    "%1 contains an invalid time: %2 %3–%4"
                    ).arg(
                        classroom.label,
                        time.day,
                        time.startTime,
                        time.endTime
                        ));
            }
            for (const Scheduled& other : projected)
            {
                if (overlaps(time, other.time))
                {
                    return std::unexpected(QObject::tr(
                        "The proposed schedule overlaps: %1 conflicts with %2 on %3."
                        ).arg(classroom.label, other.label, time.day));
                }
            }
            projected.append({classroom.label, time});
        }
    }
    return {};
}
}

Status ScheduleImportStateValidator::validate(
    const ScheduleImportPlan& plan,
    const ValidatedScheduleImportPlan& validatedPlan,
    const QList<Teacher>& existingTeachers,
    const QList<Classroom>& existingClasses,
    const QHash<int, ClassInfo>& existingInfo
    )
{
    QHash<int, Teacher> teachersById;
    for (const Teacher& teacher : existingTeachers)
    {
        teachersById.insert(teacher.id, teacher);
    }
    QSet<int> classIds;
    for (const Classroom& classroom : existingClasses)
    {
        classIds.insert(classroom.id);
    }

    for (const auto& resolution : validatedPlan.teacherResolutions)
    {
        if ((resolution.action == ScheduleImportTeacherAction::Reuse
             || resolution.action == ScheduleImportTeacherAction::UpdateRoom)
            && (!teachersById.contains(resolution.targetTeacherId)
                || teacherKey(
                    teachersById.value(resolution.targetTeacherId).teacherKr
                    ) != resolution.teacherKey))
        {
            return std::unexpected(QObject::tr(
                "A selected Korean teacher is no longer available."
                ));
        }
        if ((resolution.action == ScheduleImportTeacherAction::Create
             || resolution.action == ScheduleImportTeacherAction::Skip)
            && resolution.targetTeacherId > 0)
        {
            return std::unexpected(QObject::tr(
                "A created or skipped Korean teacher cannot have an existing target."
                ));
        }
        if (resolution.action == ScheduleImportTeacherAction::UpdateRoom
            && resolution.selectedRoom.trimmed().isEmpty())
        {
            return std::unexpected(QObject::tr(
                "Choose a room before updating a Korean teacher."
                ));
        }
    }

    for (const auto& resolution : validatedPlan.classResolutions)
    {
        if (resolution.action == ScheduleImportClassAction::UpdateExisting
            && !classIds.contains(resolution.targetClassId))
        {
            return std::unexpected(QObject::tr(
                "A selected class is no longer available."
                ));
        }
        if (resolution.action != ScheduleImportClassAction::Skip
            || resolution.targetClassId <= 0)
        {
            continue;
        }

        const auto& candidate = plan.candidates.at(
            resolution.candidateIndex
            );
        QList<int> exactTargets;
        for (const Classroom& classroom : existingClasses)
        {
            const ClassInfo info = existingInfo.value(classroom.id);
            if (normalized(info.classGrade) == normalized(candidate.classGrade)
                && normalized(info.classLevel) == normalized(candidate.classLevel)
                && teachersById.contains(info.teacherId)
                && teacherKey(teachersById.value(info.teacherId).teacherKr)
                    == candidate.teacherKey)
            {
                exactTargets.append(classroom.id);
            }
        }
        if (exactTargets.size() != 1
            || exactTargets.constFirst() != resolution.targetClassId)
        {
            return std::unexpected(QObject::tr(
                "A skipped imported class can preserve only its unique exact existing match."
                ));
        }
    }

    return validateProjectedSchedule(
        plan,
        existingInfo,
        validatedPlan.classResolutions
        );
}
