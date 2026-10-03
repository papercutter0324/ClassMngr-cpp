#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/class_teacher_assignments_read_port.h"

#include <QByteArray>

#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassTeacherAssignmentsReadPort final
    : public Application::ClassTeacherAssignmentsReadPort
{
public:
    explicit ApplicationServicesClassTeacherAssignmentsReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ClassTeacherAssignmentsReadResult
    readClassTeacherAssignments() const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for class teacher assignments is unavailable."
                );
        }

        ClassInfoRepository* const repository =
            session->classInfoRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The class information repository for teacher assignments is unavailable."
                );
        }

        try
        {
            const Result<QList<ClassTeacherAssignment>> loaded =
                repository->loadClassTeacherAssignments();
            if (!loaded)
            {
                // Repository errors are technical failures, regardless of their
                // display text, and must not look like an unavailable session.
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            Application::ClassTeacherAssignmentsReadSnapshot snapshot;
            snapshot.assignments.reserve(
                static_cast<std::size_t>(loaded->size())
                );
            for (const ClassTeacherAssignment& assignment : *loaded)
            {
                std::optional<Domain::TeacherId> teacherId;
                if (assignment.teacherId > 0)
                {
                    teacherId = Domain::TeacherId::fromString(
                        std::to_string(assignment.teacherId)
                        );
                }

                snapshot.assignments.push_back({
                    .teacherId = std::move(teacherId)
                });
            }

            return Application::ClassTeacherAssignmentsReadResult::success(
                std::move(snapshot)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class teacher assignments could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class teacher assignments could not be loaded."
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

    [[nodiscard]] static Application::ClassTeacherAssignmentsReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Class teacher assignments could not be loaded.";
        }
        return Application::ClassTeacherAssignmentsReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::NotFound
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
