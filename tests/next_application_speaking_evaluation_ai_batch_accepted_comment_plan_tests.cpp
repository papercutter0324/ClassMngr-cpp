#include "next/application/speaking_evaluation_ai_batch_accepted_comment_plan.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchAcceptedCommentAssignment;
using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchAcceptedCommentCandidate;
using ClassMngr::Next::Application::
    SpeakingEvaluationAiBatchAcceptedCommentPlan;
using ClassMngr::Next::Application::
    planSpeakingEvaluationAiBatchAcceptedComments;

namespace
{

bool filtersCandidatesAndPreservesInputOrder()
{
    const std::vector<SpeakingEvaluationAiBatchAcceptedCommentCandidate>
        candidates{
            {
                .checked = false,
                .valid = true,
                .reportIndex = 0,
                .sourceRow = 10,
                .oldComment = u"unchecked old",
                .newComment = u"unchecked new"
            },
            {
                .checked = true,
                .valid = false,
                .reportIndex = 1,
                .sourceRow = 11,
                .oldComment = u"invalid old",
                .newComment = u"invalid new"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = -1,
                .sourceRow = 12,
                .oldComment = u"negative index old",
                .newComment = u"negative index new"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = 3,
                .sourceRow = 13,
                .oldComment = u"past end old",
                .newComment = u"past end new"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = 2,
                .sourceRow = 22,
                .oldComment = u"same comment",
                .newComment = u"same comment"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = 1,
                .sourceRow = 41,
                .oldComment = u"  existing \U0001F9ED comment\u00A0",
                .newComment = u"replacement \U0001F4DA"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = 0,
                .sourceRow = 7,
                .oldComment = u"\u00A0\u2003\u3000\t",
                .newComment = u"new comment"
            }
        };

    const SpeakingEvaluationAiBatchAcceptedCommentPlan plan =
        planSpeakingEvaluationAiBatchAcceptedComments(candidates, 3);

    return plan.assignments.size() == 2
        && plan.overwriteCount == 1
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
                .reportIndex = 0,
                .sourceRow = 0,
                .oldComment = u"\u3000Existing\u00A0",
                .newComment = u"Updated"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = 1,
                .sourceRow = 1,
                .oldComment = u"\u3000\u0085\u2028\t",
                .newComment = u"First comment"
            },
            {
                .checked = true,
                .valid = true,
                .reportIndex = 2,
                .sourceRow = 2,
                .oldComment = u"Unchanged",
                .newComment = u"Unchanged"
            }
        };

    const SpeakingEvaluationAiBatchAcceptedCommentPlan plan =
        planSpeakingEvaluationAiBatchAcceptedComments(candidates, 3);
    return plan.assignments.size() == 2
        && plan.overwriteCount == 1;
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
                .reportIndex = 0,
                .sourceRow = 23,
                .oldComment = oldComment,
                .newComment = newComment
            }
        };

    const SpeakingEvaluationAiBatchAcceptedCommentPlan plan =
        planSpeakingEvaluationAiBatchAcceptedComments(candidates, 1);
    if (plan.assignments.size() != 1 || plan.overwriteCount != 1)
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
    return EXIT_SUCCESS;
}
