#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/class_co_teacher_teacher_choices_read_port.h"

#include <QByteArray>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassCoTeacherTeacherChoicesReadPort final
    : public Application::ClassCoTeacherTeacherChoicesReadPort
{
public:
    explicit ApplicationServicesClassCoTeacherTeacherChoicesReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ClassCoTeacherTeacherChoicesResult
    readClassCoTeacherTeacherChoices() const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for co-teacher choices is unavailable."
                );
        }

        TeacherRepository* const repository = session->teacherRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The co-teacher repository is unavailable."
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

            Application::ClassCoTeacherTeacherChoicesSnapshot snapshot;
            snapshot.teachers.reserve(
                static_cast<std::size_t>(loaded->size())
                );
            for (const Teacher& teacher : *loaded)
            {
                if (teacher.id <= 0)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "A co-teacher has an invalid identifier."
                        );
                }

                const auto teacherId = Domain::TeacherId::fromString(
                    std::to_string(teacher.id)
                    );
                if (!teacherId)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "A co-teacher has an invalid identifier."
                        );
                }

                snapshot.teachers.push_back({
                    .teacherId = *teacherId,
                    .teacherKr = teacher.teacherKr.toStdU16String(),
                    .teacherEn = teacher.teacherEn.toStdU16String(),
                    .roomNumber = teacher.roomNumber.toStdU16String(),
                    .internetType = teacher.internetType.toStdU16String(),
                    .wifiName = teacher.wifiName.toStdU16String(),
                    .wifiPassword = teacher.wifiPassword.toStdU16String(),
                    .projectionType = teacher.projectionType.toStdU16String(),
                    .zoomId = teacher.zoomId.toStdU16String(),
                    .zoomPassword = teacher.zoomPassword.toStdU16String()
                });
            }

            return Application::ClassCoTeacherTeacherChoicesResult::success(
                std::move(snapshot)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Co-teachers could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Co-teachers could not be loaded."
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

    [[nodiscard]] static Application::ClassCoTeacherTeacherChoicesResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Co-teachers could not be loaded.";
        }
        return Application::ClassCoTeacherTeacherChoicesResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::NotFound
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
