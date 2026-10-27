#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "domain/models/native_english_teacher.h"
#include "next/application/native_english_teacher_directory_read_port.h"
#include "next/application/native_english_teacher_directory_save_port.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class WorkspaceStoreNativeEnglishTeacherDirectoryPort final
    : public Application::NativeEnglishTeacherDirectoryReadPort,
      public Application::NativeEnglishTeacherDirectorySavePort
{
public:
    explicit WorkspaceStoreNativeEnglishTeacherDirectoryPort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::NativeEnglishTeacherDirectoryReadResult
    readNativeEnglishTeacherDirectory() const override
    {
        NativeEnglishTeacherRepository* const repository = activeRepository();
        if (!repository)
        {
            return Application::NativeEnglishTeacherDirectoryReadResult::failure(
                unavailableError());
        }
        try
        {
            const RepositoryResult<QList<NativeEnglishTeacher>> loaded =
                repository->getAll();
            if (!loaded)
            {
                return Application::NativeEnglishTeacherDirectoryReadResult::failure(
                    repositoryError(loaded.error()));
            }

            Application::NativeEnglishTeacherDirectorySnapshot snapshot;
            snapshot.reserve(static_cast<std::size_t>(loaded->size()));
            for (const NativeEnglishTeacher& teacher : *loaded)
            {
                if (teacher.id <= 0)
                {
                    return Application::NativeEnglishTeacherDirectoryReadResult::failure({
                        .code = Domain::ErrorCode::Validation,
                        .message = "The Native English Teacher directory contains an invalid teacher identifier.",
                        .recoverable = false
                    });
                }
                snapshot.push_back({
                    .id = Domain::NativeEnglishTeacherId(teacher.id),
                    .name = teacher.name.toStdU16String(),
                    .position = teacher.position.toStdU16String(),
                    .phoneNumber = teacher.phoneNumber.toStdU16String(),
                    .email = teacher.email.toStdU16String(),
                    .birthday = teacher.birthday.toStdU16String(),
                    .nationality = teacher.nationality.toStdU16String()
                });
            }
            return Application::NativeEnglishTeacherDirectoryReadResult::success(
                std::move(snapshot));
        }
        catch (const std::exception&)
        {
            return Application::NativeEnglishTeacherDirectoryReadResult::failure(
                technicalError("The Native English Teacher directory could not be loaded."));
        }
        catch (...)
        {
            return Application::NativeEnglishTeacherDirectoryReadResult::failure(
                technicalError("The Native English Teacher directory could not be loaded."));
        }
    }

    [[nodiscard]] Domain::Result<void> saveNativeEnglishTeacherDirectory(
        const Application::NativeEnglishTeacherDirectorySaveRequest& request
        ) const override
    {
        NativeEnglishTeacherRepository* const repository = activeRepository();
        if (!repository)
        {
            return Domain::Result<void>::failure(unavailableError());
        }
        QList<NativeEnglishTeacher> teachers;
        teachers.reserve(static_cast<qsizetype>(request.rows.size()));
        for (const auto& row : request.rows)
        {
            teachers.append({
                .id = row.id ? row.id->value() : -1,
                .name = QString::fromStdU16String(row.name),
                .position = QString::fromStdU16String(row.position),
                .phoneNumber = QString::fromStdU16String(row.phoneNumber),
                .birthday = QString::fromStdU16String(row.birthday),
                .nationality = QString::fromStdU16String(row.nationality),
                .email = QString::fromStdU16String(row.email)
            });
        }
        QList<int> deletedIds;
        deletedIds.reserve(static_cast<qsizetype>(request.deletedIds.size()));
        for (const Domain::NativeEnglishTeacherId id : request.deletedIds)
        {
            deletedIds.append(id.value());
        }
        try
        {
            const RepositoryStatus saved =
                repository->saveDirectory(teachers, deletedIds);
            if (!saved)
            {
                return Domain::Result<void>::failure(
                    repositoryError(saved.error()));
            }
            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(
                technicalError("The Native English Teacher directory could not be saved."));
        }
        catch (...)
        {
            return Domain::Result<void>::failure(
                technicalError("The Native English Teacher directory could not be saved."));
        }
    }

private:
    [[nodiscard]] NativeEnglishTeacherRepository* activeRepository() const noexcept
    {
        return m_store && m_store->isOpen()
            ? m_store->nativeEnglishTeacherRepository()
            : nullptr;
    }

    [[nodiscard]] static Domain::OperationError repositoryError(
        const RepositoryError& error
        )
    {
        const QByteArray message = error.diagnostic.toUtf8();
        return {
            .code = repositoryDomainErrorCode(error.code),
            .message = {message.constData(), static_cast<std::size_t>(message.size())},
            .recoverable = error.recoverableOverride.value_or(
                error.code == RepositoryErrorCode::NotFound
                    || error.code == RepositoryErrorCode::Conflict
                    || error.code == RepositoryErrorCode::Validation)
        };
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = "No active Native English Teacher database session is available.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError technicalError(std::string message)
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        };
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
