#include "next/application/schedule_import_review_summary_projection.h"

#include <stdexcept>
#include <type_traits>

using namespace ClassMngr::Next::Application;

namespace
{
void require(const bool condition)
{
    if (!condition)
        throw std::runtime_error("Schedule import review summary assertion failed");
}

void reviewCountContractIsValid()
{
    constexpr ScheduleImportReviewCount defaultCount;
    constexpr ScheduleImportReviewCount exactCount{-17};

    static_assert(std::is_constructible_v<ScheduleImportReviewCount, int>);
    static_assert(
        std::is_same_v<decltype(defaultCount.rawValue()), int>
        );
    static_assert(!std::is_convertible_v<int, ScheduleImportReviewCount>);
    static_assert(!std::is_convertible_v<ScheduleImportReviewCount, int>);

    require(defaultCount.rawValue() == 0);
    require(exactCount.rawValue() == -17);
    require(defaultCount == ScheduleImportReviewCount{});
    require(defaultCount != exactCount);

    ScheduleImportReviewCount incremented;
    ScheduleImportReviewCount& incrementedReference = ++incremented;
    require(&incrementedReference == &incremented);
    require(incremented == ScheduleImportReviewCount{1});
}
}

int main()
{
    reviewCountContractIsValid();

    ScheduleImportReviewDecisionRequest decisions;
    decisions.teachers = {
        {
            ScheduleImportReviewTeacherKey{"create-1"},
            ScheduleImportReviewTeacherAction::Create
        },
        {
            ScheduleImportReviewTeacherKey{"create-2"},
            ScheduleImportReviewTeacherAction::Create
        },
        {
            ScheduleImportReviewTeacherKey{"update-1"},
            ScheduleImportReviewTeacherAction::UpdateRoom
        },
        {
            ScheduleImportReviewTeacherKey{"skip-1"},
            ScheduleImportReviewTeacherAction::Skip
        },
        {
            ScheduleImportReviewTeacherKey{"skip-2"},
            ScheduleImportReviewTeacherAction::Skip
        },
        {
            ScheduleImportReviewTeacherKey{"reuse"},
            ScheduleImportReviewTeacherAction::Reuse
        },
        {
            ScheduleImportReviewTeacherKey{"unselected"},
            ScheduleImportReviewTeacherAction::Unselected
        },
        {
            ScheduleImportReviewTeacherKey{"invalid"},
            ScheduleImportReviewTeacherAction::Invalid
        }
    };
    decisions.classes = {
        {ScheduleImportCandidateIndex{0}, ScheduleImportReviewClassAction::CreateNew},
        {ScheduleImportCandidateIndex{1}, ScheduleImportReviewClassAction::CreateNew},
        {ScheduleImportCandidateIndex{2}, ScheduleImportReviewClassAction::UpdateExisting},
        {ScheduleImportCandidateIndex{3}, ScheduleImportReviewClassAction::Skip},
        {ScheduleImportCandidateIndex{4}, ScheduleImportReviewClassAction::Skip},
        {ScheduleImportCandidateIndex{5}, ScheduleImportReviewClassAction::Unselected},
        {ScheduleImportCandidateIndex{6}, ScheduleImportReviewClassAction::Invalid}
    };

    const ScheduleImportReviewSummary summary =
        projectScheduleImportReviewSummary(
            decisions,
            ScheduleImportReviewCount{3},
            ScheduleImportReviewCount{9}
            );
    require(summary.teachersCreated.rawValue() == 2);
    require(summary.teacherRoomsUpdated.rawValue() == 1);
    require(summary.teachersSkipped.rawValue() == 2);
    require(summary.classesCreated.rawValue() == 2);
    require(summary.classesUpdated.rawValue() == 1);
    require(summary.classesSkipped.rawValue() == 2);
    require(summary.schedulesCleared.rawValue() == 9);
    require(summary.ignoredCells.rawValue() == 3);
}
