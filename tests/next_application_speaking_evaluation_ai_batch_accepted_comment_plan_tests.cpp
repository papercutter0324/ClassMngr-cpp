#include "next/application/speaking_evaluation_ai_batch_accepted_comment_plan.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <vector>

using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchAcceptedCommentAssignment;
using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchAcceptedCommentCandidate;
using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchAcceptedCommentPlan;
using ClassMngr::Next::Application::SpeakingEvaluationAiBatchReportIndex;
using ClassMngr::Next::Application::SpeakingEvaluationAiBatchOverwriteCount;
using ClassMngr::Next::Application::
    planSpeakingEvaluationAiBatchAcceptedComments;

static_assert(std::is_same_v<
    decltype(SpeakingEvaluationAiBatchAcceptedCommentPlan::overwriteCount),
    SpeakingEvaluationAiBatchOverwriteCount
    >);
static_assert(!std::is_convertible_v<
    std::size_t,
    SpeakingEvaluationAiBatchOverwriteCount
    >);
static_assert(!std::is_convertible_v<
    SpeakingEvaluationAiBatchOverwriteCount,
    std::size_t
    >);
static_assert(std::is_same_v<
    decltype(SpeakingEvaluationAiBatchAcceptedCommentCandidate::reportIndex),
    SpeakingEvaluationAiBatchReportIndex
    >);
static_assert(!std::is_convertible_v<int, SpeakingEvaluationAiBatchReportIndex>);
static_assert(!std::is_convertible_v<SpeakingEvaluationAiBatchReportIndex, int>);

constexpr SpeakingEvaluationAiBatchOverwriteCount DefaultOverwriteCount{};
constexpr SpeakingEvaluationAiBatchOverwriteCount ZeroOverwriteCount{0};
constexpr SpeakingEvaluationAiBatchOverwriteCount OneOverwriteCount{1};
constexpr SpeakingEvaluationAiBatchOverwriteCount IncrementedOverwriteCount = [] {
    SpeakingEvaluationAiBatchOverwriteCount count;
    ++count;
    return count;
}();
static_assert(DefaultOverwriteCount.rawValue() == 0);
static_assert(DefaultOverwriteCount == ZeroOverwriteCount);
static_assert(OneOverwriteCount.rawValue() == 1);
static_assert(OneOverwriteCount != ZeroOverwriteCount);
static_assert(IncrementedOverwriteCount == OneOverwriteCount);

constexpr SpeakingEvaluationAiBatchReportIndex DefaultReportIndex{};
constexpr SpeakingEvaluationAiBatchReportIndex ArbitraryReportIndex{-37};
constexpr SpeakingEvaluationAiBatchReportIndex EqualArbitraryReportIndex{-37};
constexpr SpeakingEvaluationAiBatchReportIndex DifferentReportIndex{12};
static_assert(DefaultReportIndex.rawValue() == -1);
static_assert(ArbitraryReportIndex.rawValue() == -37);
static_assert(ArbitraryReportIndex == EqualArbitraryReportIndex);
static_assert(ArbitraryReportIndex != DifferentReportIndex);

namespace
{

bool filtersCandidatesAndPreservesInputOrder()
{
    const std::vector<SpeakingEvaluationAiBatchAcceptedCommentCandidate>
        candidates{
            {
                .checked = false,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{0},
                .sourceRow = 10,
                .oldComment = u"unchecked old",
                .newComment = u"unchecked new"
            },
            {
                .checked = true,
                .valid = false,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{1},
                .sourceRow = 11,
                .oldComment = u"invalid old",
                .newComment = u"invalid new"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{-1},
                .sourceRow = 12,
                .oldComment = u"negative index old",
                .newComment = u"negative index new"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{3},
                .sourceRow = 13,
                .oldComment = u"past end old",
                .newComment = u"past end new"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{2},
                .sourceRow = 22,
                .oldComment = u"same comment",
                .newComment = u"same comment"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{1},
                .sourceRow = 41,
                .oldComment = u"  existing \U0001F9ED comment\u00A0",
                .newComment = u"replacement \U0001F4DA"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{0},
                .sourceRow = 7,
                .oldComment = u"\u00A0\u2003\u3000\t",
                .newComment = u"new comment"
            }
        };

    const SpeakingEvaluationAiBatchAcceptedCommentPlan plan =
        planSpeakingEvaluationAiBatchAcceptedComments(candidates, 3);

    return plan.assignments.size() == 2
        && plan.overwriteCount == SpeakingEvaluationAiBatchOverwriteCount{1}
        && plan.assignments[0]
            == SpeakingEvaluationAiBatchAcceptedCommentAssignment{
                .sourceRow = 41,
                .oldComment = u"  existing \U0001F9ED comment\u00A0",
                .newComment = u"replacement \U0001F4DA"
            }
        && plan.assignments[1]
            == SpeakingEvaluationAiBatchAcceptedCommentAssignment{
                .sourceRow = 7,
                .oldComment = u"\u00A0\u2003\u3000\t",
                .newComment = u"new comment"
            };
}

bool countsOnlyChangedCommentsWithQtTrimmedContent()
{
    const std::vector<SpeakingEvaluationAiBatchAcceptedCommentCandidate>
        candidates{
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{0},
                .sourceRow = 0,
                .oldComment = u"\u3000Existing\u00A0",
                .newComment = u"Updated"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{1},
                .sourceRow = 1,
                .oldComment = u"\u3000\u0085\u2028\t",
                .newComment = u"First comment"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{2},
                .sourceRow = 2,
                .oldComment = u"Unchanged",
                .newComment = u"Unchanged"
            }
        };

