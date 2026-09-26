#pragma once

#include "next/application/schedule_import_overlap_projection.h"
#include "next/domain/domain_types.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
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

struct ScheduleImportStateCandidateIndex
{
    std::size_t value = 0;
};

// New candidates deliberately remain candidate references until the repository
// has inserted them and can map the candidate index to its generated ID.
using ScheduleImportStateClassReference =
    std::variant<Domain::ClassId, ScheduleImportStateCandidateIndex>;

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

struct ScheduleImportStateProjectedSchedule
{
    ScheduleImportStateClassReference classReference;
    std::string classLabel;
    std::vector<ScheduleImportStateTime> times;
    enum class Persistence
    {
        ReplaceRows,
        KeepExistingRows
    } persistence = Persistence::ReplaceRows;
};

// Existing classes retain numeric database-ID ordering. The former projected
// map used negative IDs for new candidates, which placed them before existing
// IDs and ordered candidates in reverse index order. Keep that deterministic
// order explicitly without pretending a candidate index is a database ID.
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

struct ScheduleImportStateClassReferenceLess
{
    [[nodiscard]] bool operator()(
        const ScheduleImportStateClassReference& left,
        const ScheduleImportStateClassReference& right
        ) const
    {
        const auto* leftCandidate =
            std::get_if<ScheduleImportStateCandidateIndex>(&left);
        const auto* rightCandidate =
            std::get_if<ScheduleImportStateCandidateIndex>(&right);
        if (leftCandidate != nullptr || rightCandidate != nullptr)
        {
            if (leftCandidate == nullptr)
            {
                return false;
            }
            if (rightCandidate == nullptr)
            {
                return true;
            }
            return leftCandidate->value > rightCandidate->value;
        }
        return ScheduleImportStateClassIdNumericLess{}(
            std::get<Domain::ClassId>(left),
            std::get<Domain::ClassId>(right)
            );
    }
};

// Selects the exact regular or intensive schedule rows that apply will
// persist. Order is deterministic; each schedule's meetings retain their
// source order, including the workbook's selected-day ordering.
[[nodiscard]] inline std::vector<ScheduleImportStateProjectedSchedule>
projectScheduleImportStateSchedules(
    const ScheduleImportStateValidationRequest& request
    )
{
    std::map<
        ScheduleImportStateClassReference,
        ScheduleImportStateProjectedSchedule,
        ScheduleImportStateClassReferenceLess
        > projected;
    const bool preservesAbsentIntensiveClasses =
        request.kind == ScheduleImportStateKind::Intensive
        && request.intensiveMode
            == ScheduleImportStateIntensiveMode::UpdateExisting;
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
                const ScheduleImportStateClassReference reference =
                    classroom.id;
                projected.insert_or_assign(
                    reference,
                    ScheduleImportStateProjectedSchedule{
                        reference,
                        classroom.classLabel,
                        times,
                        ScheduleImportStateProjectedSchedule::Persistence::
                            KeepExistingRows
                    }
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
                const auto classroom = std::find_if(
                    request.existingClasses.begin(),
                    request.existingClasses.end(),
                    [&resolution](const auto& value)
                    {
                        return value.id == *resolution.targetClassId;
                    }
                    );
                if (classroom != request.existingClasses.end())
                {
                    const ScheduleImportStateClassReference reference =
                        classroom->id;
                    projected.insert_or_assign(
                        reference,
                        ScheduleImportStateProjectedSchedule{
                            reference,
                            classroom->classLabel,
                            selectedTimes(*classroom)
                        }
                        );
                }
            }
            continue;
        }

        ScheduleImportStateClassReference reference =
            ScheduleImportStateCandidateIndex{resolution.candidateIndex};
        if (resolution.action
                == ScheduleImportStateClassAction::UpdateExisting
            && resolution.targetClassId)
        {
            reference = *resolution.targetClassId;
        }
        projected.insert_or_assign(
            reference,
            ScheduleImportStateProjectedSchedule{
                reference,
                candidate.classLabel,
                candidate.times
            }
            );
    }

    std::vector<ScheduleImportStateProjectedSchedule> result;
    result.reserve(projected.size());
    for (auto& [reference, schedule] : projected)
    {
        static_cast<void>(reference);
        result.push_back(std::move(schedule));
    }
    return result;
}

} // namespace ClassMngr::Next::Application
