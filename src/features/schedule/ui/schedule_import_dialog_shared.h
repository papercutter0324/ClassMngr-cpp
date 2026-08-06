#pragma once

#include "domain/models/schedule_import.h"

#include <QString>

class ApplicationServices;
class DataService;

struct ScheduleImportReviewRequest
{
    ScheduleImportUserBlock user;
    ScheduleImportKind kind = ScheduleImportKind::Normal;
    QString profileName;
    bool updateProfileName = false;
};

[[nodiscard]] DataService* openScheduleImportDataService(
    ApplicationServices* services
    );
