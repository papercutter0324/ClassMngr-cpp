#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "domain/models/native_english_teacher.h"
#include "next/application/native_english_teacher_directory_save_port.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesNativeEnglishTeacherDirectorySavePort final
    : public Application::NativeEnglishTeacherDirectorySavePort
{
public:
    explicit ApplicationServicesNativeEnglishTeacherDirectorySavePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] bool hasActiveSession() const noexcept
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        return session
            && session->isOpen()
            && session->nativeEnglishTeacherRepository();
    }

    [[nodiscard]] Domain::Result<void> saveNativeEnglishTeacherDirectory(
        const Application::NativeEnglishTeacherDirectorySaveRequest& request
        ) const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }

        NativeEnglishTeacherRepository* const repository =
            session->nativeEnglishTeacherRepository();
        if (!repository)
        {
            return unavailableFailure();
        }

        QList<NativeEnglishTeacher> teachers;
        teachers.reserve(static_cast<qsizetype>(request.rows.size()));
        for (const auto& row : request.rows)
        {
            NativeEnglishTeacher teacher;
            teacher.id = row.id ? row.id->value() : -1;
            teacher.name = QString::fromStdU16String(row.name);
            teacher.position = QString::fromStdU16String(row.position);
            teacher.phoneNumber = QString::fromStdU16String(row.phoneNumber);
            teacher.email = QString::fromStdU16String(row.email);
            teacher.birthday = QString::fromStdU16String(row.birthday);
            teacher.nationality = QString::fromStdU16String(row.nationality);
            teachers.append(std::move(teacher));
        }

        QList<int> deletedIds;
        deletedIds.reserve(static_cast<qsizetype>(request.deletedIds.size()));
        for (const Domain::NativeEnglishTeacherId id : request.deletedIds)
        {
            deletedIds.append(id.value());
        }

        try
        {
            const Status saved = repository->saveDirectory(teachers, deletedIds);
            if (!saved)
            {
                std::string message = utf8(saved.error());
                if (message.empty())
                {
                    message =
                        "The Native English Teacher directory could not be saved.";
                }
                return failure(
                    Domain::ErrorCode::Technical,
                    std::move(message)
                    );
            }

            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return technicalFailure(
                "The Native English Teacher directory could not be saved.");
        }
        catch (...)
        {
            return technicalFailure(
                "The Native English Teacher directory could not be saved.");
        }
    }

private:
    [[nodiscard]] static std::string utf8(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static Domain::Result<void> unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "No active Native English Teacher database session is available.",
            true
            );
    }

    [[nodiscard]] static Domain::Result<void> technicalFailure(
        std::string message
        )
    {
        return failure(
            Domain::ErrorCode::Technical,
            std::move(message)
            );
    }

    [[nodiscard]] static Domain::Result<void> failure(
        const Domain::ErrorCode code,
        std::string message,
        const bool recoverable = false
        )
    {
        return Domain::Result<void>::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = recoverable
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
