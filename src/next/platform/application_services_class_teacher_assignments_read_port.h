#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
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
        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for class teacher assignments is unavailable."
                );
        }

        ClassInfoRepository* const repository =
            store->classInfoRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The class information repository for teacher assignments is unavailable."
                );
        }

        try
        {
            const RepositoryResult<QList<ClassTeacherAssignmentReadRecord>> loaded =
                repository->loadClassTeacherAssignmentRecords();
            if (!loaded)
            {
                const QByteArray bytes =
                    toLegacyDiagnostic(loaded.error()).toUtf8();
                return Application::ClassTeacherAssignmentsReadResult::failure({
                    .code = domainErrorCode(loaded.error().code),
                    .message = bytes.isEmpty()
                        ? "Class teacher assignments could not be loaded."
                        : bytes.toStdString(),
                    // Repository reads were always treated as nonrecoverable
                    // failures by this port, regardless of diagnostic text.
                    .recoverable = false
                });
            }

            Application::ClassTeacherAssignmentsReadSnapshot snapshot;
            snapshot.assignments.reserve(
                static_cast<std::size_t>(loaded->size())
                );
            for (const ClassTeacherAssignmentReadRecord& assignment : *loaded)
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
    [[nodiscard]] static Domain::ErrorCode domainErrorCode(
        const RepositoryErrorCode code
        ) noexcept
    {
        switch (code)
        {
        case RepositoryErrorCode::InvalidInput:
            return Domain::ErrorCode::InvalidInput;
        case RepositoryErrorCode::NotFound:
            return Domain::ErrorCode::NotFound;
        case RepositoryErrorCode::Conflict:
            return Domain::ErrorCode::Conflict;
        case RepositoryErrorCode::Validation:
            return Domain::ErrorCode::Validation;
        case RepositoryErrorCode::Technical:
            return Domain::ErrorCode::Technical;
        }
        return Domain::ErrorCode::Technical;
    }

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
