#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
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

    [[nodiscard]] Application::TeacherCreateResult createTeacher(
        const Application::TeacherCreateRequest& request
        ) const override
    {
        try
        {
            DatabaseSession* const session =
                m_services ? m_services->databaseSession() : nullptr;
            if (!session || !session->isOpen())
            {
                return unavailableFailure();
            }

            TeacherRepository* const repository = session->teacherRepository();
            if (!repository)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "The teacher repository is unavailable."
                    );
            }

            const Teacher normalized = TeacherValidator::normalized(
                toLegacyTeacher(request.fields)
                );
            const ValidationResult validation =
                TeacherValidator::validate(normalized);
            if (validation.hasErrors())
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    toStdString(validationError(validation))
                    );
            }

            const Result<int> created = repository->createTeacher(normalized);
            if (!created)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(created.error())
                    );
            }
            if (*created <= 0)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    "Creating the teacher did not return a valid ID."
                    );
            }
            const auto teacherId = Domain::TeacherId::fromString(
                std::to_string(*created)
                );
            if (!teacherId)
            {
                return failure(
                    Domain::ErrorCode::Technical,
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

    [[nodiscard]] static Application::TeacherCreateResult unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for teachers is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
