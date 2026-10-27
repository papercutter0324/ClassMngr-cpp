#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/application/teacher_profile_edit_use_case.h"
#include "next/application/teacher_profile_read_port.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Direct WorkspaceStore adapters for the Next teacher profile vertical
// slice. This port deliberately has no ApplicationServices dependency.
class WorkspaceStoreTeacherProfilePort final
    : public Application::TeacherProfileReadPort,
      public Application::TeacherProfileEditPersistencePort
{
public:
    explicit WorkspaceStoreTeacherProfilePort(WorkspaceStore& store) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::TeacherProfileReadResult readTeacherProfile(
        const Domain::TeacherId& id
        ) const override
    {
        const std::optional<qint64> parsedId = teacherId(id);
        if (!parsedId)
        {
            return Application::TeacherProfileReadResult::failure(
                invalidTeacherIdError()
                );
        }

        TeacherRepository* const repository = activeRepository();
        if (!repository)
        {
            return Application::TeacherProfileReadResult::failure(
                unavailableError()
                );
        }

        try
        {
            const RepositoryResult<TeacherProfileReadRecord> loaded =
                repository->lookupTeacherProfileById(*parsedId);
            if (!loaded)
            {
                return Application::TeacherProfileReadResult::failure(
                    repositoryError(loaded.error(),
                        QStringLiteral("Teacher profile could not be loaded."))
                    );
            }
            if (loaded->teacherId != *parsedId)
            {
                return Application::TeacherProfileReadResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Teacher repository returned a different teacher identifier.",
                    .recoverable = false
                });
            }

            return Application::TeacherProfileReadResult::success({
                .teacherId = id,
                .fields = profileFields(*loaded)
            });
        }
        catch (...)
        {
            return Application::TeacherProfileReadResult::failure(
                technicalError(QStringLiteral("Teacher profile could not be loaded."))
                );
        }
    }

    [[nodiscard]] Domain::Result<void> update(
        const Domain::TeacherProfile& profile
        ) const override
    {
        const std::optional<qint64> parsedId = teacherId(profile.id);
        if (!parsedId)
        {
            return Domain::Result<void>::failure(invalidTeacherIdError());
        }

        TeacherRepository* const repository = activeRepository();
        if (!repository)
        {
            return Domain::Result<void>::failure(unavailableError());
        }

        try
        {
            const RepositoryStatus result = repository->updateTeacherWithOutcome(
                legacyTeacher(profile.fields),
                *parsedId
                );
            if (!result)
            {
                return Domain::Result<void>::failure(repositoryError(
                    result.error(),
                    QStringLiteral("Teacher profile could not be saved.")
                    ));
            }
            return Domain::Result<void>::success();
        }
        catch (...)
        {
            return Domain::Result<void>::failure(
                technicalError(QStringLiteral("Teacher profile could not be saved."))
                );
        }
    }

    [[nodiscard]] Domain::Result<Domain::TeacherProfile> reload(
        const Domain::TeacherId id
        ) const override
    {
        const std::optional<qint64> parsedId = teacherId(id);
        if (!parsedId)
        {
            return Domain::Result<Domain::TeacherProfile>::failure(
                invalidTeacherIdError()
                );
        }

        TeacherRepository* const repository = activeRepository();
        if (!repository)
        {
            return Domain::Result<Domain::TeacherProfile>::failure(
                unavailableError()
                );
        }

        try
        {
            const RepositoryResult<TeacherProfileReadRecord> loaded =
                repository->lookupTeacherProfileById(
                    *parsedId,
                    QObject::tr("Loading teacher")
                    );
            if (!loaded)
            {
                return Domain::Result<Domain::TeacherProfile>::failure(
                    repositoryError(loaded.error(),
                        QStringLiteral("Teacher profile could not be reloaded."))
                    );
            }
            if (loaded->teacherId != *parsedId)
            {
                return Domain::Result<Domain::TeacherProfile>::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Reloaded teacher profile has a different identifier.",
                    .recoverable = false
                });
            }

            return Domain::Result<Domain::TeacherProfile>::success({
                .id = id,
                .fields = profileFields(*loaded)
            });
        }
        catch (...)
        {
            return Domain::Result<Domain::TeacherProfile>::failure(
                technicalError(QStringLiteral("Teacher profile could not be reloaded."))
                );
        }
    }

private:
    [[nodiscard]] static std::optional<qint64> teacherId(
        const Domain::TeacherId& id
        )
    {
        const std::optional<std::int64_t> parsed =
            Application::parseCanonicalPositiveDecimalId64(id.value());
        return parsed ? std::optional<qint64>(static_cast<qint64>(*parsed))
                      : std::nullopt;
    }

    [[nodiscard]] TeacherRepository* activeRepository() const noexcept
    {
        if (!m_store || !m_store->isOpen())
        {
            return nullptr;
        }
        return m_store->teacherRepository();
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

    [[nodiscard]] static Domain::TeacherProfileFields profileFields(
        const TeacherProfileReadRecord& profile
        )
    {
        return {
            .teacherKr = profile.teacherKr.toStdU16String(),
            .teacherEn = profile.teacherEn.toStdU16String(),
            .preferredRomanization =
                profile.preferredRomanization.toStdU16String(),
            .preferredName = profile.preferredName.toStdU16String(),
            .roomNumber = profile.roomNumber.toStdU16String(),
            .birthday = profile.birthday.toStdU16String(),
            .phoneNumber = profile.phoneNumber.toStdU16String(),
            .wifiName = profile.wifiName.toStdU16String(),
            .wifiPassword = profile.wifiPassword.toStdU16String(),
            .internetType = profile.internetType.toStdU16String(),
            .zoomId = profile.zoomId.toStdU16String(),
            .zoomPassword = profile.zoomPassword.toStdU16String(),
            .projectionType = profile.projectionType.toStdU16String(),
            .notes = profile.notes.toStdU16String()
        };
    }

    [[nodiscard]] static std::string utf8(const QString& message)
    {
        const QByteArray bytes = message.toUtf8();
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static Domain::OperationError invalidTeacherIdError()
    {
        return {
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Teacher ID must be a canonical positive integer.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = "No active Teacher Profile database session is available.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError technicalError(
        const QString& fallback
        )
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = utf8(fallback),
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError repositoryError(
        const RepositoryError& error,
        const QString& fallback
        )
    {
        const QString message = error.diagnostic.isEmpty()
            ? fallback
            : error.diagnostic;
        return {
            .code = repositoryDomainErrorCode(error.code),
            .message = utf8(message),
            .recoverable = error.recoverableOverride.value_or(
                error.code == RepositoryErrorCode::NotFound
                    || error.code == RepositoryErrorCode::Conflict
                    || error.code == RepositoryErrorCode::Validation
                )
        };
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
