#pragma once

#include "next/application/schedule_import_state_projection.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class ScheduleImportStateValidationErrorCode
{
    SelectedTeacherUnavailable,
    InvalidTeacherTarget,
    MissingTeacherRoom,
    SelectedClassUnavailable,
    CreateNewClassHasTarget,
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

[[nodiscard]] inline std::optional<ScheduleImportStateValidationError>
validateScheduleImportState(
    const ScheduleImportStateValidationRequest& request,
    const std::vector<ScheduleImportStateProjectedSchedule>& projectedSchedules
    )
{
    const auto failure = [](ScheduleImportStateValidationErrorCode code)
    {
        return std::optional<ScheduleImportStateValidationError>(
            ScheduleImportStateValidationError{code}
            );
    };

    const auto teacherForId = [&request](
                                   const std::optional<Domain::TeacherId>& id
                                   )
        -> const ScheduleImportStateTeacherSnapshot*
    {
        if (!id)
        {
            return nullptr;
        }
        for (const auto& teacher : request.existingTeachers)
        {
            if (teacher.id == *id)
            {
                return &teacher;
            }
        }
        return nullptr;
    };

    const auto classForId = [&request](
                                 const std::optional<Domain::ClassId>& id
                                 )
        -> const ScheduleImportStateClassSnapshot*
    {
        if (!id)
        {
            return nullptr;
        }
        for (const auto& classroom : request.existingClasses)
        {
            if (classroom.id == *id)
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
            && resolution.targetTeacherId)
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
        if (resolution.action == ScheduleImportStateClassAction::CreateNew
            && resolution.targetClassId)
        {
            return failure(
                ScheduleImportStateValidationErrorCode::
                    CreateNewClassHasTarget
                );
        }
        if (resolution.action == ScheduleImportStateClassAction::UpdateExisting
            && !classForId(resolution.targetClassId))
        {
            return failure(
                ScheduleImportStateValidationErrorCode::SelectedClassUnavailable
                );
        }
        if (resolution.action != ScheduleImportStateClassAction::Skip
            || !resolution.targetClassId
            || resolution.candidateIndex >= request.candidates.size())
        {
            continue;
        }

        const auto& candidate = request.candidates[resolution.candidateIndex];
        std::vector<Domain::ClassId> exactTargets;
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
            || exactTargets.front() != *resolution.targetClassId)
        {
            return failure(
                ScheduleImportStateValidationErrorCode::
                    SkippedClassNotUniqueExactMatch
                );
        }
    }

    std::vector<ScheduleImportOverlapSchedule> overlapSchedules;
    overlapSchedules.reserve(projectedSchedules.size());
    for (const auto& classroom : projectedSchedules)
    {
        ScheduleImportOverlapSchedule projectedSchedule;
        projectedSchedule.classLabel = classroom.classLabel;
        for (const auto& time : classroom.times)
        {
            auto projectedTime = projectScheduleImportStateTime(time);
            if (!projectedTime)
            {
                auto error = *failure(
                    ScheduleImportStateValidationErrorCode::InvalidProjectedTime
                    );
                error.classLabel = classroom.classLabel;
                error.day = time.dayLabel;
                error.startTime = time.startLabel;
                error.endTime = time.endLabel;
                return error;
            }
            projectedSchedule.times.push_back(std::move(*projectedTime));
        }
        overlapSchedules.push_back(std::move(projectedSchedule));
    }

    const auto conflicts =
        projectScheduleImportOverlaps(overlapSchedules);
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

[[nodiscard]] inline std::optional<ScheduleImportStateValidationError>
validateScheduleImportState(
    const ScheduleImportStateValidationRequest& request
    )
{
    const auto projectedSchedules =
        projectScheduleImportStateSchedules(request);
    return validateScheduleImportState(request, projectedSchedules);
}

} // namespace ClassMngr::Next::Application
