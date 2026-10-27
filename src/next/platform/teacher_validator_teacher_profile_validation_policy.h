#pragma once

#include "domain/validation/teacher_validator.h"
#include "next/application/teacher_profile_edit_use_case.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Keeps the Next profile contract on the established TeacherValidator rules.
class TeacherValidatorTeacherProfileValidationPolicy final
    : public Application::TeacherProfileValidationPolicy
{
public:
    [[nodiscard]] Application::TeacherProfileValidationResult validate(
        const Domain::TeacherProfile& profile
        ) const override
    {
        try
        {
            const Teacher normalized = TeacherValidator::normalized(
                legacyTeacher(profile.fields)
                );
            const ValidationResult validation =
                TeacherValidator::validate(normalized);

            std::vector<Application::TeacherProfileValidationIssue> issues;
            issues.reserve(
                static_cast<std::size_t>(validation.issues().size())
                );
            for (const ValidationIssue& issue : validation.issues())
            {
                Application::TeacherProfileValidationIssue mapped{
                    .code = Application::TeacherProfileValidationCode(
                        issue.code.toStdString()
                        ),
                    .field = Application::TeacherProfileValidationField(
                        issue.field.toStdString()
                        ),
                    .severity = issue.isWarning()
                        ? Application::TeacherProfileValidationSeverity::Warning
                        : Application::TeacherProfileValidationSeverity::Error
                };
                for (auto argument = issue.arguments.cbegin();
                     argument != issue.arguments.cend();
                     ++argument)
                {
                    const bool retained = mapped.arguments.add({
                        .key = Application::TeacherProfileValidationArgumentKey(
                            argument.key().toStdString()
                            ),
                        .value = argument.value().toString().toStdU16String()
                    });
                    Q_UNUSED(retained);
                }
                issues.push_back(std::move(mapped));
            }

            return {
                .normalizedFields = profileFields(normalized),
                .issues = std::move(issues)
            };
        }
        catch (...)
        {
            Application::TeacherProfileValidationIssue issue{
                .code = Application::TeacherProfileValidationCode(
                    "teacher.validation.unavailable"
                    ),
                .field = Application::TeacherProfileValidationField(""),
                .severity = Application::TeacherProfileValidationSeverity::Error
            };
            return {
                .normalizedFields = profile.fields,
                .issues = {std::move(issue)}
            };
        }
    }

private:
    [[nodiscard]] static Teacher legacyTeacher(
        const Domain::TeacherProfileFields& fields
        )
    {
        return {
            .teacherKr = QString::fromStdU16String(fields.teacherKr),
            .teacherEn = QString::fromStdU16String(fields.teacherEn),
            .preferredRomanization = QString::fromStdU16String(
                fields.preferredRomanization
                ),
            .preferredName = QString::fromStdU16String(fields.preferredName),
            .roomNumber = QString::fromStdU16String(fields.roomNumber),
            .birthday = QString::fromStdU16String(fields.birthday),
            .phoneNumber = QString::fromStdU16String(fields.phoneNumber),
            .wifiName = QString::fromStdU16String(fields.wifiName),
            .wifiPassword = QString::fromStdU16String(fields.wifiPassword),
            .internetType = QString::fromStdU16String(fields.internetType),
            .zoomId = QString::fromStdU16String(fields.zoomId),
            .zoomPassword = QString::fromStdU16String(fields.zoomPassword),
            .projectionType = QString::fromStdU16String(fields.projectionType),
            .notes = QString::fromStdU16String(fields.notes)
        };
    }

    [[nodiscard]] static Domain::TeacherProfileFields profileFields(
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
};

} // namespace ClassMngr::Next::Platform