    const SpeakingEvaluationAiBatchAcceptedCommentPlan plan =
        planSpeakingEvaluationAiBatchAcceptedComments(candidates, 3);
    return plan.assignments.size() == 2
        && plan.overwriteCount == SpeakingEvaluationAiBatchOverwriteCount{1};
}

bool preservesExactUtf16AssignmentValues()
{
    const std::u16string oldComment =
        u"\u3000\uBC29\U0001F9ED\u00A0";
    const std::u16string newComment =
        u"\uBCC0\uACBD \U0001F4DA text";
    const std::vector<SpeakingEvaluationAiBatchAcceptedCommentCandidate>
        candidates{
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{0},
                .sourceRow = 23,
                .oldComment = oldComment,
                .newComment = newComment
            }
        };

    const SpeakingEvaluationAiBatchAcceptedCommentPlan plan =
        planSpeakingEvaluationAiBatchAcceptedComments(candidates, 1);
    if (plan.assignments.size() != 1
        || plan.overwriteCount != SpeakingEvaluationAiBatchOverwriteCount{1})
    {
        return false;
    }

    const auto& assignment = plan.assignments.front();
    return assignment.sourceRow == 23
        && assignment.oldComment == oldComment
        && assignment.newComment == newComment
        && assignment.oldComment.size() == oldComment.size()
        && assignment.newComment.size() == newComment.size();
}

bool projectsTypedZeroOneAndMultipleOverwriteCounts()
{
    const auto zeroPlan = planSpeakingEvaluationAiBatchAcceptedComments({}, 0);
    if (zeroPlan.overwriteCount != SpeakingEvaluationAiBatchOverwriteCount{})
    {
        return false;
    }

    const std::vector<SpeakingEvaluationAiBatchAcceptedCommentCandidate>
        oneOverwriteCandidate{
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{0},
                .sourceRow = 4,
                .oldComment = u"Existing comment",
                .newComment = u"Replacement comment"
            }
        };
    const auto onePlan = planSpeakingEvaluationAiBatchAcceptedComments(
        oneOverwriteCandidate,
        1
        );
    if (onePlan.overwriteCount != SpeakingEvaluationAiBatchOverwriteCount{1})
    {
        return false;
    }

    const std::vector<SpeakingEvaluationAiBatchAcceptedCommentCandidate>
        multipleOverwriteCandidates{
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{0},
                .sourceRow = 5,
                .oldComment = u"Existing comment one",
                .newComment = u"Replacement comment one"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = SpeakingEvaluationAiBatchReportIndex{1},
                .sourceRow = 6,
                .oldComment = u"Existing comment two",
                .newComment = u"Replacement comment two"
            }
        };
    const auto multiplePlan = planSpeakingEvaluationAiBatchAcceptedComments(
        multipleOverwriteCandidates,
        2
        );
    return multiplePlan.overwriteCount
        == SpeakingEvaluationAiBatchOverwriteCount{2};
}

} // namespace

int main()
{
    if (!filtersCandidatesAndPreservesInputOrder())
    {
        std::fprintf(
            stderr,
            "Accepted-comment candidates were not filtered in input order.\n"
            );
        return EXIT_FAILURE;
    }
    if (!countsOnlyChangedCommentsWithQtTrimmedContent())
    {
        std::fprintf(
            stderr,
            "Overwrite counting did not follow Qt-trimmed content.\n"
            );
        return EXIT_FAILURE;
    }
    if (!preservesExactUtf16AssignmentValues())
    {
        std::fprintf(
            stderr,
            "Accepted-comment UTF-16 values were changed.\n"
            );
        return EXIT_FAILURE;
    }
    if (!projectsTypedZeroOneAndMultipleOverwriteCounts())
    {
        std::fprintf(
            stderr,
            "Typed overwrite counts were not projected correctly.\n"
            );
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
