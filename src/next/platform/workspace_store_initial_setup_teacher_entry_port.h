#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/initial_setup_teacher_choices_read_port.h"
#include "next/application/teacher_create.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Implements both teacher-entry ports directly against the active v2
// WorkspaceStore. This adapter deliberately does not route through
// ApplicationServices or the legacy workspace gateway.
class WorkspaceStoreInitialSetupTeacherEntryPort final
    : public Application::TeacherCreatePort,
      public Application::InitialSetupTeacherChoicesReadPort
{
public:
    explicit WorkspaceStoreInitialSetupTeacherEntryPort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    WorkspaceStoreInitialSetupTeacherEntryPort(
        const WorkspaceStoreInitialSetupTeacherEntryPort& ) = delete;
    WorkspaceStoreInitialSetupTeacherEntryPort& operator=(
        const WorkspaceStoreInitialSetupTeacherEntryPort& ) = delete;
    WorkspaceStoreInitialSetupTeacherEntryPort(
        WorkspaceStoreInitialSetupTeacherEntryPort&& ) = delete;
    WorkspaceStoreInitialSetupTeacherEntryPort& operator=(
        WorkspaceStoreInitialSetupTeacherEntryPort&& ) = delete;

    [[nodiscard]] Domain::Result<void> preflight() const override
    {
        const auto available = activeRepository();
        return available
            ? Domain::Result<void>::success()
            : Domain::Result<void>::failure(available.error());
    }

    [[nodiscard]] Application::TeacherCreateResult createTeacher(
        const Application::TeacherCreateRequest& request
        ) const override
    {
        try
        {
            // Recheck availability after validation in case the workspace
            // was closed between the use case's preflight and persistence.
            const auto available = activeRepository();
            if (!available)
            {
                return Application::TeacherCreateResult::failure(
                    available.error()
                    );
            }

            const RepositoryResult<qint64> created =
                available.value()->createTeacherWithWideId(
                    legacyTeacher(request.fields)
                    );
            if (!created)
            {
                return Application::TeacherCreateResult::failure(
                    repositoryOperationError(created.error())
                    );
            }
            if (*created <= 0)
            {
                return Application::TeacherCreateResult::failure(
                    invalidCreatedIdError()
                    );
            }

            const auto teacherId = Domain::TeacherId::fromString(
                std::to_string(*created)
                );
            if (!teacherId)
            {
                return Application::TeacherCreateResult::failure(
                    invalidCreatedIdError()
                    );
            }
            return Application::TeacherCreateResult::success(*teacherId);
        }
        catch (const std::exception&)
        {
            return Application::TeacherCreateResult::failure(technicalError());
        }
        catch (...)
        {
            return Application::TeacherCreateResult::failure(technicalError());
        }
    }

    [[nodiscard]] Application::InitialSetupTeacherChoicesResult
    readInitialSetupTeacherChoices() const override
    {
        const auto available = activeRepository();
        if (!available)
        {
            return Application::InitialSetupTeacherChoicesResult::failure(
                available.error()
                );
        }

        try
        {
            const RepositoryResult<
                QList<InitialSetupTeacherChoiceReadRecord>> loaded =
                    available.value()->loadInitialSetupTeacherChoiceRecords();
            if (!loaded)
            {
                return Application::InitialSetupTeacherChoicesResult::failure(
                    repositoryOperationError(loaded.error())
                    );
            }

            Application::InitialSetupTeacherChoicesSnapshot snapshot;
            snapshot.teachers.reserve(static_cast<std::size_t>(loaded->size()));
            for (const InitialSetupTeacherChoiceReadRecord& teacher : *loaded)
            {
                if (teacher.teacherId <= 0)
                {
                    return Application::InitialSetupTeacherChoicesResult::failure(
                        invalidReadIdError()
                        );
                }
                const auto teacherId = Domain::TeacherId::fromString(
                    std::to_string(teacher.teacherId)
                    );
                if (!teacherId)
                {
                    return Application::InitialSetupTeacherChoicesResult::failure(
                        invalidReadIdError()
                        );
                }

                snapshot.teachers.push_back({
                    .teacherId = *teacherId,
                    .teacherKr = teacher.teacherKr.toStdU16String(),
                    .teacherEn = teacher.teacherEn.toStdU16String(),
                    .preferredRomanization =
                        teacher.preferredRomanization.toStdU16String(),
                    .preferredName = teacher.preferredName.toStdU16String()
                });
            }

            return Application::InitialSetupTeacherChoicesResult::success(
                std::move(snapshot)
                );
        }
        catch (const std::exception&)
        {
            return Application::InitialSetupTeacherChoicesResult::failure(
                technicalChoicesError()
                );
        }
        catch (...)
        {
            return Application::InitialSetupTeacherChoicesResult::failure(
                technicalChoicesError()
                );
        }
    }

private:
    [[nodiscard]] Domain::Result<TeacherRepository*> activeRepository() const
    {
        if (!m_store || !m_store->isOpen())
        {
            return Domain::Result<TeacherRepository*>::failure(
                unavailableError()
                );
        }

        TeacherRepository* const repository = m_store->teacherRepository();
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

    [[nodiscard]] static Teacher legacyTeacher(
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

    [[nodiscard]] static Domain::OperationError repositoryOperationError(
        const RepositoryError& error
        )
    {
        const QByteArray message = error.diagnostic.toUtf8();
        return {
            .code = repositoryDomainErrorCode(error.code),
            .message = std::string(
                message.constData(),
                static_cast<std::size_t>(message.size())
                ),
            .recoverable = error.recoverableOverride.value_or(
                error.code == RepositoryErrorCode::NotFound
                    || error.code == RepositoryErrorCode::Conflict
                    || error.code == RepositoryErrorCode::Validation
                )
        };
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = "The active setup profile is unavailable.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError technicalError()
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = "The teacher could not be created.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError invalidCreatedIdError()
    {
        return {
            .code = Domain::ErrorCode::Validation,
            .message = "Creating the teacher did not return a valid ID.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError invalidReadIdError()
    {
        return {
            .code = Domain::ErrorCode::Validation,
            .message = "Initial setup teacher choices contain an invalid teacher identifier.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError technicalChoicesError()
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = "Teachers could not be loaded.",
            .recoverable = false
        };
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
