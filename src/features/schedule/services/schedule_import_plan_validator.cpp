#include "schedule_import_plan_validator.h"

#include "domain/rules/schedule_import_rules.h"
#include "features/classes/config/class_info_config.h"
#include "features/teacher/import/teacher_import_name_utils.h"

#include <QObject>
#include <QRegularExpression>
#include <QSet>

namespace
{
bool validCourse(const QString& grade, const QString& level)
{
    return ClassInfoConfig::Grades.contains(grade)
        && ClassInfoConfig::levelsForGrade(grade).contains(level);
}

QString normalizedHexColor(const QString& value)
{
    static const QRegularExpression expression(
        QStringLiteral("^#[0-9A-Fa-f]{6}$")
        );
    const QString color = value.trimmed();
    return expression.match(color).hasMatch()
        ? color.toUpper()
        : QString();
}
}

Result<ValidatedScheduleImportPlan> ScheduleImportPlanValidator::validate(
    const ScheduleImportPlan& plan
    )
{
    if (
        plan.kind == ScheduleImportKind::Intensive
        && plan.intensiveMode != ScheduleImportIntensiveMode::UpdateExisting
        && plan.intensiveMode != ScheduleImportIntensiveMode::ReplaceWithNew
        )
    {
        return std::unexpected(
            QObject::tr(
                "Choose how the existing intensive schedule should be handled."
                )
            );
    }

    if (!plan.diagnostics.isEmpty() && !plan.unknownCellsAcknowledged)
    {
        return std::unexpected(
            QObject::tr(
                "Unrecognized timetable cells must be acknowledged before importing."
                )
            );
    }

    ValidatedScheduleImportPlan validated;
    for (const ScheduleImportTeacherResolution& resolution : plan.teachers)
    {
        const bool validAction =
            resolution.action == ScheduleImportTeacherAction::Reuse
            || resolution.action == ScheduleImportTeacherAction::UpdateRoom
            || resolution.action == ScheduleImportTeacherAction::Create
            || resolution.action == ScheduleImportTeacherAction::Skip;
        if (
            !validAction
            || resolution.teacherKey.isEmpty()
            || validated.teacherResolutions.contains(resolution.teacherKey)
            )
        {
            return std::unexpected(
                QObject::tr(
                    "The teacher import plan contains an invalid or duplicate resolution."
                    )
                );
        }
        validated.teacherResolutions.insert(
            resolution.teacherKey,
            resolution
            );
    }

    QSet<int> claimedTargets;
    for (const ScheduleImportClassResolution& resolution : plan.classes)
    {
        const bool validAction =
            resolution.action == ScheduleImportClassAction::UpdateExisting
            || resolution.action == ScheduleImportClassAction::CreateNew
            || resolution.action == ScheduleImportClassAction::Skip;
        if (
            !validAction
            || resolution.candidateIndex < 0
            || resolution.candidateIndex >= plan.candidates.size()
            || validated.classResolutions.contains(resolution.candidateIndex)
            )
        {
            return std::unexpected(
                QObject::tr(
                    "The class import plan contains an invalid or duplicate resolution."
                    )
                );
        }
        if (
            resolution.action == ScheduleImportClassAction::UpdateExisting
            && (
                resolution.targetClassId <= 0
                || claimedTargets.contains(resolution.targetClassId)
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
                resolution.action == ScheduleImportClassAction::UpdateExisting
                || resolution.action == ScheduleImportClassAction::Skip
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
        validated.classResolutions.insert(
            resolution.candidateIndex,
            resolution
            );
    }

    QSet<QString> candidateTeacherKeys;
    QHash<QString, QStringList> importedRooms;
    for (const ScheduleImportClassCandidate& candidate : plan.candidates)
    {
        if (
            !validCourse(candidate.classGrade, candidate.classLevel)
            || candidate.teacherKey.isEmpty()
            || TeacherImportNameUtils::hangulOnly(candidate.teacherKr)
                != candidate.teacherKey
            || candidate.times.isEmpty()
            )
        {
            return std::unexpected(
                QObject::tr("The import contains an invalid class.")
                );
        }
        candidateTeacherKeys.insert(candidate.teacherKey);
        for (const QString& room : candidate.rooms)
        {
            const QString trimmedRoom = room.trimmed();
            if (
                !trimmedRoom.isEmpty()
                && !importedRooms[candidate.teacherKey].contains(trimmedRoom)
                )
            {
                importedRooms[candidate.teacherKey].append(trimmedRoom);
            }
        }
    }

    if (
        validated.teacherResolutions.size() != candidateTeacherKeys.size()
        || validated.classResolutions.size() != plan.candidates.size()
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
        if (!validated.teacherResolutions.contains(key))
        {
            return std::unexpected(
                QObject::tr(
                    "Every imported teacher requires a matching resolution."
                    )
                );
        }

        const ScheduleImportTeacherResolution resolution =
            validated.teacherResolutions.value(key);
        const QString room = resolution.selectedRoom.trimmed();
        const bool roomMustBeSelected =
            resolution.action == ScheduleImportTeacherAction::Create
            || resolution.action == ScheduleImportTeacherAction::UpdateRoom
            || (
                resolution.action != ScheduleImportTeacherAction::Skip
                && importedRooms.value(key).size() > 1
                );
        if (
            roomMustBeSelected
            && (room.isEmpty() || !importedRooms.value(key).contains(room))
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
            validated.teacherResolutions.value(
                plan.candidates[index].teacherKey
                );
        if (
            teacherResolution.action == ScheduleImportTeacherAction::Skip
            && validated.classResolutions.value(index).action
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
            validated.classResolutions.value(index);
        const QString meetingPatternError =
            scheduleImportMeetingPatternError(plan.candidates[index]);
        if (
            classResolution.action != ScheduleImportClassAction::Skip
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
            classResolution.action != ScheduleImportClassAction::Skip
            && (
                normalizedHexColor(classResolution.classColor).isEmpty()
                || normalizedHexColor(classResolution.fontColor).isEmpty()
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

    return validated;
}
