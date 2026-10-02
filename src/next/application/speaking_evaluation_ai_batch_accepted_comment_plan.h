#pragma once

#include "next/application/qt_compatible_text.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationAiBatchAcceptedCommentCandidate final
{
    bool checked = false;
    bool valid = false;
    int reportIndex = -1;
    int sourceRow = -1;
    std::u16string oldComment;
    std::u16string newComment;
};

struct SpeakingEvaluationAiBatchAcceptedCommentAssignment final
{
    int sourceRow = -1;
    std::u16string oldComment;
    std::u16string newComment;

    friend bool operator==(
        const SpeakingEvaluationAiBatchAcceptedCommentAssignment&,
        const SpeakingEvaluationAiBatchAcceptedCommentAssignment&
        ) = default;
};

struct SpeakingEvaluationAiBatchAcceptedCommentPlan final
{
    std::vector<SpeakingEvaluationAiBatchAcceptedCommentAssignment>
        assignments;
    std::size_t overwriteCount = 0;
};

// Candidates arrive in review-table row order. Text is already adapted and
// normalized by the UI; this policy only filters and plans assignments.
[[nodiscard]] inline SpeakingEvaluationAiBatchAcceptedCommentPlan
planSpeakingEvaluationAiBatchAcceptedComments(
    const std::vector<SpeakingEvaluationAiBatchAcceptedCommentCandidate>&
        candidates,
    const std::size_t reportCount
    )
{
    SpeakingEvaluationAiBatchAcceptedCommentPlan plan;
    for (const auto& candidate : candidates)
    {
        if (!candidate.checked || !candidate.valid)
        {
            continue;
        }
        if (
            candidate.reportIndex < 0
            || static_cast<std::size_t>(candidate.reportIndex) >= reportCount
            )
        {
            continue;
        }
        if (candidate.oldComment == candidate.newComment)
        {
            continue;
        }

        if (!trimQtWhitespace(candidate.oldComment).empty())
        {
            ++plan.overwriteCount;
        }
        plan.assignments.push_back({
            candidate.sourceRow,
            candidate.oldComment,
            candidate.newComment
        });
    }

    return plan;
}

} // namespace ClassMngr::Next::Application
