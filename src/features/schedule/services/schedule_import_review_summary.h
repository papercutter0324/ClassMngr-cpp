#pragma once

#include "domain/models/schedule_import.h"

struct ScheduleImportReviewSummary
{
    int teachersCreated = 0;
    int teacherRoomsUpdated = 0;
    int teachersSkipped = 0;
    int classesCreated = 0;
    int classesUpdated = 0;
    int classesSkipped = 0;
    int schedulesCleared = 0;
    int ignoredCells = 0;
};

class ScheduleImportReviewSummaryBuilder final
{
public:
    [[nodiscard]] static ScheduleImportReviewSummary build(
        const ScheduleImportPlan& plan,
        int schedulesCleared
        );
};
