#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/application/teacher_create.h"
#include "next/application/teacher_delete.h"
#include "next/application/teacher_directory_read_port.h"
#include "next/domain/teacher_display_name.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QString>

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Direct normal-workspace teacher directory and mutation adapter. The list
// read projects only the four name fields and keeps database IDs as qint64.
class WorkspaceStoreTeacherDirectoryPort final
    : public Application::TeacherDirectoryReadPort,
      public Application::TeacherCreatePort,
      public Application::TeacherDeletePort
{
public:
    explicit WorkspaceStoreTeacherDirectoryPort(WorkspaceStore& store) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::TeacherDirectoryReadResult readTeacherDirectory()
        const override
    {
        const auto available = activeRepository();
        if (!available)
        {
            return Application::TeacherDirectoryReadResult::failure(
                available.error()
                );
        }

        try
        {
            const RepositoryResult<QList<TeacherDirectoryReadRecord>> loaded =
                available.value()->loadTeacherDirectoryRecords();
            if (!loaded)
            {
                return Application::TeacherDirectoryReadResult::failure(
                    repositoryOperationError(loaded.error())
                    );
            }

            Application::TeacherDirectoryReadSnapshot snapshot;
            snapshot.teachers.reserve(static_cast<std::size_t>(loaded->size()));
            for (const TeacherDirectoryReadRecord& record : *loaded)
            {
                if (record.teacherId <= 0)
                {
                    return Application::TeacherDirectoryReadResult::failure(
                        invalidReadIdError()
                        );
                }
                const auto teacherId = Domain::TeacherId::fromString(
                    std::to_string(record.teacherId)
                    );
                if (!teacherId)
                {
                    return Application::TeacherDirectoryReadResult::failure(
                        invalidReadIdError()
                        );
                }

                const QString korean = record.teacherKr.trimmed();
                const QString english = record.teacherEn.trimmed();
                const QString romanization =
                    record.preferredRomanization.trimmed();
                const QString preferred = record.preferredName.trimmed();
                snapshot.teachers.push_back({
                    .teacherId = *teacherId,
                    .teacherKr = korean.toStdU16String(),
                    .teacherEn = english.toStdU16String(),
                    .preferredRomanization = romanization.toStdU16String(),
                    .preferredName = preferred.toStdU16String()
                });
            }

            std::stable_sort(
                snapshot.teachers.begin(),
                snapshot.teachers.end(),
                [](const Application::TeacherDirectoryEntry& left,
                   const Application::TeacherDirectoryEntry& right) {
                    const QString leftEnglish = QString::fromStdU16String(
                        left.teacherEn
                        );
                    const QString rightEnglish = QString::fromStdU16String(
                        right.teacherEn
                        );
                    const bool leftHasEnglish = !leftEnglish.isEmpty();
                    const bool rightHasEnglish = !rightEnglish.isEmpty();
                    if (leftHasEnglish != rightHasEnglish)
                    {
                        return leftHasEnglish;
                    }

                    const int englishOrder = QString::localeAwareCompare(
                        leftEnglish,
                        rightEnglish
                        );
                    if (englishOrder != 0)
                    {
                        return englishOrder < 0;
                    }

                    const QString leftKorean = QString::fromStdU16String(
                        left.teacherKr
                        );
                    const QString rightKorean = QString::fromStdU16String(
                        right.teacherKr
                        );
                    const int koreanOrder = QString::localeAwareCompare(
                        leftKorean,
                        rightKorean
                        );
                    if (koreanOrder != 0)
                    {
                        return koreanOrder < 0;
                    }

                    return Application::parseCanonicalPositiveDecimalId64(
                               left.teacherId.value()
                               )
                        < Application::parseCanonicalPositiveDecimalId64(
                               right.teacherId.value()
                               );
                }
                );

            return Application::TeacherDirectoryReadResult::success(
                std::move(snapshot)
                );
        }
        catch (const std::exception&)
        {
            return Application::TeacherDirectoryReadResult::failure(
                technicalError("The Korean teacher list could not be loaded.")
                );
        }
        catch (...)
        {
            return Application::TeacherDirectoryReadResult::failure(
                technicalError("The Korean teacher list could not be loaded.")
                );
        }
    }

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
        const auto available = activeRepository();
        if (!available)
        {
            return Application::TeacherCreateResult::failure(
                available.error()
                );
        }

        try
        {
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
            return Application::TeacherCreateResult::failure(
                technicalError("The teacher could not be created.")
                );
        }
        catch (...)
        {
            return Application::TeacherCreateResult::failure(
                technicalError("The teacher could not be created.")
                );
        }
    }

    [[nodiscard]] Application::TeacherDeleteResult deleteTeacher(
        const Application::TeacherDeleteRequest& request
        ) const override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return Application::TeacherDeleteResult::failure(validation.error());
        }

        const auto teacherId = Application::parseCanonicalPositiveDecimalId64(
            request.teacherId.value()
            );
        if (!teacherId)
        {
            return Application::TeacherDeleteResult::failure(
                invalidTeacherIdError()
                );
        }

        const auto available = activeRepository();
        if (!available)
        {
            return Application::TeacherDeleteResult::failure(
                available.error()
                );
        }

        try
        {
            const RepositoryStatus deleted =
                available.value()->deleteTeacher(static_cast<qint64>(*teacherId));
            if (!deleted)
            {
                return Application::TeacherDeleteResult::failure(
                    repositoryOperationError(deleted.error())
                    );
            }
            return Application::TeacherDeleteResult::success();
        }
        catch (const std::exception&)
        {
            return Application::TeacherDeleteResult::failure(
                technicalError("The teacher could not be deleted.")
                );
        }
        catch (...)
        {
            return Application::TeacherDeleteResult::failure(
                technicalError("The teacher could not be deleted.")
                );
        }
    }

private:
    [[nodiscard]] Domain::Result<TeacherRepository*> activeRepository() const
    {
        if (!m_store || !m_store->isOpen())
        {
            return Domain::Result<TeacherRepository*>::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The active workspace store for teachers is unavailable.",
                .recoverable = true
            });
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

    [[nodiscard]] static Domain::OperationError invalidReadIdError()
    {
        return {
            .code = Domain::ErrorCode::Validation,
            .message = "The Korean teacher list contains an invalid teacher identifier.",
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError invalidTeacherIdError()
    {
        return {
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Teacher ID must be a canonical positive integer.",
            .recoverable = true
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

    [[nodiscard]] static Domain::OperationError technicalError(
        const char* message
        )
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = message,
            .recoverable = false
        };
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
