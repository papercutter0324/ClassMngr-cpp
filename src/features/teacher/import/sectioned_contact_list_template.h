#pragma once

#include "features/teacher/import/teacher_import_template.h"

class SectionedContactListTemplate final : public ITeacherImportTemplate
{
public:
    [[nodiscard]] QString id() const override;
    [[nodiscard]] QString displayName() const override;
    [[nodiscard]] bool recognizes(
        const CalendarImport::Workbook& workbook
        ) const override;
    [[nodiscard]] QStringList discoveredSections(
        const CalendarImport::Workbook& workbook
        ) const override;
    [[nodiscard]] Result<TeacherImportPreview> parse(
        const CalendarImport::Workbook& workbook
        ) const override;
};
