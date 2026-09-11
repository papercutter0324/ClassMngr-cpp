#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"

#include <QByteArray>

#include <functional>

using ScheduleImportCancellation = std::function<bool()>;

Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    const QByteArray& data,
    ScheduleImportKind kind,
    const ScheduleImportCancellation& isCancelled = {}
    );

QString normalizedScheduleImportUserName(
    const QString& value
    );
