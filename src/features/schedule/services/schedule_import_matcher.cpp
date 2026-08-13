#include "schedule_import_matcher.h"

#include "domain/rules/schedule_import_rules.h"
#include "features/teacher/import/teacher_import_name_utils.h"

#include <QObject>
#include <QSet>

#include <algorithm>

namespace
{
QString normalized(const QString& value)
{
    return value.simplified().toCaseFolded();
}

void appendUnique(QList<int>* values, int value)
{
    if (values && value > 0 && !values->contains(value))
    {
        values->append(value);
    }
}
}

ScheduleImportPreview ScheduleImportMatcher::preview(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind,
    const QList<Teacher>& teachers,
    const QList<Classroom>& classrooms,
    const QHash<int, ClassInfo>& classInfo
    )
{
    ScheduleImportPreview result;
    result.kind = kind;
    result.user = user;
    result.inventory.classCount = classrooms.size();
    for (const ClassInfo& info : classInfo)
    {
        result.inventory.hasRegularHours =
            result.inventory.hasRegularHours || !info.classTimes.isEmpty();
        result.inventory.hasIntensiveHours =
            result.inventory.hasIntensiveHours || !info.intensiveTimes.isEmpty();
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
                TeacherImportNameUtils::hangulOnly(teacher.teacherKr)
                == candidate.teacherKey
                )
            {
                teacherPreview.matchingTeacherIds.append(teacher.id);
            }
        }
        for (const ClassInfo& info : classInfo)
        {
            if (teacherPreview.matchingTeacherIds.contains(info.teacherId))
            {
                ++teacherPreview.affectedClassCount;
            }
        }
        result.teachers.append(teacherPreview);
    }

    QSet<int> exactTargets;
    for (int index = 0; index < user.classes.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate = user.classes[index];
        ScheduleImportClassPreview classPreview;
        classPreview.candidateIndex = index;

        QList<int> importedTeacherIds;
        for (const ScheduleImportTeacherPreview& teacher : result.teachers)
        {
            if (teacher.teacherKey == candidate.teacherKey)
            {
                importedTeacherIds = teacher.matchingTeacherIds;
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
            const ClassInfo info = classInfo.value(classroom.id);
            if (!scheduleImportClassOptionIsEligible(candidate, info, kind))
            {
                continue;
            }

            const bool teacherMatches =
                importedTeacherIds.contains(info.teacherId);
            const bool roomMatches = std::any_of(
                candidate.rooms.cbegin(),
                candidate.rooms.cend(),
                [&info](const QString& room)
                {
                    return normalized(room) == normalized(info.roomNumber);
                }
                );
            const QList<ClassTime> targetTimes =
                scheduleImportTargetTimesForKind(info, kind);
            const bool targetDaysMatch =
                !targetTimes.isEmpty()
                && scheduleImportMeetingDaysMatch(candidate.times, targetTimes);
            const QList<ClassTime> referenceTimes =
                scheduleImportTimesForKind(info, kind);
            const bool referenceDaysMatch =
                !referenceTimes.isEmpty()
                && scheduleImportMeetingDaysMatch(candidate.times, referenceTimes);

            if (teacherMatches && roomMatches && targetDaysMatch)
            {
                appendUnique(&exact, classroom.id);
            }
            else if (teacherMatches && roomMatches && referenceDaysMatch)
            {
                appendUnique(&sameCourseTeacherRoomSameDays, classroom.id);
            }
            else if (teacherMatches && roomMatches)
            {
                appendUnique(&sameCourseTeacherRoom, classroom.id);
            }
            else if (teacherMatches && referenceDaysMatch)
            {
                appendUnique(&sameCourseTeacherSameDays, classroom.id);
            }
            else if (teacherMatches)
            {
                appendUnique(&sameCourseTeacher, classroom.id);
            }
            else if (referenceDaysMatch)
            {
                appendUnique(&sameCourseSameDays, classroom.id);
            }
            else
            {
                appendUnique(&sameCourse, classroom.id);
            }
        }

        const QList<QList<int>> rankedMatches{
            exact,
            sameCourseTeacherRoomSameDays,
            sameCourseTeacherRoom,
            sameCourseTeacherSameDays,
            sameCourseTeacher,
            sameCourseSameDays,
            sameCourse
        };
        for (const QList<int>& matches : rankedMatches)
        {
            for (int classId : matches)
            {
                appendUnique(&classPreview.matchingClassIds, classId);
            }
        }

        if (exact.size() == 1)
        {
            classPreview.suggestedClassId = exact.first();
            classPreview.exactMatch = true;
            classPreview.matchConfidence =
                ScheduleImportClassMatchConfidence::Confident;
            classPreview.matchExplanation = QObject::tr(
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
                hasTargetHours = hasTargetHours
                    || !scheduleImportTargetTimesForKind(info, kind).isEmpty();
                hasOtherHours = hasOtherHours
                    || !scheduleImportTimesForKind(info, kind).isEmpty();
            }
            classPreview.matchExplanation = hasTargetHours
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
            classPreview.matchExplanation = QObject::tr(
                "No existing class has the same grade and level with a compatible weekday group."
                );
        }
        result.classes.append(classPreview);
    }

    for (const Classroom& classroom : classrooms)
    {
        if (!exactTargets.contains(classroom.id))
        {
            result.initiallyAbsentClassIds.append(classroom.id);
        }
    }
    return result;
}
