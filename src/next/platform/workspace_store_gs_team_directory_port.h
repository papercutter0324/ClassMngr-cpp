#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/gs_team_repository.h"
#include "domain/models/gs_team_member.h"
#include "next/application/gs_team_directory_read_port.h"
#include "next/application/gs_team_directory_save_port.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class WorkspaceStoreGsTeamDirectoryPort final
    : public Application::GsTeamDirectoryReadPort,
      public Application::GsTeamDirectorySavePort
{
public:
    explicit WorkspaceStoreGsTeamDirectoryPort(WorkspaceStore& store) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::GsTeamDirectoryReadResult
    readGsTeamDirectory() const override
    {
        GsTeamRepository* const repository = activeRepository();
        if (!repository)
        {
            return Application::GsTeamDirectoryReadResult::failure(
                unavailableError());
        }
        try
        {
            const RepositoryResult<QList<GsTeamMember>> loaded =
                repository->getAll();
            if (!loaded)
            {
                return Application::GsTeamDirectoryReadResult::failure(
                    repositoryError(loaded.error()));
            }

            Application::GsTeamDirectorySnapshot snapshot;
            snapshot.reserve(static_cast<std::size_t>(loaded->size()));
            for (const GsTeamMember& member : *loaded)
            {
                if (member.id <= 0)
                {
                    return Application::GsTeamDirectoryReadResult::failure({
                        .code = Domain::ErrorCode::Validation,
                        .message = "The GS Team directory contains an invalid member identifier.",
                        .recoverable = false
                    });
                }
                snapshot.push_back({
                    .id = Domain::GsTeamMemberId(member.id),
                    .name = member.name.toStdU16String(),
                    .koreanName = member.koreanName.toStdU16String(),
                    .position = member.position.toStdU16String(),
                    .phoneNumber = member.phoneNumber.toStdU16String(),
                    .birthday = member.birthday.toStdU16String()
                });
            }
            return Application::GsTeamDirectoryReadResult::success(
                std::move(snapshot));
        }
        catch (const std::exception&)
        {
            return Application::GsTeamDirectoryReadResult::failure(
                technicalError("The GS Team directory could not be loaded."));
        }
        catch (...)
        {
            return Application::GsTeamDirectoryReadResult::failure(
                technicalError("The GS Team directory could not be loaded."));
        }
    }

    [[nodiscard]] Domain::Result<void> saveGsTeamDirectory(
        const Application::GsTeamDirectorySaveRequest& request
        ) const override
    {
        GsTeamRepository* const repository = activeRepository();
        if (!repository)
        {
            return Domain::Result<void>::failure(unavailableError());
        }
        QList<GsTeamMember> members;
        members.reserve(static_cast<qsizetype>(request.rows.size()));
        for (const auto& row : request.rows)
        {
            members.append({
                .id = row.id ? row.id->value() : -1,
                .name = QString::fromStdU16String(row.name),
                .koreanName = QString::fromStdU16String(row.koreanName),
                .position = QString::fromStdU16String(row.position),
                .phoneNumber = QString::fromStdU16String(row.phoneNumber),
                .birthday = QString::fromStdU16String(row.birthday)
            });
        }
        QList<int> deletedIds;
        deletedIds.reserve(static_cast<qsizetype>(request.deletedIds.size()));
        for (const Domain::GsTeamMemberId id : request.deletedIds)
        {
            deletedIds.append(id.value());
        }
        try
        {
            const RepositoryStatus saved =
                repository->saveDirectory(members, deletedIds);
            if (!saved)
            {
                return Domain::Result<void>::failure(
                    repositoryError(saved.error()));
            }
            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(
                technicalError("The GS Team directory could not be saved."));
        }
        catch (...)
        {
            return Domain::Result<void>::failure(
                technicalError("The GS Team directory could not be saved."));
        }
    }

private:
    [[nodiscard]] GsTeamRepository* activeRepository() const noexcept
    {
        return m_store && m_store->isOpen() ? m_store->gsTeamRepository() : nullptr;
    }

    [[nodiscard]] static Domain::OperationError repositoryError(
        const RepositoryError& error
        )
    {
        const QByteArray message = error.diagnostic.toUtf8();
        return {
            .code = repositoryDomainErrorCode(error.code),
            .message = {message.constData(), static_cast<std::size_t>(message.size())},
            .recoverable = error.recoverableOverride.value_or(
                error.code == RepositoryErrorCode::NotFound
                    || error.code == RepositoryErrorCode::Conflict
                    || error.code == RepositoryErrorCode::Validation)
        };
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = "No active GS Team database session is available.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError technicalError(std::string message)
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        };
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
