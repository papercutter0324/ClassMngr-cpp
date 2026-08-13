#pragma once

#include "domain/models/schedule_import.h"

struct ScheduleImportReviewContext
{
    ScheduleImportKind kind = ScheduleImportKind::Normal;
    ScheduleImportIntensiveMode intensiveMode =
        ScheduleImportIntensiveMode::UpdateExisting;
    QString selectedUserName;
    bool saveProfileNameIfBlank = false;
    bool updateProfileName = false;
    bool unknownCellsAcknowledged = false;
    QList<ScheduleImportClassCandidate> candidates;
    QList<IntensiveSlotState> intensiveSlotStates;
    QList<ScheduleImportDiagnostic> diagnostics;
};

class ScheduleImportReviewModel final
{
public:
    [[nodiscard]] static ScheduleImportPlan buildPlan(
        const ScheduleImportReviewContext& context,
        const QList<ScheduleImportTeacherResolution>& teachers,
        const QList<ScheduleImportClassResolution>& classes
        );
};
