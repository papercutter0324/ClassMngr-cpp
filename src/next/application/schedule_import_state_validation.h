#pragma once

#include "next/application/schedule_import_overlap_projection.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class ScheduleImportStateKind
{
    Normal,
    Intensive
};

enum class ScheduleImportStateIntensiveMode
{
    UpdateExisting,
    ReplaceWithNew
};

enum class ScheduleImportStateTeacherAction
{
    Reuse,
    UpdateRoom,
    Create,
    Skip
};

enum class ScheduleImportStateClassAction
{
    UpdateExisting,
    CreateNew,
    Skip
};

enum class ScheduleImportStateValidationErrorCode
{
    SelectedTeacherUnavailable,
    InvalidTeacherTarget,
    MissingTeacherRoom,
    SelectedClassUnavailable,
    SkippedClassNotUniqueExactMatch,
    InvalidProjectedTime,
    ProjectedScheduleOverlap
};

struct ScheduleImportStateValidationError
{
    ScheduleImportStateValidationErrorCode code;
    std::string classLabel;
    std::string conflictingClassLabel;
    std::string day;
    std::string startTime;
    std::string endTime;
};

struct ScheduleImportStateCandidate
{
    std::string teacherKey;
    std::string normalizedGrade;
    std::string normalizedLevel;
    std::string classLabel;
    std::vector<ScheduleImportStateTime> times;
};

struct ScheduleImportStateTeacherResolution
{
    std::string teacherKey;
    ScheduleImportStateTeacherAction action =
        ScheduleImportStateTeacherAction::Create;
    int targetTeacherId = -1;
    bool roomSelected = false;
};

struct ScheduleImportStateClassResolution
{
    std::size_t candidateIndex = 0;
    ScheduleImportStateClassAction action =
        ScheduleImportStateClassAction::CreateNew;
    int targetClassId = -1;
};

struct ScheduleImportStateTeacherSnapshot
{
    int id = -1;
    std::string teacherKey;
};

struct ScheduleImportStateClassSnapshot
{
    int id = -1;
    int teacherId = -1;
    std::string normalizedGrade;
    std::string normalizedLevel;
    std::string classLabel;
    std::vector<ScheduleImportStateTime> normalTimes;
    std::vector<ScheduleImportStateTime> intensiveTimes;
};

struct ScheduleImportStateValidationRequest
{
    ScheduleImportStateKind kind = ScheduleImportStateKind::Normal;
    ScheduleImportStateIntensiveMode intensiveMode =
        ScheduleImportStateIntensiveMode::UpdateExisting;
    std::vector<ScheduleImportStateCandidate> candidates;
    std::vector<ScheduleImportStateTeacherResolution> teacherResolutions;
    std::vector<ScheduleImportStateClassResolution> classResolutions;
    std::vector<ScheduleImportStateTeacherSnapshot> existingTeachers;
    std::vector<ScheduleImportStateClassSnapshot> existingClasses;
};

