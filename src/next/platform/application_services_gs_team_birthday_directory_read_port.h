#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/gs_team_repository.h"
#include "next/application/gs_team_birthday_directory_read_port.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesGsTeamBirthdayDirectoryReadPort final
    : public Application::GsTeamBirthdayDirectoryReadPort
{
public:
    explicit ApplicationServicesGsTeamBirthdayDirectoryReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::GsTeamBirthdayDirectoryReadResult
    readGsTeamBirthdayDirectory() const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Application::GsTeamBirthdayDirectoryReadResult::failure(
                unavailableError());
        }

        GsTeamRepository* const repository = session->gsTeamRepository();
        if (!repository)
        {
            return Application::GsTeamBirthdayDirectoryReadResult::failure(
                unavailableError());
        }

        try
        {
            const auto loaded = repository->loadBirthdayDirectoryReadRecords();
            if (!loaded)
            {
                return Application::GsTeamBirthdayDirectoryReadResult::failure(
                    repositoryError(loaded.error()));
            }

            Application::GsTeamBirthdayDirectorySnapshot snapshot;
            snapshot.reserve(static_cast<std::size_t>(loaded->size()));
            for (const GsTeamBirthdayReadRecord& member : *loaded)
            {
                snapshot.push_back({
                    member.name.toStdU16String(),
                    member.koreanName.toStdU16String(),
                    member.position.toStdU16String(),
                    member.birthday.toStdU16String()
                });
            }

            return Application::GsTeamBirthdayDirectoryReadResult::success(
                std::move(snapshot));
        }
        catch (const std::exception&)
        {
            return Application::GsTeamBirthdayDirectoryReadResult::failure(
                technicalError(
                    "The GS Team birthday directory could not be loaded."));
        }
        catch (...)
        {
            return Application::GsTeamBirthdayDirectoryReadResult::failure(
                technicalError(
                    "The GS Team birthday directory could not be loaded."));
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
                ? "The GS Team birthday directory could not be loaded."
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
