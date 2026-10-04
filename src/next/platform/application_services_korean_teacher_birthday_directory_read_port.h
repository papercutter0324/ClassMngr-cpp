#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/korean_teacher_birthday_directory_read_port.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesKoreanTeacherBirthdayDirectoryReadPort final
    : public Application::KoreanTeacherBirthdayDirectoryReadPort
{
public:
    explicit ApplicationServicesKoreanTeacherBirthdayDirectoryReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::KoreanTeacherBirthdayDirectoryReadResult
    readKoreanTeacherBirthdayDirectory() const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Application::KoreanTeacherBirthdayDirectoryReadResult::failure(
                unavailableError());
        }

        TeacherRepository* const repository = session->teacherRepository();
        if (!repository)
        {
            return Application::KoreanTeacherBirthdayDirectoryReadResult::failure(
                unavailableError());
        }

        try
        {
            const Result<QList<KoreanTeacherBirthdayDirectoryReadRecord>> loaded =
                repository->loadKoreanTeacherBirthdayDirectoryRecords();
            if (!loaded)
            {
                return Application::KoreanTeacherBirthdayDirectoryReadResult::failure(
                    repositoryError(loaded.error()));
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

private:
    [[nodiscard]] static std::string utf8(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = "No active Korean Teacher birthday database session is available.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError repositoryError(
        const QString& message
        )
    {
        const std::string detail = utf8(message);
        return {
            .code = Domain::ErrorCode::Technical,
            .message = detail.empty() ? "Teachers could not be loaded." : detail,
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError technicalError(
        std::string message
        )
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        };
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
