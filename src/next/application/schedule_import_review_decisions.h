#pragma once

#include "next/domain/domain_types.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

enum class ScheduleImportReviewTeacherAction
{
    Unselected,
    Reuse,
    UpdateRoom,
    Create,
    Skip,
    Invalid
};

enum class ScheduleImportReviewClassAction
{
    Unselected,
    UpdateExisting,
    CreateNew,
    Skip,
    Invalid
};

struct ScheduleImportReviewDecisionCandidate final
{
    std::string teacherKey;
    // Room values are normalized by the owning adapter before they cross the
    // Application boundary.
    std::vector<std::string> importedRooms;
};

struct ScheduleImportReviewTeacherResolution final
{
    std::string teacherKey;
    ScheduleImportReviewTeacherAction action =
        ScheduleImportReviewTeacherAction::Unselected;
    std::string selectedRoom;
};

struct ScheduleImportReviewClassResolution final
{
    int candidateIndex = -1;
    ScheduleImportReviewClassAction action =
        ScheduleImportReviewClassAction::Unselected;
    std::optional<Domain::ClassId> targetClassId;
};

struct ScheduleImportReviewDecisionRequest final
{
    std::vector<ScheduleImportReviewDecisionCandidate> candidates;
    std::vector<ScheduleImportReviewTeacherResolution> teachers;
    std::vector<ScheduleImportReviewClassResolution> classes;
};

enum class ScheduleImportReviewDecisionIssueCode
{
    InvalidTeacherAction,
    EmptyTeacherKey,
    DuplicateTeacherResolution,
    UnknownTeacherResolution,
    MissingTeacherResolution,
    MissingTeacherRoom,
    ForeignTeacherRoom,
    InvalidClassAction,
    CandidateIndexOutOfRange,
    DuplicateClassResolution,
    MissingClassResolution,
    UpdateClassMissingTarget,
    CreateNewClassHasTarget,
    DuplicateUpdatedClassTarget,
    DuplicateSkippedClassTarget,
    ActiveClassAssignedToSkippedTeacher
};

struct ScheduleImportReviewDecisionIssue final
{
    ScheduleImportReviewDecisionIssueCode code;
    std::string teacherKey;
    int candidateIndex = -1;
    std::optional<Domain::ClassId> targetClassId;
    std::vector<int> candidateIndexes;
};

struct ScheduleImportReviewDecisionResult final
{
    std::vector<ScheduleImportReviewDecisionIssue> issues;

    [[nodiscard]] bool accepted() const noexcept
    {
        return issues.empty();
    }
};

