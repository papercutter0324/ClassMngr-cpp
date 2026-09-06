#include "teacher_import_file_validator.h"

#include "features/calendar/calendar_workbook_reader.h"
#include "features/teacher/import/teacher_import_template_registry.h"

#include <QFile>

namespace
{
TeacherImportPreviewCounts previewCounts(const TeacherImportPreview& preview)
{
    TeacherImportPreviewCounts result;
    for (const KoreanTeacherImportGroup& group : preview.koreanGroups)
    {
        result.koreanTeachers += group.candidates.size();
    }
    result.nativeEnglishTeachers = preview.nativeEnglishTeachers.size();
    result.gsTeamMembers = preview.gsTeamMembers.size();
    return result;
}
}

TeacherImportFileValidation validateTeacherImportData(
    const QByteArray& data,
    const TeacherImportTemplateRegistry& registry
    )
{
    TeacherImportFileValidation validation;
    QString workbookError;
    const CalendarImport::Workbook workbook =
        CalendarImport::parseWorkbook(data, &workbookError);
    if (workbook.worksheets.isEmpty())
    {
        validation.status = TeacherImportFileStatus::Unreadable;
        validation.diagnostics.append(
            workbookError.isEmpty()
                ? QObject::tr("The selected file is not a readable XLSX workbook.")
                : workbookError
            );
        return validation;
    }

    const QList<const ITeacherImportTemplate*> matches =
        registry.matchingTemplates(workbook);
    if (matches.isEmpty())
    {
        validation.status = TeacherImportFileStatus::UnsupportedTemplate;
        validation.diagnostics.append(
            QObject::tr("The workbook does not match a supported teacher import template.")
            );
        return validation;
    }
    if (matches.size() > 1)
    {
        validation.status = TeacherImportFileStatus::AmbiguousTemplate;
        validation.diagnostics.append(
            QObject::tr("The workbook matches more than one teacher import template.")
            );
        return validation;
    }

    const ITeacherImportTemplate* importTemplate = matches.first();
    validation.templateId = importTemplate->id();
    validation.templateName = importTemplate->displayName();
    validation.discoveredSections = importTemplate->discoveredSections(workbook);
    const Result<TeacherImportPreview> preview = importTemplate->parse(workbook);
    if (!preview)
    {
        validation.status = TeacherImportFileStatus::RecognizedButInvalid;
        validation.diagnostics.append(preview.error());
        return validation;
    }

    validation.previewCounts = previewCounts(*preview);
    validation.status = TeacherImportFileStatus::Valid;
    validation.preview = *preview;
    validation.sourceDate = preview->sourceDate;
    return validation;
}

TeacherImportFileValidation validateTeacherImportFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        TeacherImportFileValidation validation;
        validation.status = TeacherImportFileStatus::Unreadable;
        validation.diagnostics.append(
            QObject::tr("The selected file could not be opened: %1").arg(file.errorString())
            );
        return validation;
    }

    const TeacherImportTemplateRegistry registry =
        createDefaultTeacherImportTemplateRegistry();
    return validateTeacherImportData(file.readAll(), registry);
}
