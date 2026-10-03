#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/initial_setup_teacher_choices_read_port.h"

#include <QByteArray>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesInitialSetupTeacherChoicesReadPort final
    : public Application::InitialSetupTeacherChoicesReadPort
{
public:
    explicit ApplicationServicesInitialSetupTeacherChoicesReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::InitialSetupTeacherChoicesResult
    readInitialSetupTeacherChoices() const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for initial setup teacher choices is unavailable."
                );
        }

        TeacherRepository* const repository = session->teacherRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The initial setup teacher repository is unavailable."
                );
        }

        try
        {
            const Result<QList<Teacher>> loaded =
                repository->getAllTeachers();
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            Application::InitialSetupTeacherChoicesSnapshot snapshot;
            snapshot.teachers.reserve(
                static_cast<std::size_t>(loaded->size())
                );
            for (const Teacher& teacher : *loaded)
            {
                if (teacher.id <= 0)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Initial setup teacher choices contain an invalid teacher identifier."
                        );
                }

                const auto teacherId = Domain::TeacherId::fromString(
                    std::to_string(teacher.id)
                    );
                if (!teacherId)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Initial setup teacher choices contain an invalid teacher identifier."
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
            return failure(
                Domain::ErrorCode::Technical,
                "Teachers could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Teachers could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
        };
    }

    [[nodiscard]] static Application::InitialSetupTeacherChoicesResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Teachers could not be loaded.";
        }
        return Application::InitialSetupTeacherChoicesResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::NotFound
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
