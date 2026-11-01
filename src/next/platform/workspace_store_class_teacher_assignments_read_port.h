#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/class_teacher_assignments_read_port.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Reads the active workspace's regular-class teacher assignments without
// exposing WorkspaceStore to the application or UI layers.
class WorkspaceStoreClassTeacherAssignmentsReadPort final
    : public Application::ClassTeacherAssignmentsReadPort
{
public:
    explicit WorkspaceStoreClassTeacherAssignmentsReadPort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::ClassTeacherAssignmentsReadResult
    readClassTeacherAssignments() const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active workspace for class teacher assignments is unavailable."
                );
        }

        ClassInfoRepository* const repository = m_store->classInfoRepository();
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
                const QByteArray bytes = loaded.error().diagnostic.toUtf8();
                const Domain::ErrorCode code = repositoryDomainErrorCode(
                    loaded.error().code
                    );
                return failure(
                    code,
                    bytes.isEmpty()
                        ? "Class teacher assignments could not be loaded."
                        : std::string(bytes.constData(),
                            static_cast<std::size_t>(bytes.size()))
                    );
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
                    if (!teacherId)
                    {
                        return failure(
                            Domain::ErrorCode::Validation,
                            "Class teacher assignments contain an invalid teacher identifier."
                            );
                    }
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
    [[nodiscard]] static Application::ClassTeacherAssignmentsReadResult
    failure(const Domain::ErrorCode code, std::string message)
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

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
