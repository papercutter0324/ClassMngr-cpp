#pragma once

#include "domain/models/schedule_import.h"

#include <QString>

struct ScheduleImportReviewRequest
{
    ScheduleImportUserBlock user;
    ScheduleImportKind kind = ScheduleImportKind::Normal;
    QString profileName;
    bool updateProfileName = false;
};
