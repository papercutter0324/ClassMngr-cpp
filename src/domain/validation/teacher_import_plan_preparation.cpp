#include "teacher_import_plan_preparation.h"

#include "domain/validation/teacher_validator.h"

#include <QStringList>

#include <utility>

namespace
{
void appendImportedValidation(
    ValidationResult& destination,
    const ValidationResult& source,
    const QString& prefix
    )
{
    for (ValidationIssue issue : source.issues())
    {
        issue.field = issue.field.isEmpty()
            ? prefix
            : QStringLiteral("%1.%2").arg(prefix, issue.field);
        destination.add(std::move(issue));
    }
}

QString validationError(const ValidationResult& validation)
{
    QStringList details;
    for (const ValidationIssue& issue : validation.errors())
    {
        QString detail = issue.field.isEmpty()
            ? issue.code
            : QStringLiteral("%1: %2").arg(issue.field, issue.code);
        if (issue.row >= 0 && !issue.field.contains(QChar(u'[')))
        {
            detail.prepend(QStringLiteral("row %1, ").arg(issue.row + 1));
        }
        details.append(detail);
    }

    return QStringLiteral("Teacher import validation failed: %1")
        .arg(details.join(QStringLiteral("; ")));
}
}

std::expected<TeacherImportPlan, QString>
ClassMngr::Domain::prepareTeacherImportPlan(const TeacherImportPlan& plan)
{
    TeacherImportPlan normalizedPlan = plan;
    ValidationResult validation;
    for (int index = 0; index < normalizedPlan.koreanTeachers.size(); ++index)
    {
        Teacher& teacher = normalizedPlan.koreanTeachers[index];
        teacher = TeacherValidator::normalized(teacher);
        appendImportedValidation(
            validation,
            TeacherValidator::validate(teacher),
            QStringLiteral("koreanTeachers[%1]").arg(index)
            );
    }
    if (validation.hasErrors())
    {
        return std::unexpected(validationError(validation));
    }
    return normalizedPlan;
}
