#pragma once

#include "next/application/class_transfer_projection.h"

#include <charconv>
#include <optional>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

// Apply carries user choices only. Candidates are supplied independently from
// the current preview at each validation boundary.
struct ClassTransferApplyRequest final
{
    std::vector<ClassTransferReviewClassResolution> classes;
    std::vector<ClassTransferReviewTeacherResolution> teachers;
};

struct ClassTransferApplyCandidates final
{
    std::vector<ClassTransferReviewClassCandidate> classes;
    std::vector<ClassTransferReviewTeacherCandidate> teachers;
};

template <typename TypedId>
[[nodiscard]] inline std::optional<int> classTransferApplyDestinationId(const TypedId& id)
{
    const auto& value = id.value();
    int parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size()
        || parsed <= 0 || std::to_string(parsed) != value)
        return std::nullopt;
    return parsed;
}

[[nodiscard]] inline ClassTransferReviewDecisionRequest classTransferApplyReviewRequest(
    const ClassTransferApplyRequest& choices, const ClassTransferApplyCandidates& candidates)
{
    return {candidates.classes, candidates.teachers, choices.classes, choices.teachers};
}

[[nodiscard]] inline ClassTransferReviewDecisionResult validateClassTransferApplyRequest(
    const ClassTransferApplyRequest& choices, const ClassTransferApplyCandidates& candidates)
{
    using ClassAction = ClassTransferReviewClassAction;
    using TeacherAction = ClassTransferReviewTeacherAction;
    using Code = ClassTransferReviewDecisionIssueCode;
    // Legacy int-to-typed conversion rejected malformed targets before running
    // decision rules, in class-choice then teacher-choice order. Preserve that
    // precedence, including letting invalid actions reach the decision rules.
    for (const auto& choice : choices.classes)
    {
        if (choice.targetClassId && !classTransferApplyDestinationId(*choice.targetClassId)
            && (choice.action == ClassAction::Create || choice.action == ClassAction::Replace
                || choice.action == ClassAction::Skip))
        {
            return {{{choice.action == ClassAction::Replace
                ? Code::ReplaceClassMissingTarget : Code::NonReplaceClassHasTarget,
                choice.packageClassIndex, {}, choice.targetClassId}}};
        }
    }
    for (const auto& choice : choices.teachers)
    {
        if (choice.targetTeacherId && !classTransferApplyDestinationId(*choice.targetTeacherId)
            && (choice.action == TeacherAction::Create || choice.action == TeacherAction::KeepExisting
                || choice.action == TeacherAction::ReplaceExisting))
        {
            ClassTransferReviewDecisionIssue issue{
                choice.action == TeacherAction::Create ? Code::CreateTeacherHasTarget
                                                      : Code::TeacherActionMissingTarget,
                -1, choice.teacherKey};
            issue.targetTeacherId = choice.targetTeacherId;
            return {{std::move(issue)}};
        }
    }
    return validateClassTransferReviewDecisions(classTransferApplyReviewRequest(choices, candidates));
}

} // namespace ClassMngr::Next::Application
