#pragma once

#include "next/application/schedule_import_overlap_projection.h"
#include "next/domain/domain_types.h"

#include <charconv>
#include <map>
#include <optional>
#include <string>
#include <system_error>
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
    std::optional<Domain::TeacherId> targetTeacherId;
    bool roomSelected = false;
};

struct ScheduleImportStateClassResolution
{
    std::size_t candidateIndex = 0;
    ScheduleImportStateClassAction action =
        ScheduleImportStateClassAction::CreateNew;
    std::optional<Domain::ClassId> targetClassId;
};

struct ScheduleImportStateTeacherSnapshot
{
    Domain::TeacherId id;
    std::string teacherKey;
};

struct ScheduleImportStateClassSnapshot
{
    Domain::ClassId id;
    Domain::TeacherId teacherId;
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

// Legacy apply state projected into the Qt-free contract still orders classes
// by their numeric database IDs. TypedId's default ordering compares strings,
// which would put "10" before "2" and change which conflict is reported first.
struct ScheduleImportStateClassIdNumericLess
{
    [[nodiscard]] bool operator()(
        const Domain::ClassId& left,
        const Domain::ClassId& right
        ) const
    {
        const auto leftNumber = numericValue(left);
        const auto rightNumber = numericValue(right);
        if (leftNumber.has_value() != rightNumber.has_value())
        {
            return leftNumber.has_value();
        }
        if (leftNumber && rightNumber)
        {
            return *leftNumber < *rightNumber;
        }
        return left.value() < right.value();
    }

private:
    [[nodiscard]] static std::optional<int> numericValue(
        const Domain::ClassId& id
        )
    {
        int value = 0;
        const auto [end, error] = std::from_chars(
            id.value().data(),
            id.value().data() + id.value().size(),
            value
            );
        if (error != std::errc{}
            || end != id.value().data() + id.value().size())
        {
            return std::nullopt;
        }
        return value;
    }
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

    struct ProjectedClass
    {
        std::string label;
        std::vector<ScheduleImportStateTime> times;
    };
    const bool preservesAbsentIntensiveClasses =
        request.kind == ScheduleImportStateKind::Intensive
        && request.intensiveMode
            == ScheduleImportStateIntensiveMode::UpdateExisting;
    std::map<
        Domain::ClassId,
        ProjectedClass,
        ScheduleImportStateClassIdNumericLess
        > projectedClasses;
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
                && resolution.targetClassId)
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

        const Domain::ClassId projectedId =
            resolution.action == ScheduleImportStateClassAction::UpdateExisting
                ? *resolution.targetClassId
                : Domain::ClassId::fromString(std::to_string(
                      -static_cast<int>(resolution.candidateIndex + 1)
                      )).value();
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
            auto projectedTime = projectScheduleImportStateTime(time);
            if (!projectedTime)
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
            projectedSchedule.times.push_back(std::move(*projectedTime));
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