[[nodiscard]] inline ScheduleImportReviewDecisionResult
validateScheduleImportReviewDecisions(
    const ScheduleImportReviewDecisionRequest& request
    )
{
    using TeacherAction = ScheduleImportReviewTeacherAction;
    using ClassAction = ScheduleImportReviewClassAction;
    using Issue = ScheduleImportReviewDecisionIssue;
    using IssueCode = ScheduleImportReviewDecisionIssueCode;

    ScheduleImportReviewDecisionResult result;
    std::vector<std::string> candidateTeacherKeys;
    std::set<std::string> candidateTeacherKeySet;
    std::map<std::string, std::vector<std::string>> importedRooms;

    for (const auto& candidate : request.candidates)
    {
        if (candidateTeacherKeySet.insert(candidate.teacherKey).second)
        {
            candidateTeacherKeys.push_back(candidate.teacherKey);
        }
        auto& rooms = importedRooms[candidate.teacherKey];
        for (const auto& room : candidate.importedRooms)
        {
            if (!room.empty()
                && std::find(rooms.begin(), rooms.end(), room) == rooms.end())
            {
                rooms.push_back(room);
            }
        }
    }

    const auto validTeacherAction = [](TeacherAction action)
    {
        switch (action)
        {
        case TeacherAction::Reuse:
        case TeacherAction::UpdateRoom:
        case TeacherAction::Create:
        case TeacherAction::Skip:
            return true;
        case TeacherAction::Unselected:
        case TeacherAction::Invalid:
            return false;
        }
        return false;
    };
    const auto validClassAction = [](ClassAction action)
    {
        switch (action)
        {
        case ClassAction::UpdateExisting:
        case ClassAction::CreateNew:
        case ClassAction::Skip:
            return true;
        case ClassAction::Unselected:
        case ClassAction::Invalid:
            return false;
        }
        return false;
    };

    std::map<std::string, const ScheduleImportReviewTeacherResolution*>
        teacherResolutions;
    for (const auto& resolution : request.teachers)
    {
        if (resolution.teacherKey.empty())
        {
            result.issues.push_back(
                Issue{IssueCode::EmptyTeacherKey, resolution.teacherKey}
                );
        }
        if (!validTeacherAction(resolution.action))
        {
            result.issues.push_back(
                Issue{IssueCode::InvalidTeacherAction, resolution.teacherKey}
                );
        }
        if (resolution.teacherKey.empty())
        {
            continue;
        }
        if (!candidateTeacherKeySet.contains(resolution.teacherKey))
        {
            result.issues.push_back(
                Issue{IssueCode::UnknownTeacherResolution,
                      resolution.teacherKey}
                );
        }
        if (teacherResolutions.contains(resolution.teacherKey))
        {
            result.issues.push_back(
                Issue{IssueCode::DuplicateTeacherResolution,
                      resolution.teacherKey}
                );
            continue;
        }
        teacherResolutions.emplace(resolution.teacherKey, &resolution);
    }

    const std::size_t candidateCount = request.candidates.size();
    std::vector<bool> classResolutionSeen(candidateCount, false);
    std::vector<const ScheduleImportReviewClassResolution*> classByCandidate(
        candidateCount,
        nullptr
        );
    std::map<Domain::ClassId, std::vector<int>> claimedClassTargets;
    std::map<Domain::ClassId, std::size_t> duplicateTargetIssueIndexes;

    const auto claimClassTarget = [&] (
        const Domain::ClassId& targetClassId,
        int candidateIndex,
        ClassAction action
        )
    {
        auto& claimants = claimedClassTargets[targetClassId];
        if (claimants.empty())
        {
            claimants.push_back(candidateIndex);
            return;
        }

        const auto foundIssue = duplicateTargetIssueIndexes.find(targetClassId);
        if (foundIssue == duplicateTargetIssueIndexes.end())
        {
            const IssueCode code = action == ClassAction::UpdateExisting
                ? IssueCode::DuplicateUpdatedClassTarget
                : IssueCode::DuplicateSkippedClassTarget;
            Issue issue{code};
            issue.candidateIndex = candidateIndex;
            issue.targetClassId = targetClassId;
            issue.candidateIndexes = claimants;
            issue.candidateIndexes.push_back(candidateIndex);
            duplicateTargetIssueIndexes.emplace(
                targetClassId,
                result.issues.size()
                );
            result.issues.push_back(std::move(issue));
        }
        else
        {
            result.issues[foundIssue->second].candidateIndexes.push_back(
                candidateIndex
                );
        }
        claimants.push_back(candidateIndex);
    };

    for (const auto& resolution : request.classes)
    {
        if (resolution.candidateIndex < 0
            || static_cast<std::size_t>(resolution.candidateIndex)
                >= candidateCount)
        {
            Issue issue{IssueCode::CandidateIndexOutOfRange};
            issue.candidateIndex = resolution.candidateIndex;
            result.issues.push_back(std::move(issue));
            continue;
        }

        const auto candidateIndex =
            static_cast<std::size_t>(resolution.candidateIndex);
        if (classResolutionSeen[candidateIndex])
        {
            Issue issue{IssueCode::DuplicateClassResolution};
            issue.candidateIndex = resolution.candidateIndex;
            result.issues.push_back(std::move(issue));
            continue;
        }
        classResolutionSeen[candidateIndex] = true;
        classByCandidate[candidateIndex] = &resolution;

        if (!validClassAction(resolution.action))
        {
            Issue issue{IssueCode::InvalidClassAction};
            issue.candidateIndex = resolution.candidateIndex;
            result.issues.push_back(std::move(issue));
            continue;
        }

        if (resolution.action == ClassAction::UpdateExisting)
        {
            if (!resolution.targetClassId)
            {
                Issue issue{IssueCode::UpdateClassMissingTarget};
                issue.candidateIndex = resolution.candidateIndex;
                result.issues.push_back(std::move(issue));
            }
            else
            {
                claimClassTarget(
                    *resolution.targetClassId,
                    resolution.candidateIndex,
                    resolution.action
                    );
            }
        }
        else if (resolution.action == ClassAction::CreateNew)
        {
            if (resolution.targetClassId)
            {
                Issue issue{IssueCode::CreateNewClassHasTarget};
                issue.candidateIndex = resolution.candidateIndex;
                issue.targetClassId = *resolution.targetClassId;
                result.issues.push_back(std::move(issue));
            }
        }
        else if (resolution.action == ClassAction::Skip
                 && resolution.targetClassId)
        {
            claimClassTarget(
                *resolution.targetClassId,
                resolution.candidateIndex,
                resolution.action
                );
        }
    }

    for (const auto& teacherKey : candidateTeacherKeys)
    {
        if (!teacherResolutions.contains(teacherKey))
        {
            result.issues.push_back(
                Issue{IssueCode::MissingTeacherResolution, teacherKey}
                );
        }
    }
    for (std::size_t candidateIndex = 0;
         candidateIndex < candidateCount;
         ++candidateIndex)
    {
        if (!classResolutionSeen[candidateIndex])
        {
            Issue issue{IssueCode::MissingClassResolution};
            issue.candidateIndex = static_cast<int>(candidateIndex);
            result.issues.push_back(std::move(issue));
        }
    }

    for (const auto& teacherKey : candidateTeacherKeys)
    {
        const auto found = teacherResolutions.find(teacherKey);
        if (found == teacherResolutions.end())
        {
            continue;
        }
        const auto& resolution = *found->second;
        if (!validTeacherAction(resolution.action)
            || resolution.action == TeacherAction::Skip)
        {
            continue;
        }

        const auto& rooms = importedRooms[teacherKey];
        const bool roomMustBeSelected =
            resolution.action == TeacherAction::Create
            || resolution.action == TeacherAction::UpdateRoom
            || (resolution.action == TeacherAction::Reuse && rooms.size() > 1);
        if (!roomMustBeSelected)
        {
            continue;
        }
        if (resolution.selectedRoom.empty())
        {
            result.issues.push_back(
                Issue{IssueCode::MissingTeacherRoom, teacherKey}
                );
        }
        else if (std::find(rooms.begin(), rooms.end(), resolution.selectedRoom)
                 == rooms.end())
        {
            result.issues.push_back(
                Issue{IssueCode::ForeignTeacherRoom, teacherKey}
                );
        }
    }

    for (std::size_t candidateIndex = 0;
         candidateIndex < candidateCount;
         ++candidateIndex)
    {
        const auto& candidate = request.candidates[candidateIndex];
        const auto teacher = teacherResolutions.find(candidate.teacherKey);
        if (teacher == teacherResolutions.end()
            || !validTeacherAction(teacher->second->action)
            || teacher->second->action != TeacherAction::Skip)
        {
            continue;
        }

        const auto* classResolution = classByCandidate[candidateIndex];
        if (classResolution
            && validClassAction(classResolution->action)
            && classResolution->action != ClassAction::Skip)
        {
            Issue issue{IssueCode::ActiveClassAssignedToSkippedTeacher,
                        candidate.teacherKey};
            issue.candidateIndex = static_cast<int>(candidateIndex);
            result.issues.push_back(std::move(issue));
        }
    }

    return result;
}

}
