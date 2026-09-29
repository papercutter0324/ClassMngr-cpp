#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/testing_teacher_choices_read_query.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesTestingTeacherChoicesReadPort final
    : public Application::TestingTeacherChoicesReadPort
{
public:
    explicit ApplicationServicesTestingTeacherChoicesReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesTestingTeacherChoicesReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::TestingTeacherChoicesReadResult
    readTestingTeacherChoices(
        const Application::TestingTeacherChoicesReadQuery&
        ) const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }

        TeacherRepository* const repository = session->teacherRepository();
        if (!repository)
        {
            return unavailableFailure();
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

            Application::TestingTeacherChoicesSnapshot snapshot;
            snapshot.choices.reserve(
                static_cast<std::size_t>(loaded->size())
                );
            for (const Teacher& teacher : *loaded)
            {
                if (teacher.id <= 0)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "A teacher has an invalid identifier."
                        );
                }

                const auto teacherId = Domain::TeacherId::fromString(
                    std::to_string(teacher.id)
                    );
                if (!teacherId)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "A teacher has an invalid identifier."
                        );
                }

                snapshot.choices.push_back({
                    .teacherId = *teacherId,
                    .name = teacher.teacherKr.toStdU16String(),
                    .room = teacher.roomNumber.toStdU16String()
                });
            }

            return Application::TestingTeacherChoicesReadResult::success(
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
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    [[nodiscard]] static Application::TestingTeacherChoicesReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Teachers could not be loaded.";
        }

        return Application::TestingTeacherChoicesReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::NotFound
        });
    }

    [[nodiscard]] static Application::TestingTeacherChoicesReadResult
    unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for teacher choices is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
