#pragma once

#include "domain/models/schedule_import.h"

#include <QString>

class ApplicationServices;
class ScheduleService;

struct ScheduleImportReviewRequest
{
    ScheduleImportUserBlock user;
    ScheduleImportKind kind = ScheduleImportKind::Normal;
    QString profileName;
    bool updateProfileName = false;
};

[[nodiscard]] ScheduleService* openScheduleImportService(
    ApplicationServices* services
    );
