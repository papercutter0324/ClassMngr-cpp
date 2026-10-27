#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/gs_team_repository.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/gs_team_birthday_directory_read_port.h"
#include "next/application/korean_teacher_birthday_directory_read_port.h"
#include "next/application/native_english_teacher_birthday_directory_read_port.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Narrow projection reads for the manual upcoming-birthdays action. This
// adapter borrows the one WorkspaceStore owned by the bootstrap composition.
class WorkspaceStoreUpcomingBirthdayDirectoryReadPorts final
    : public Application::KoreanTeacherBirthdayDirectoryReadPort,
      public Application::NativeEnglishTeacherBirthdayDirectoryReadPort,
      public Application::GsTeamBirthdayDirectoryReadPort
{
public:
    explicit WorkspaceStoreUpcomingBirthdayDirectoryReadPorts(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::KoreanTeacherBirthdayDirectoryReadResult
    readKoreanTeacherBirthdayDirectory() const override
    {
        TeacherRepository* const repository = m_store && m_store->isOpen()
            ? m_store->teacherRepository()
            : nullptr;
        if (!repository)
        {
            return Application::KoreanTeacherBirthdayDirectoryReadResult::failure(
                unavailable("Korean Teacher"));
        }

        try
        {
            const auto loaded =
                repository->loadKoreanTeacherBirthdayDirectoryRecords();
            if (!loaded)
            {
                return Application::KoreanTeacherBirthdayDirectoryReadResult::failure(
                    repositoryError(loaded.error(), "Teachers"));
            }

            Application::KoreanTeacherBirthdayDirectorySnapshot snapshot;
            snapshot.reserve(static_cast<std::size_t>(loaded->size()));
            for (const KoreanTeacherBirthdayDirectoryReadRecord& teacher : *loaded)
            {
                snapshot.push_back({
                    .birthday = teacher.birthday.toStdU16String(),
                    .teacherKr = teacher.teacherKr.toStdU16String(),
                    .teacherEn = teacher.teacherEn.toStdU16String(),
                    .preferredRomanization =
                        teacher.preferredRomanization.toStdU16String(),
                    .preferredName = teacher.preferredName.toStdU16String()
                });
            }
            return Application::KoreanTeacherBirthdayDirectoryReadResult::success(
                std::move(snapshot));
        }
        catch (const std::exception&)
        {
            return Application::KoreanTeacherBirthdayDirectoryReadResult::failure(
                technicalError("Teachers could not be loaded."));
        }
        catch (...)
        {
            return Application::KoreanTeacherBirthdayDirectoryReadResult::failure(
                technicalError("Teachers could not be loaded."));
        }
    }

    [[nodiscard]] Application::NativeEnglishTeacherBirthdayDirectoryReadResult
    readNativeEnglishTeacherBirthdayDirectory() const override
    {
        NativeEnglishTeacherRepository* const repository =
            m_store && m_store->isOpen()
            ? m_store->nativeEnglishTeacherRepository()
            : nullptr;
        if (!repository)
        {
            return Application::NativeEnglishTeacherBirthdayDirectoryReadResult::failure(
                unavailable("Native English Teacher"));
        }

        try
        {
            const auto loaded = repository->loadBirthdayDirectoryReadRecords();
            if (!loaded)
            {
                return Application::NativeEnglishTeacherBirthdayDirectoryReadResult::failure(
                    repositoryError(
                        loaded.error(),
                        "The Native English Teacher birthday directory"
                        ));
            }

            Application::NativeEnglishTeacherBirthdayDirectorySnapshot snapshot;
            snapshot.reserve(static_cast<std::size_t>(loaded->size()));
            for (const NativeEnglishTeacherBirthdayReadRecord& teacher : *loaded)
            {
                snapshot.push_back({
                    .name = teacher.name.toStdU16String(),
                    .position = teacher.position.toStdU16String(),
                    .birthday = teacher.birthday.toStdU16String()
                });
            }
            return Application::NativeEnglishTeacherBirthdayDirectoryReadResult::success(
                std::move(snapshot));
        }
        catch (const std::exception&)
        {
            return Application::NativeEnglishTeacherBirthdayDirectoryReadResult::failure(
                technicalError(
                    "The Native English Teacher birthday directory could not be loaded."));
        }
        catch (...)
        {
            return Application::NativeEnglishTeacherBirthdayDirectoryReadResult::failure(
                technicalError(
                    "The Native English Teacher birthday directory could not be loaded."));
        }
    }

    [[nodiscard]] Application::GsTeamBirthdayDirectoryReadResult
    readGsTeamBirthdayDirectory() const override
    {
        GsTeamRepository* const repository = m_store && m_store->isOpen()
            ? m_store->gsTeamRepository()
            : nullptr;
        if (!repository)
        {
            return Application::GsTeamBirthdayDirectoryReadResult::failure(
                unavailable("GS Team"));
        }

        try
        {
            const auto loaded = repository->loadBirthdayDirectoryReadRecords();
            if (!loaded)
            {
                return Application::GsTeamBirthdayDirectoryReadResult::failure(
                    repositoryError(
                        loaded.error(),
                        "The GS Team birthday directory"
                        ));
            }

            Application::GsTeamBirthdayDirectorySnapshot snapshot;
            snapshot.reserve(static_cast<std::size_t>(loaded->size()));
            for (const GsTeamBirthdayReadRecord& member : *loaded)
            {
                snapshot.push_back({
                    .name = member.name.toStdU16String(),
                    .koreanName = member.koreanName.toStdU16String(),
                    .position = member.position.toStdU16String(),
                    .birthday = member.birthday.toStdU16String()
                });
            }
            return Application::GsTeamBirthdayDirectoryReadResult::success(
                std::move(snapshot));
        }
        catch (const std::exception&)
        {
            return Application::GsTeamBirthdayDirectoryReadResult::failure(
                technicalError(
                    "The GS Team birthday directory could not be loaded."));
        }
        catch (...)
        {
            return Application::GsTeamBirthdayDirectoryReadResult::failure(
                technicalError(
                    "The GS Team birthday directory could not be loaded."));
        }
    }

private:
    [[nodiscard]] static std::string utf8(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static Domain::OperationError unavailable(
        const char* directory
        )
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = std::string("No active ") + directory
                + " birthday database session is available.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError technicalError(
        const char* message
        )
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = message,
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError repositoryError(
        const RepositoryError& error,
        const char* operation
        )
    {
        const std::string detail = utf8(toLegacyDiagnostic(error));
        return {
            .code = repositoryDomainErrorCode(error.code),
            .message = detail.empty() ? operation : detail,
            .recoverable = error.recoverableOverride.value_or(false)
        };
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
