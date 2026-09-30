#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "domain/models/native_english_teacher.h"
#include "next/application/native_english_teacher_directory_read_port.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesNativeEnglishTeacherDirectoryReadPort final
    : public Application::NativeEnglishTeacherDirectoryReadPort
{
public:
    explicit ApplicationServicesNativeEnglishTeacherDirectoryReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::NativeEnglishTeacherDirectoryReadResult
    readNativeEnglishTeacherDirectory() const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Application::NativeEnglishTeacherDirectoryReadResult::failure(
                unavailableError());
        }

        NativeEnglishTeacherRepository* const repository =
            session->nativeEnglishTeacherRepository();
        if (!repository)
        {
            return Application::NativeEnglishTeacherDirectoryReadResult::failure(
                unavailableError());
        }

        try
        {
            const Result<QList<NativeEnglishTeacher>> loaded =
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
            .message = "No active Native English Teacher database session is available.",
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
            .message = detail.empty()
                ? "The Native English Teacher directory could not be loaded."
                : detail,
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
