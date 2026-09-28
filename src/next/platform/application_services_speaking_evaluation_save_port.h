#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/speaking_evaluation.h"
#include "next/application/speaking_evaluation_save_use_case.h"

#include <QByteArray>
#include <QStringList>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesSpeakingEvaluationSavePort final
    : public Application::SpeakingEvaluationSavePort
{
public:
    explicit ApplicationServicesSpeakingEvaluationSavePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesSpeakingEvaluationSavePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesSpeakingEvaluationSavePort(
        const ApplicationServicesSpeakingEvaluationSavePort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationSavePort& operator=(
        const ApplicationServicesSpeakingEvaluationSavePort&
        ) = delete;
    ApplicationServicesSpeakingEvaluationSavePort(
        ApplicationServicesSpeakingEvaluationSavePort&&
        ) = delete;
    ApplicationServicesSpeakingEvaluationSavePort& operator=(
        ApplicationServicesSpeakingEvaluationSavePort&&
        ) = delete;

    [[nodiscard]] Domain::Result<void> saveEvaluation(
        const Application::SpeakingEvaluationSaveRequest& request
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
            const Status saved = service->saveEvaluation(
                *classId,
                legacyText(request.evaluationName),
                legacyRows(request.evaluation.rows),
                legacyChanges(request.evaluation.changedCells),
                request.allowQuestionableKoreanNameLengths
                );
            if (!saved)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(saved.error())
                    );
            }

            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Speaking evaluation could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Speaking evaluation could not be saved."
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

    [[nodiscard]] static SpeakingEvalRows legacyRows(
        const std::vector<std::vector<std::u16string>>& sourceRows
        )
    {
        SpeakingEvalRows rows;
        rows.reserve(static_cast<qsizetype>(sourceRows.size()));
        for (const std::vector<std::u16string>& sourceRow : sourceRows)
        {
            QStringList row;
            row.reserve(static_cast<qsizetype>(sourceRow.size()));
            for (const std::u16string& cell : sourceRow)
            {
                row.append(legacyText(cell));
            }
            rows.append(std::move(row));
        }

        return rows;
    }

    [[nodiscard]] static QList<SpeakingEvalCellChange> legacyChanges(
        const std::vector<Application::SpeakingEvaluationCellChange>& changes
        )
    {
        QList<SpeakingEvalCellChange> legacy;
        legacy.reserve(static_cast<qsizetype>(changes.size()));
        for (const Application::SpeakingEvaluationCellChange& change : changes)
        {
            legacy.append({change.row, change.column});
        }

        return legacy;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Domain::Result<void> failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Speaking evaluation could not be saved.";
        }

        return Domain::Result<void>::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static Domain::Result<void> unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The speaking evaluation service is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
