#include "next/platform/application_services_speaking_evaluation_batch_read_port.h"

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/speaking_eval_repository.h"

#include <QByteArray>
#include <QStringList>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

std::optional<int> legacyClassId(const std::string& value)
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

QString legacyText(const std::u16string& value)
{
    return QString::fromUtf16(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

std::string toStdString(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return bytes.toStdString();
}

ClassMngr::Next::Application::SpeakingEvaluationReadBatchResult failure(
    const ClassMngr::Next::Domain::ErrorCode code,
    std::string message
    )
{
    if (message.empty())
    {
        message = "Speaking evaluations could not be loaded.";
    }

    return ClassMngr::Next::Application::
        SpeakingEvaluationReadBatchResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != ClassMngr::Next::Domain::ErrorCode::InvalidInput
        });
}

ClassMngr::Next::Application::SpeakingEvaluationReadBatchResult
unavailableFailure()
{
    return ClassMngr::Next::Application::
        SpeakingEvaluationReadBatchResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::NotFound,
            .message = "The active database session for speaking evaluations "
                       "is unavailable.",
            .recoverable = true
        });
}

std::vector<std::vector<std::u16string>> applicationRows(
    const SpeakingEvalRows& rows
    )
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

} // namespace

namespace ClassMngr::Next::Platform
{

ApplicationServicesSpeakingEvaluationBatchReadPort::
ApplicationServicesSpeakingEvaluationBatchReadPort(
    ApplicationServices& services
    ) noexcept
    : m_services(&services)
{
}

ApplicationServicesSpeakingEvaluationBatchReadPort::
ApplicationServicesSpeakingEvaluationBatchReadPort(
    ApplicationServices* services
    ) noexcept
    : m_services(services)
{
}

Application::SpeakingEvaluationReadBatchResult
ApplicationServicesSpeakingEvaluationBatchReadPort::readEvaluations(
    const Application::SpeakingEvaluationReadBatchQuery& query
    ) const
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

    if (query.evaluationNames.empty())
    {
        return Application::SpeakingEvaluationReadBatchResult::success({});
    }

    DatabaseSession* const session =
        m_services ? m_services->databaseSession() : nullptr;
    if (!session || !session->isOpen())
    {
        return unavailableFailure();
    }

    SpeakingEvalRepository* const repository =
        session->speakingEvalRepository();
    if (!repository)
    {
        return unavailableFailure();
    }

    try
    {
        QStringList evaluationNames;
        evaluationNames.reserve(
            static_cast<qsizetype>(query.evaluationNames.size())
            );
        for (const std::u16string& name : query.evaluationNames)
        {
            evaluationNames.append(legacyText(name));
        }

        const Result<QList<SpeakingEvalNamedRows>> loaded =
            repository->loadSpeakingEvalBatch(*classId, evaluationNames);
        if (!loaded)
        {
            return failure(
                Domain::ErrorCode::Technical,
                toStdString(loaded.error())
                );
        }

        if (loaded->size() != static_cast<qsizetype>(query.evaluationNames.size()))
        {
            return failure(
                Domain::ErrorCode::Validation,
                "The speaking evaluation repository returned an unexpected "
                "result count."
                );
        }

        Application::SpeakingEvaluationReadBatchSnapshot snapshot;
        snapshot.evaluations.reserve(query.evaluationNames.size());
        for (std::size_t index = 0; index < query.evaluationNames.size(); ++index)
        {
            const SpeakingEvalNamedRows& source =
                loaded->at(static_cast<qsizetype>(index));
            snapshot.evaluations.push_back({
                .classId = query.classId,
                .evaluationName = source.evaluationName.toStdU16String(),
                .rows = applicationRows(source.rows)
            });
        }

        return Application::SpeakingEvaluationReadBatchResult::success(
            std::move(snapshot)
            );
    }
    catch (const std::exception&)
    {
        return failure(
            Domain::ErrorCode::Technical,
            "Speaking evaluations could not be loaded."
            );
    }
    catch (...)
    {
        return failure(
            Domain::ErrorCode::Technical,
            "Speaking evaluations could not be loaded."
            );
    }
}

} // namespace ClassMngr::Next::Platform
