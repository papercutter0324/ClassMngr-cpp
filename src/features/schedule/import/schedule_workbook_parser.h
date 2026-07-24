#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"

#include <QByteArray>

Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    const QByteArray& data,
    ScheduleImportKind kind
    );

QString normalizedScheduleImportUserName(
    const QString& value
    );
