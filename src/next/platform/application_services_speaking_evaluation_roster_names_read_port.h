#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/roster_repository.h"
#include "next/application/speaking_evaluation_roster_names_read_port.h"

#include <QByteArray>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesSpeakingEvaluationRosterNamesReadPort final
    : public Application::SpeakingEvaluationRosterNamesReadPort
{
public:
    explicit ApplicationServicesSpeakingEvaluationRosterNamesReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesSpeakingEvaluationRosterNamesReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesSpeakingEvaluationRosterNamesReadPort(
        const ApplicationServicesSpeakingEvaluationRosterNamesReadPort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationRosterNamesReadPort& operator=(
        const ApplicationServicesSpeakingEvaluationRosterNamesReadPort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationRosterNamesReadPort(
        ApplicationServicesSpeakingEvaluationRosterNamesReadPort&&
        ) = delete;
    ApplicationServicesSpeakingEvaluationRosterNamesReadPort& operator=(
        ApplicationServicesSpeakingEvaluationRosterNamesReadPort&&
        ) = delete;

    [[nodiscard]] Application::SpeakingEvaluationRosterNamesReadResult
    readSpeakingEvaluationRosterNames(
        const Application::SpeakingEvaluationRosterNamesReadRequest& request
        ) const override
    {
        const std::optional<int> classId = legacyClassId(
            request.classId.value()
            );
        if (!classId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."
                );
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The roster service is unavailable."
                );
        }

        RosterRepository* const repository = session->rosterRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster repository is unavailable."
                );
        }

        try
        {
            const auto loaded =
                repository->loadSpeakingEvaluationRosterNames(*classId);
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            Application::SpeakingEvaluationRosterNamesReadSnapshot snapshot(
                request.classId
                );
            snapshot.hasEnglishColumn = loaded->hasEnglishColumn;
            snapshot.hasKoreanColumn = loaded->hasKoreanColumn;
            snapshot.rows.reserve(
                static_cast<std::size_t>(loaded->rows.size())
                );
            for (const auto& row : loaded->rows)
            {
                snapshot.rows.push_back({
                    row.englishName.toStdU16String(),
                    row.koreanName.toStdU16String()
                });
            }

            return Application::SpeakingEvaluationRosterNamesReadResult::success(
                std::move(snapshot)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster names could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster names could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyClassId(
        const std::string& value
        )
    {
        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0
            || std::to_string(parsed) != value)
        {
            return std::nullopt;
        }

        return parsed;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Application::SpeakingEvaluationRosterNamesReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Roster names could not be loaded.";
        }

        return Application::SpeakingEvaluationRosterNamesReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::InvalidInput
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
