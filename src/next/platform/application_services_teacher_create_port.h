#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/teacher_repository.h"
#include "domain/validation/teacher_validator.h"
#include "next/application/teacher_create.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

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

class ApplicationServicesTeacherCreatePort final
    : public Application::TeacherCreatePort
{
public:
    explicit ApplicationServicesTeacherCreatePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesTeacherCreatePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesTeacherCreatePort(
        const ApplicationServicesTeacherCreatePort&
        ) = delete;
    ApplicationServicesTeacherCreatePort& operator=(
        const ApplicationServicesTeacherCreatePort&
        ) = delete;
    ApplicationServicesTeacherCreatePort(
        ApplicationServicesTeacherCreatePort&&
        ) = delete;
    ApplicationServicesTeacherCreatePort& operator=(
        ApplicationServicesTeacherCreatePort&&
        ) = delete;

    [[nodiscard]] Domain::Result<void> preflight() const override
    {
        try
        {
            const Domain::Result<TeacherRepository*> available =
                activeRepository();
            if (!available)
            {
                return Domain::Result<void>::failure(available.error());
            }
            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(technicalError());
        }
        catch (...)
        {
            return Domain::Result<void>::failure(technicalError());
        }
    }

    [[nodiscard]] Application::TeacherCreateResult createTeacher(
        const Application::TeacherCreateRequest& request
        ) const override
    {
        try
        {
            // Recheck availability after validation in case the workspace
            // store changed between preflight and persistence.
            const Domain::Result<TeacherRepository*> available =
                activeRepository();
            if (!available)
            {
                return Application::TeacherCreateResult::failure(
                    available.error());
            }

            const RepositoryResult<qint64> created =
                available.value()->createTeacherWithWideId(
                    toLegacyTeacher(request.fields));
            if (!created)
            {
                return failure(
                    domainErrorCode(created.error().code),
                    toStdString(toLegacyDiagnostic(created.error()))
                    );
            }
            if (*created <= 0)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Creating the teacher did not return a valid ID."
                    );
            }
            const auto teacherId = Domain::TeacherId::fromString(
                std::to_string(*created)
                );
            if (!teacherId)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Creating the teacher did not return a valid ID."
                    );
            }

            return Application::TeacherCreateResult::success(*teacherId);
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The teacher could not be created."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The teacher could not be created."
                );
        }
    }

private:
    [[nodiscard]] static Domain::ErrorCode domainErrorCode(
        const RepositoryErrorCode code
        )
    {
        switch (code)
        {
        case RepositoryErrorCode::InvalidInput:
            return Domain::ErrorCode::InvalidInput;
        case RepositoryErrorCode::NotFound:
            return Domain::ErrorCode::NotFound;
        case RepositoryErrorCode::Conflict:
            return Domain::ErrorCode::Conflict;
        case RepositoryErrorCode::Validation:
            return Domain::ErrorCode::Validation;
        case RepositoryErrorCode::Technical:
            return Domain::ErrorCode::Technical;
        }
        return Domain::ErrorCode::Technical;
    }

    [[nodiscard]] Domain::Result<TeacherRepository*> activeRepository() const
    {
        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return Domain::Result<TeacherRepository*>::failure(
                unavailableError());
        }

        TeacherRepository* const repository = store->teacherRepository();
        if (!repository)
        {
            return Domain::Result<TeacherRepository*>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The teacher repository is unavailable.",
                .recoverable = false
            });
        }

        return Domain::Result<TeacherRepository*>::success(repository);
    }

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

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
        };
    }

    [[nodiscard]] static Application::TeacherCreateResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "The teacher could not be created.";
        }

        return Application::TeacherCreateResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Technical
        });
    }

    [[nodiscard]] static Domain::OperationError technicalError()
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = "The teacher could not be created.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = "The active database session for teachers is unavailable.",
            .recoverable = true
        };
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
