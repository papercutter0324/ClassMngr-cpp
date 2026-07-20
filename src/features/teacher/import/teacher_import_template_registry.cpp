#include "teacher_import_template_registry.h"

#include "features/teacher/import/sectioned_contact_list_template.h"

void TeacherImportTemplateRegistry::registerTemplate(
    std::unique_ptr<ITeacherImportTemplate> importTemplate
    )
{
    if (importTemplate)
    {
        m_templates.push_back(std::move(importTemplate));
    }
}

QList<const ITeacherImportTemplate*>
TeacherImportTemplateRegistry::matchingTemplates(
    const CalendarImport::Workbook& workbook
    ) const
{
    QList<const ITeacherImportTemplate*> result;
    for (const auto& importTemplate : m_templates)
    {
        if (importTemplate->recognizes(workbook))
        {
            result.append(importTemplate.get());
        }
    }
    return result;
}

TeacherImportTemplateRegistry createDefaultTeacherImportTemplateRegistry()
{
    TeacherImportTemplateRegistry registry;
    registry.registerTemplate(std::make_unique<SectionedContactListTemplate>());
    return registry;
}
