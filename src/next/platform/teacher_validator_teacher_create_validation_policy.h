#pragma once

#include "domain/validation/teacher_validator.h"
#include "next/application/teacher_create.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <exception>
#include <string>

namespace ClassMngr::Next::Platform
{

// Adapts the established teacher rules to the Next typed create contract.
// Keeping this policy independent of ApplicationServices lets WorkspaceStore
// setup and legacy callers share validation without sharing runtime services.
class TeacherValidatorTeacherCreateValidationPolicy final
    : public Application::TeacherCreateValidationPolicy
{
public:
    [[nodiscard]] Application::TeacherCreateValidationResult validate(
        const Domain::TeacherProfileFields& fields
        ) const override
    {
        try
        {
            const Teacher normalized = TeacherValidator::normalized(
                toLegacyTeacher(fields)
                );
            const ValidationResult validation =
                TeacherValidator::validate(normalized);
            if (validation.hasErrors())
            {
                return Application::TeacherCreateValidationResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = toStdString(validationError(validation)),
                    .recoverable = true
                });
            }

            return Application::TeacherCreateValidationResult::success(
                fromLegacyTeacher(normalized)
                );
        }
        catch (const std::exception&)
        {
            return Application::TeacherCreateValidationResult::failure(
                validationTechnicalError());
        }
        catch (...)
        {
            return Application::TeacherCreateValidationResult::failure(
                validationTechnicalError());
        }
    }

private:
    [[nodiscard]] static Teacher toLegacyTeacher(
        const Domain::TeacherProfileFields& fields
        )
    {
        Teacher teacher;
        teacher.teacherKr = QString::fromStdU16String(fields.teacherKr);
        teacher.teacherEn = QString::fromStdU16String(fields.teacherEn);
        teacher.preferredRomanization = QString::fromStdU16String(
            fields.preferredRomanization
            );
        teacher.preferredName = QString::fromStdU16String(fields.preferredName);
        teacher.roomNumber = QString::fromStdU16String(fields.roomNumber);
        teacher.birthday = QString::fromStdU16String(fields.birthday);
        teacher.phoneNumber = QString::fromStdU16String(fields.phoneNumber);
        teacher.wifiName = QString::fromStdU16String(fields.wifiName);
        teacher.wifiPassword = QString::fromStdU16String(fields.wifiPassword);
        teacher.internetType = QString::fromStdU16String(fields.internetType);
        teacher.zoomId = QString::fromStdU16String(fields.zoomId);
        teacher.zoomPassword = QString::fromStdU16String(fields.zoomPassword);
        teacher.projectionType = QString::fromStdU16String(fields.projectionType);
        teacher.notes = QString::fromStdU16String(fields.notes);
        return teacher;
    }

    [[nodiscard]] static Domain::TeacherProfileFields fromLegacyTeacher(
        const Teacher& teacher
        )
    {
        return {
            .teacherKr = teacher.teacherKr.toStdU16String(),
            .teacherEn = teacher.teacherEn.toStdU16String(),
            .preferredRomanization =
                teacher.preferredRomanization.toStdU16String(),
            .preferredName = teacher.preferredName.toStdU16String(),
            .roomNumber = teacher.roomNumber.toStdU16String(),
            .birthday = teacher.birthday.toStdU16String(),
            .phoneNumber = teacher.phoneNumber.toStdU16String(),
            .wifiName = teacher.wifiName.toStdU16String(),
            .wifiPassword = teacher.wifiPassword.toStdU16String(),
            .internetType = teacher.internetType.toStdU16String(),
            .zoomId = teacher.zoomId.toStdU16String(),
            .zoomPassword = teacher.zoomPassword.toStdU16String(),
            .projectionType = teacher.projectionType.toStdU16String(),
            .notes = teacher.notes.toStdU16String()
        };
    }

    [[nodiscard]] static QString validationError(
        const ValidationResult& validation
        )
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

        return QStringLiteral("Teacher validation failed: %1")
            .arg(details.join(QStringLiteral("; ")));
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
        };
    }

    [[nodiscard]] static Domain::OperationError validationTechnicalError()
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = "The teacher could not be created.",
            .recoverable = false
        };
    }
};

} // namespace ClassMngr::Next::Platform
