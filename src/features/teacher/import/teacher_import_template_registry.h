#pragma once

#include "features/teacher/import/teacher_import_template.h"

#include <memory>
#include <vector>

class TeacherImportTemplateRegistry
{
public:
    void registerTemplate(std::unique_ptr<ITeacherImportTemplate> importTemplate);

    [[nodiscard]] QList<const ITeacherImportTemplate*> matchingTemplates(
        const CalendarImport::Workbook& workbook
        ) const;

private:
    std::vector<std::unique_ptr<ITeacherImportTemplate>> m_templates;
};

[[nodiscard]] TeacherImportTemplateRegistry
createDefaultTeacherImportTemplateRegistry();