[[nodiscard]] inline std::optional<ScheduleImportStateValidationError>
validateScheduleImportState(
    const ScheduleImportStateValidationRequest& request
    )
{
    const auto failure = [](ScheduleImportStateValidationErrorCode code)
    {
        return std::optional<ScheduleImportStateValidationError>(
            ScheduleImportStateValidationError{code}
            );
    };

    const auto teacherForId = [&request](int id)
        -> const ScheduleImportStateTeacherSnapshot*
    {
        for (const auto& teacher : request.existingTeachers)
        {
            if (teacher.id == id)
            {
                return &teacher;
            }
        }
        return nullptr;
    };

    const auto classForId = [&request](int id)
        -> const ScheduleImportStateClassSnapshot*
    {
        for (const auto& classroom : request.existingClasses)
        {
            if (classroom.id == id)
            {
                return &classroom;
            }
        }
        return nullptr;
    };

    for (const auto& resolution : request.teacherResolutions)
    {
        if (resolution.action == ScheduleImportStateTeacherAction::Reuse
            || resolution.action == ScheduleImportStateTeacherAction::UpdateRoom)
        {
            const auto* teacher = teacherForId(resolution.targetTeacherId);
            if (!teacher || teacher->teacherKey != resolution.teacherKey)
            {
                return failure(
                    ScheduleImportStateValidationErrorCode::
                        SelectedTeacherUnavailable
                    );
            }
        }
        if ((resolution.action == ScheduleImportStateTeacherAction::Create
             || resolution.action == ScheduleImportStateTeacherAction::Skip)
            && resolution.targetTeacherId > 0)
        {
            return failure(
                ScheduleImportStateValidationErrorCode::InvalidTeacherTarget
                );
        }
        if (resolution.action == ScheduleImportStateTeacherAction::UpdateRoom
            && !resolution.roomSelected)
        {
            return failure(
                ScheduleImportStateValidationErrorCode::MissingTeacherRoom
                );
        }
    }

    for (const auto& resolution : request.classResolutions)
    {
        if (resolution.action == ScheduleImportStateClassAction::UpdateExisting
            && !classForId(resolution.targetClassId))
        {
            return failure(
                ScheduleImportStateValidationErrorCode::SelectedClassUnavailable
                );
        }
        if (resolution.action != ScheduleImportStateClassAction::Skip
            || resolution.targetClassId <= 0
            || resolution.candidateIndex >= request.candidates.size())
        {
            continue;
        }

        const auto& candidate = request.candidates[resolution.candidateIndex];
        std::vector<int> exactTargets;
        for (const auto& classroom : request.existingClasses)
        {
            const auto* teacher = teacherForId(classroom.teacherId);
            if (classroom.normalizedGrade == candidate.normalizedGrade
                && classroom.normalizedLevel == candidate.normalizedLevel
                && teacher
                && teacher->teacherKey == candidate.teacherKey)
            {
                exactTargets.push_back(classroom.id);
            }
        }
        if (exactTargets.size() != 1
            || exactTargets.front() != resolution.targetClassId)
        {
            return failure(
                ScheduleImportStateValidationErrorCode::
                    SkippedClassNotUniqueExactMatch
                );
        }
    }

    struct ProjectedClass
    {
        std::string label;
        std::vector<ScheduleImportStateTime> times;
    };
    const bool preservesAbsentIntensiveClasses =
        request.kind == ScheduleImportStateKind::Intensive
        && request.intensiveMode
            == ScheduleImportStateIntensiveMode::UpdateExisting;
    std::map<int, ProjectedClass> projectedClasses;
    const auto selectedTimes = [&request](
                                  const ScheduleImportStateClassSnapshot& classroom
                                  ) -> const std::vector<ScheduleImportStateTime>&
    {
        return request.kind == ScheduleImportStateKind::Intensive
            ? classroom.intensiveTimes
            : classroom.normalTimes;
    };

    if (preservesAbsentIntensiveClasses)
    {
        for (const auto& classroom : request.existingClasses)
        {
            const auto& times = selectedTimes(classroom);
            if (!times.empty())
            {
                projectedClasses.insert_or_assign(
                    classroom.id,
                    ProjectedClass{classroom.classLabel, times}
                    );
            }
        }
    }

    for (const auto& resolution : request.classResolutions)
    {
        if (resolution.candidateIndex >= request.candidates.size())
        {
            continue;
        }
        const auto& candidate = request.candidates[resolution.candidateIndex];
        if (resolution.action == ScheduleImportStateClassAction::Skip)
        {
            if (!preservesAbsentIntensiveClasses
                && resolution.targetClassId > 0)
            {
                if (const auto* classroom = classForId(resolution.targetClassId))
                {
                    projectedClasses.insert_or_assign(
                        classroom->id,
                        ProjectedClass{
                            classroom->classLabel,
                            selectedTimes(*classroom)
                        }
                        );
                }
            }
            continue;
        }

        const int projectedId =
            resolution.action == ScheduleImportStateClassAction::UpdateExisting
                ? resolution.targetClassId
                : -static_cast<int>(resolution.candidateIndex + 1);
        projectedClasses.insert_or_assign(
            projectedId,
            ProjectedClass{candidate.classLabel, candidate.times}
            );
    }

    std::vector<ScheduleImportOverlapSchedule> projectedSchedules;
    for (const auto& [classId, classroom] : projectedClasses)
    {
        static_cast<void>(classId);
        ScheduleImportOverlapSchedule projectedSchedule;
        projectedSchedule.classLabel = classroom.label;
        for (const auto& time : classroom.times)
        {
            if (time.dayIndex < 0
                || time.dayIndex > 6
                || time.startMinute < 0
                || time.startMinute >= 24 * 60
                || time.endMinute < 0
                || time.endMinute >= 24 * 60
                || time.endMinute <= time.startMinute)
            {
                auto error = *failure(
                    ScheduleImportStateValidationErrorCode::InvalidProjectedTime
                    );
                error.classLabel = classroom.label;
                error.day = time.dayLabel;
                error.startTime = time.startLabel;
                error.endTime = time.endLabel;
                return error;
            }
            projectedSchedule.times.push_back(time);
        }
        projectedSchedules.push_back(std::move(projectedSchedule));
    }

    const auto conflicts =
        projectScheduleImportOverlaps(projectedSchedules);
    if (!conflicts.empty())
    {
        auto error = *failure(
            ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap
            );
        error.classLabel = conflicts.front().classLabel;
        error.conflictingClassLabel =
            conflicts.front().conflictingClassLabel;
        error.day = conflicts.front().time.dayLabel;
        return error;
    }

    return std::nullopt;
}

} // namespace ClassMngr::Next::Application
