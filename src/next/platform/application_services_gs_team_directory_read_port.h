#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/gs_team_repository.h"
#include "domain/models/gs_team_member.h"
#include "next/application/gs_team_directory_read_port.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesGsTeamDirectoryReadPort final
    : public Application::GsTeamDirectoryReadPort
{
public:
    explicit ApplicationServicesGsTeamDirectoryReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::GsTeamDirectoryReadResult
    readGsTeamDirectory() const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Application::GsTeamDirectoryReadResult::failure(
                unavailableError());
        }

        GsTeamRepository* const repository = session->gsTeamRepository();
        if (!repository)
        {
            return Application::GsTeamDirectoryReadResult::failure(
                unavailableError());
        }

        try
        {
            const Result<QList<GsTeamMember>> loaded = repository->getAll();
            if (!loaded)
            {
                return Application::GsTeamDirectoryReadResult::failure(
                    repositoryError(loaded.error()));
            }

            Application::GsTeamDirectorySnapshot snapshot;
            snapshot.reserve(static_cast<std::size_t>(loaded->size()));
            for (const GsTeamMember& member : *loaded)
            {
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
            .message = "No active GS Team database session is available.",
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
                ? "The GS Team directory could not be loaded."
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
