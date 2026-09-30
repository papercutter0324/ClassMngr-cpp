#pragma once

#include "core/application_services.h"
#include "core/enums/schedule_type.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_info.h"
#include "domain/validation/class_info_validator.h"
#include "domain/validation/validation_result.h"
#include "next/application/class_co_teacher_assignment_use_case.h"

#include <QByteArray>
#include <QStringList>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassCoTeacherAssignmentPort final
    : public Application::ClassCoTeacherAssignmentPort
{
public:
    explicit ApplicationServicesClassCoTeacherAssignmentPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesClassCoTeacherAssignmentPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesClassCoTeacherAssignmentPort(
        const ApplicationServicesClassCoTeacherAssignmentPort&
        ) = delete;
    ApplicationServicesClassCoTeacherAssignmentPort& operator=(
        const ApplicationServicesClassCoTeacherAssignmentPort&
        ) = delete;
    ApplicationServicesClassCoTeacherAssignmentPort(
        ApplicationServicesClassCoTeacherAssignmentPort&&
        ) = delete;
    ApplicationServicesClassCoTeacherAssignmentPort& operator=(
        ApplicationServicesClassCoTeacherAssignmentPort&&
        ) = delete;

    [[nodiscard]] Domain::Result<void> assignClassCoTeacher(
        const Application::ClassCoTeacherAssignmentRequest& request
        ) const override
    {
        const std::optional<int> classId = legacyPositiveId(
            request.classId.value()
            );
        const std::optional<int> teacherId = request.teacherId
            ? legacyPositiveId(request.teacherId->value())
            : std::optional<int>{-1};
        if (!classId || !teacherId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class and teacher IDs must be positive integers."
                );
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }

        ClassInfoRepository* const repository =
            session->classInfoRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class information repository is unavailable."
                );
        }

        try
        {
            const Result<ClassInfo> loaded =
                repository->loadClassInfo(*classId);
            if (!loaded)
            {
                return repositoryFailure(session, loaded.error());
            }

            ClassInfo info = *loaded;
            info.teacherId = *teacherId;

            const ClassInfo normalized = ClassInfoValidator::normalized(info);
            const ValidationResult validation =
                ClassInfoValidator::validate(normalized);
            if (validation.hasErrors())
            {
                return repositoryFailure(
                    session,
                    validationError(validation)
                    );
            }

            const Status regularConflicts = validateScheduleConflicts(
                *repository,
                normalized,
                normalized.classTimes,
                ScheduleType::Regular
                );
            if (!regularConflicts)
            {
                return repositoryFailure(session, regularConflicts.error());
            }

            const Status intensiveConflicts = validateScheduleConflicts(
                *repository,
                normalized,
                normalized.intensiveTimes,
                ScheduleType::Intensive
                );
            if (!intensiveConflicts)
            {
                return repositoryFailure(session, intensiveConflicts.error());
            }

            const Status saved = repository->saveClassInfo(normalized);
            if (!saved)
            {
                return repositoryFailure(session, saved.error());
            }

            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Co-teacher assignment could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Co-teacher assignment could not be saved."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyPositiveId(
        const std::string& value
        )
    {
        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0)
        {
            return std::nullopt;
        }

        return parsed;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
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
                detail.prepend(
                    QStringLiteral("row %1, ").arg(issue.row + 1)
                    );
            }
            details.append(detail);
        }

        return QStringLiteral("Class information validation failed: %1")
            .arg(details.join(QStringLiteral("; ")));
    }

    [[nodiscard]] static Status validateScheduleConflicts(
        ClassInfoRepository& repository,
        const ClassInfo& info,
        const QList<ClassTime>& times,
        const ScheduleType type
        )
    {
        if (times.isEmpty())
        {
            return {};
        }

        const Result<QList<ClassConflict>> conflicts =
            repository.getClassTimeConflicts(info.classId, times, type);
        if (!conflicts)
        {
            return std::unexpected(conflicts.error());
        }
        if (conflicts->isEmpty())
        {
            return {};
        }

        const ClassConflict& first = conflicts->first();
        return std::unexpected(
            QStringLiteral(
                "Class schedule conflict: %1 %2\u2013%3 conflicts with %4."
                ).arg(
                    first.day,
                    first.startTime,
                    first.endTime,
                    first.conflictingClassName
                    )
            );
    }

    [[nodiscard]] static Domain::Result<void> repositoryFailure(
        DatabaseSession* session,
        const QString& message
        )
    {
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }
        return failure(Domain::ErrorCode::Technical, toStdString(message));
    }

    [[nodiscard]] static Domain::Result<void> failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Co-teacher assignment could not be saved.";
        }

        return Domain::Result<void>::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static Domain::Result<void> unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The class service is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
