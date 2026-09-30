#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/gs_team_repository.h"
#include "domain/models/gs_team_member.h"
#include "next/application/gs_team_directory_save_port.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesGsTeamDirectorySavePort final
    : public Application::GsTeamDirectorySavePort
{
public:
    explicit ApplicationServicesGsTeamDirectorySavePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] bool hasActiveSession() const noexcept
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        return session && session->isOpen() && session->gsTeamRepository();
    }

    [[nodiscard]] Domain::Result<void> saveGsTeamDirectory(
        const Application::GsTeamDirectorySaveRequest& request
        ) const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }

        GsTeamRepository* const repository = session->gsTeamRepository();
        if (!repository)
        {
            return unavailableFailure();
        }

        QList<GsTeamMember> members;
        members.reserve(static_cast<qsizetype>(request.rows.size()));
        for (const auto& row : request.rows)
        {
            GsTeamMember member;
            member.id = row.id ? row.id->value() : -1;
            member.name = QString::fromStdU16String(row.name);
            member.koreanName = QString::fromStdU16String(row.koreanName);
            member.position = QString::fromStdU16String(row.position);
            member.phoneNumber = QString::fromStdU16String(row.phoneNumber);
            member.birthday = QString::fromStdU16String(row.birthday);
            members.append(std::move(member));
        }

        QList<int> deletedIds;
        deletedIds.reserve(static_cast<qsizetype>(request.deletedIds.size()));
        for (const Domain::GsTeamMemberId id : request.deletedIds)
        {
            deletedIds.append(id.value());
        }

        try
        {
            const Status saved = repository->saveDirectory(members, deletedIds);
            if (!saved)
            {
                std::string message = utf8(saved.error());
                if (message.empty())
                {
                    message = "The GS Team directory could not be saved.";
                }
                return failure(Domain::ErrorCode::Technical, std::move(message));
            }

            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return technicalFailure(
                "The GS Team directory could not be saved.");
        }
        catch (...)
        {
            return technicalFailure(
                "The GS Team directory could not be saved.");
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
            "No active GS Team database session is available.",
            true
            );
    }

    [[nodiscard]] static Domain::Result<void> technicalFailure(
        std::string message
        )
    {
        return failure(Domain::ErrorCode::Technical, std::move(message));
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
