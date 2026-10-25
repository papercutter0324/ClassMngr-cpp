#pragma once

#include "core/application_services.h"
#include "core/enums/schedule_type.h"
#include "data/database/workspace_store.h"
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
        const std::optional<qint64> teacherId = request.teacherId
            ? legacyPositiveTeacherId(request.teacherId->value())
            : std::optional<qint64>{-1};
        if (!classId || !teacherId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class and teacher IDs must be positive integers."
                );
        }

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return unavailableFailure();
        }

        ClassInfoRepository* const repository =
            store->classInfoRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class information repository is unavailable."
                );
        }

        try
        {
            const RepositoryResult<ClassCoTeacherAssignmentValidationSnapshotReadRecord>
                loaded = repository
                    ->loadClassCoTeacherAssignmentValidationSnapshotDetailed(
                        *classId
                        );
            if (!loaded)
            {
                return repositoryFailure(store, loaded.error());
            }

            ClassInfo info;
            info.classId = loaded->classId;
            // ClassInfo retains its general int-based teacher contract. The
            // selected Co-Teacher ID is validated above and written at full
            // width below, while the validation model uses the equivalent
            // assigned/unassigned state.
            info.teacherId = *teacherId > 0 ? 1 : -1;
            info.classGrade = loaded->classGrade;
            info.classLevel = loaded->classLevel;
            info.readingBook = loaded->readingBook;
            info.essayBook = loaded->essayBook;
            info.classColor = loaded->classColor;
            info.fontColor = loaded->fontColor;
            info.notes = loaded->notes;
            info.timeFillerActivities = loaded->timeFillerActivities;
            info.classTimes = loaded->regularTimes;
            info.intensiveTimes = loaded->intensiveTimes;

            const ClassInfo normalized = ClassInfoValidator::normalized(info);
            const ValidationResult validation =
                ClassInfoValidator::validate(normalized);
            if (validation.hasErrors())
            {
                return repositoryFailure(
                    store,
                    validationError(validation)
                    );
            }

            const RepositoryStatus regularConflicts = validateScheduleConflicts(
                *repository,
                normalized,
                normalized.classTimes,
                ScheduleType::Regular
                );
            if (!regularConflicts)
            {
                return repositoryFailure(store, regularConflicts.error());
            }

            const RepositoryStatus intensiveConflicts = validateScheduleConflicts(
                *repository,
                normalized,
                normalized.intensiveTimes,
                ScheduleType::Intensive
                );
            if (!intensiveConflicts)
            {
                return repositoryFailure(store, intensiveConflicts.error());
            }

            const auto saved =
                repository->saveClassCoTeacherAssignmentDetailed(
                normalized.classId,
                *teacherId > 0
                    ? std::optional<qint64>{*teacherId}
                    : std::nullopt
                );
            if (!saved)
            {
                if (store->isOpen()
                    && saved.error().code == RepositoryErrorCode::NotFound)
                {
                    return Domain::Result<void>::failure({
                        .code = domainErrorCode(saved.error().code),
                        .message = toStdString(
                            toLegacyDiagnostic(saved.error())
                            ),
                        .recoverable = saved.error().recoverableOverride
                            .value_or(false)
                    });
                }

                return repositoryFailure(store, saved.error());
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

    [[nodiscard]] static std::optional<qint64> legacyPositiveTeacherId(
        const std::string& value
        )
    {
        qint64 parsed = 0;
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

    [[nodiscard]] static RepositoryStatus validateScheduleConflicts(
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

        const RepositoryResult<QList<ClassConflict>> conflicts =
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
        return std::unexpected(RepositoryError(
            RepositoryErrorCode::Technical,
            QStringLiteral(
                "Class schedule conflict: %1 %2\u2013%3 conflicts with %4."
                ).arg(
                    first.day,
                    first.startTime,
                    first.endTime,
                    first.conflictingClassName
                )
            ));
    }

    [[nodiscard]] static Domain::Result<void> repositoryFailure(
        WorkspaceStore* store,
        const QString& message
        )
    {
        if (!store || !store->isOpen())
        {
            return unavailableFailure();
        }
        return failure(Domain::ErrorCode::Technical, toStdString(message));
    }

    [[nodiscard]] static Domain::Result<void> repositoryFailure(
        WorkspaceStore* store,
        const RepositoryError& error
        )
    {
        if (!store || !store->isOpen())
        {
            return unavailableFailure();
        }
        return failure(
            domainErrorCode(error.code),
            toStdString(toLegacyDiagnostic(error))
            );
    }

    [[nodiscard]] static Domain::ErrorCode domainErrorCode(
        const RepositoryErrorCode code
        ) noexcept
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
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "The class service is unavailable.",
            .recoverable = true
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
