#pragma once

#include "next/application/qt_compatible_text.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

class SpeakingEvaluationAiBatchReportIndex final
{
public:
    constexpr SpeakingEvaluationAiBatchReportIndex() noexcept = default;

    explicit constexpr SpeakingEvaluationAiBatchReportIndex(
        const int value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(
        const SpeakingEvaluationAiBatchReportIndex&,
        const SpeakingEvaluationAiBatchReportIndex&
        ) noexcept = default;

private:
    int m_value = -1;
};

struct SpeakingEvaluationAiBatchAcceptedCommentCandidate final
{
    bool checked = false;
    bool valid = false;
    SpeakingEvaluationAiBatchReportIndex reportIndex;
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

class SpeakingEvaluationAiBatchOverwriteCount final
{
public:
    constexpr SpeakingEvaluationAiBatchOverwriteCount() noexcept = default;

    explicit constexpr SpeakingEvaluationAiBatchOverwriteCount(
        const std::size_t value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::size_t rawValue() const noexcept
    {
        return m_value;
    }

    constexpr SpeakingEvaluationAiBatchOverwriteCount& operator++() noexcept
    {
        ++m_value;
        return *this;
    }

    friend constexpr bool operator==(
        const SpeakingEvaluationAiBatchOverwriteCount&,
        const SpeakingEvaluationAiBatchOverwriteCount&
        ) noexcept = default;

private:
    std::size_t m_value = 0;
};

struct SpeakingEvaluationAiBatchAcceptedCommentPlan final
{
    std::vector<SpeakingEvaluationAiBatchAcceptedCommentAssignment>
        assignments;
    SpeakingEvaluationAiBatchOverwriteCount overwriteCount;
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
            candidate.reportIndex.rawValue() < 0
            || static_cast<std::size_t>(candidate.reportIndex.rawValue())
                >= reportCount
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
