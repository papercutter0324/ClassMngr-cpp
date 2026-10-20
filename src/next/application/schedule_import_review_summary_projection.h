#pragma once

#include "next/application/schedule_import_review_decisions.h"

namespace ClassMngr::Next::Application
{

class ScheduleImportReviewCount final
{
public:
    constexpr ScheduleImportReviewCount() noexcept = default;

    explicit constexpr ScheduleImportReviewCount(const int value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr int rawValue() const noexcept
    {
        return m_value;
    }

    constexpr ScheduleImportReviewCount& operator++() noexcept
    {
        ++m_value;
        return *this;
    }

    friend constexpr bool operator==(
        const ScheduleImportReviewCount&,
        const ScheduleImportReviewCount&
        ) noexcept = default;

private:
    int m_value = 0;
};

struct ScheduleImportReviewSummary final
{
    ScheduleImportReviewCount teachersCreated;
    ScheduleImportReviewCount teacherRoomsUpdated;
    ScheduleImportReviewCount teachersSkipped;
    ScheduleImportReviewCount classesCreated;
    ScheduleImportReviewCount classesUpdated;
    ScheduleImportReviewCount classesSkipped;
    ScheduleImportReviewCount schedulesCleared;
    ScheduleImportReviewCount ignoredCells;
};

[[nodiscard]] inline ScheduleImportReviewSummary
projectScheduleImportReviewSummary(
    const ScheduleImportReviewDecisionRequest& decisions,
    const ScheduleImportReviewCount ignoredDiagnosticCount,
    const ScheduleImportReviewCount schedulesCleared
    )
{
    ScheduleImportReviewSummary summary;
    summary.schedulesCleared = schedulesCleared;
    summary.ignoredCells = ignoredDiagnosticCount;

    for (const ScheduleImportReviewTeacherResolution& teacher : decisions.teachers)
    {
        switch (teacher.action)
        {
        case ScheduleImportReviewTeacherAction::Create:
            ++summary.teachersCreated;
            break;
        case ScheduleImportReviewTeacherAction::UpdateRoom:
            ++summary.teacherRoomsUpdated;
            break;
        case ScheduleImportReviewTeacherAction::Skip:
            ++summary.teachersSkipped;
            break;
        case ScheduleImportReviewTeacherAction::Unselected:
        case ScheduleImportReviewTeacherAction::Reuse:
        case ScheduleImportReviewTeacherAction::Invalid:
            break;
        }
    }

    for (const ScheduleImportReviewClassResolution& classroom : decisions.classes)
    {
        switch (classroom.action)
        {
        case ScheduleImportReviewClassAction::CreateNew:
            ++summary.classesCreated;
            break;
        case ScheduleImportReviewClassAction::UpdateExisting:
            ++summary.classesUpdated;
            break;
        case ScheduleImportReviewClassAction::Skip:
            ++summary.classesSkipped;
            break;
        case ScheduleImportReviewClassAction::Unselected:
        case ScheduleImportReviewClassAction::Invalid:
            break;
        }
    }

    return summary;
}

} // namespace ClassMngr::Next::Application
