#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/speaking_evaluation.h"
#include "next/application/speaking_evaluation_query.h"

#include <QByteArray>
#include <QStringList>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesSpeakingEvaluationReadPort final
    : public Application::SpeakingEvaluationReadPort
{
public:
    explicit ApplicationServicesSpeakingEvaluationReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesSpeakingEvaluationReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesSpeakingEvaluationReadPort(
        const ApplicationServicesSpeakingEvaluationReadPort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationReadPort& operator=(
        const ApplicationServicesSpeakingEvaluationReadPort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationReadPort(
        ApplicationServicesSpeakingEvaluationReadPort&&
        ) = delete;
    ApplicationServicesSpeakingEvaluationReadPort& operator=(
        ApplicationServicesSpeakingEvaluationReadPort&&
        ) = delete;

    [[nodiscard]] Application::SpeakingEvaluationReadResult readEvaluation(
        const Application::SpeakingEvaluationReadQuery& query
        ) const override
    {
        const std::optional<int> classId = legacyClassId(
            query.classId.value()
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
            return unavailableFailure();
        }

        SpeakingEvaluationService* const service =
            m_services->speakingEvaluationService();
        if (!service || !service->isAvailable())
        {
            return unavailableFailure();
        }

        try
        {
            const Result<SpeakingEvalRows> loaded = service->evaluation(
                *classId,
                legacyText(query.evaluationName)
                );
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            return Application::SpeakingEvaluationReadResult::success({
                .classId = query.classId,
                .evaluationName = query.evaluationName,
                .rows = applicationRows(*loaded)
            });
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Speaking evaluation could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Speaking evaluation could not be loaded."
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

    [[nodiscard]] static QString legacyText(const std::u16string& value)
    {
        return QString::fromUtf16(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    [[nodiscard]] static std::vector<std::vector<std::u16string>>
    applicationRows(const SpeakingEvalRows& rows)
    {
        std::vector<std::vector<std::u16string>> result;
        result.reserve(static_cast<std::size_t>(rows.size()));
        for (const QStringList& sourceRow : rows)
        {
            std::vector<std::u16string> row;
            row.reserve(static_cast<std::size_t>(sourceRow.size()));
            for (const QString& cell : sourceRow)
            {
                row.push_back(cell.toStdU16String());
            }
            result.push_back(std::move(row));
        }

        return result;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Application::SpeakingEvaluationReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Speaking evaluation could not be loaded.";
        }

        return Application::SpeakingEvaluationReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::InvalidInput
        });
    }

    [[nodiscard]] static Application::SpeakingEvaluationReadResult
    unavailableFailure()
    {
        return Application::SpeakingEvaluationReadResult::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "The speaking evaluation service is unavailable.",
            .recoverable = true
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
