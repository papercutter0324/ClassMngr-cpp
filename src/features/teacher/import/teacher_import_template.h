#pragma once

#include "core/result.h"
#include "domain/models/teacher_import.h"
#include "features/calendar/calendar_workbook_reader.h"

#include <QString>
#include <QStringList>

class ITeacherImportTemplate
{
public:
    virtual ~ITeacherImportTemplate() = default;

    [[nodiscard]] virtual QString id() const = 0;
    [[nodiscard]] virtual QString displayName() const = 0;
    [[nodiscard]] virtual bool recognizes(
        const CalendarImport::Workbook& workbook
        ) const = 0;
    [[nodiscard]] virtual QStringList discoveredSections(
        const CalendarImport::Workbook& workbook
        ) const = 0;
    [[nodiscard]] virtual Result<TeacherImportPreview> parse(
        const CalendarImport::Workbook& workbook
        ) const = 0;
};
