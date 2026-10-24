#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"

#include <QByteArray>
#include <QIODevice>

Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    const QByteArray& data,
    ScheduleImportKind kind
    );

// The device must be open, readable, and seekable for both worksheet passes.
Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    QIODevice& device,
    ScheduleImportKind kind
    );

QString normalizedScheduleImportUserName(
    const QString& value
    );
