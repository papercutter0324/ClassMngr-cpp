#pragma once

#include "core/result.h"
#include "domain/models/schedule_import.h"

#include <QByteArray>
#include <QIODevice>

class ArchiveReadPort;

Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    const QByteArray& data,
    ScheduleImportKind kind
    );

Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    const QByteArray& data,
    ArchiveReadPort& archiveReader,
    ScheduleImportKind kind
    );

// The device must be open, readable, and seekable for both worksheet passes.
Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    QIODevice& device,
    ScheduleImportKind kind
    );

Result<ScheduleImportWorkbook> parseScheduleImportWorkbook(
    QIODevice& device,
    ArchiveReadPort& archiveReader,
    ScheduleImportKind kind
    );

QString normalizedScheduleImportUserName(
    const QString& value
    );
