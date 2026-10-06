#include "class_transfer_package_validator.h"

#include "domain/validation/class_info_validator.h"
#include "domain/validation/teacher_validator.h"

#include <QStringList>

namespace
{
void appendValidation(
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

QString validationMessage(const ValidationResult& validation)
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

    return QStringLiteral("Class import validation failed: %1")
        .arg(details.join(QStringLiteral("; ")));
}
}

Result<ClassTransferPackage> ClassTransferPackageValidator::normalizedAndValidated(
    const ClassTransferPackage& package
    )
{
    ClassTransferPackage normalizedPackage = package;
    ValidationResult validation;

    for (int index = 0; index < normalizedPackage.teachers.size(); ++index)
    {
        ClassTransferTeacher& teacher = normalizedPackage.teachers[index];
        teacher.teacher = TeacherValidator::normalized(teacher.teacher);
        appendValidation(
            validation,
            TeacherValidator::validate(teacher.teacher),
            QStringLiteral("teachers[%1]").arg(index)
            );
    }

    for (int index = 0; index < normalizedPackage.classes.size(); ++index)
    {
        ClassTransferClass& transferredClass = normalizedPackage.classes[index];
        transferredClass.info = ClassInfoValidator::normalized(
            transferredClass.info
            );

        // Transfer payloads omit local identity. The repository assigns the
        // destination ID when creating the imported class.
        if (transferredClass.info.classId == -1)
        {
            transferredClass.info.classId = 1;
        }
        appendValidation(
            validation,
            ClassInfoValidator::validate(transferredClass.info),
            QStringLiteral("classes[%1].info").arg(index)
            );
    }

    if (validation.hasErrors())
    {
        return std::unexpected(validationMessage(validation));
    }

    return normalizedPackage;
}
