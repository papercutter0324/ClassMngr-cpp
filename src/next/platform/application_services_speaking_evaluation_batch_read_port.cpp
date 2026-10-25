#include "next/platform/application_services_speaking_evaluation_batch_read_port.h"

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/speaking_eval_repository.h"
#include "next/platform/repository_error_code.h"

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

std::vector<std::u16string> applicationRow(const QStringList& sourceRow)
{
    std::vector<std::u16string> row;
    row.reserve(static_cast<std::size_t>(sourceRow.size()));
    for (const QString& cell : sourceRow)
    {
        row.push_back(cell.toStdU16String());
    }

    return row;
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

std::size_t
ApplicationServicesSpeakingEvaluationBatchReadPort::successfulCompletedRows() const noexcept
{
    return m_successfulCompletedRows.load(std::memory_order_relaxed);
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

    WorkspaceStore* const store =
        m_services ? m_services->workspaceStore() : nullptr;
    if (!store || !store->isOpen())
    {
        return unavailableFailure();
    }

    SpeakingEvalRepository* const repository =
        store->speakingEvalRepository();
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

        Application::SpeakingEvaluationReadBatchSnapshot snapshot;
        snapshot.evaluations.reserve(query.evaluationNames.size());
        for (const std::u16string& name : query.evaluationNames)
        {
            snapshot.evaluations.push_back({
                .classId = query.classId,
                .evaluationName = name
            });
        }

        const SpeakingEvalRepository::BatchReadStreamResult loaded =
            repository->forEachSpeakingEvalBatchRowDetailed(
                *classId,
                evaluationNames,
                [&snapshot](const qsizetype index, const QStringList& row)
                {
                    snapshot.evaluations[
                        static_cast<std::size_t>(index)
                        ].rows.push_back(applicationRow(row));
                }
                );
        if (!loaded)
        {
            return failure(
                repositoryDomainErrorCode(loaded.error().code),
                toStdString(loaded.error().diagnostic)
                );
        }

        if (snapshot.evaluations.size() != query.evaluationNames.size())
        {
            return failure(
                Domain::ErrorCode::Validation,
                "The speaking evaluation repository returned an unexpected "
                "result count."
                );
        }

        std::size_t completedRows = 0;
        for (std::size_t index = 0; index < query.evaluationNames.size(); ++index)
        {
            const auto& evaluation = snapshot.evaluations[index];
            if (evaluation.classId != query.classId
                || evaluation.evaluationName != query.evaluationNames[index])
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "The speaking evaluation repository returned a different "
                    "query identity or order."
                    );
            }
            completedRows += evaluation.rows.size();
        }

        auto result = Application::SpeakingEvaluationReadBatchResult::success(
            std::move(snapshot)
            );
        if (result)
        {
            m_successfulCompletedRows.fetch_add(
                completedRows,
                std::memory_order_relaxed
                );
        }
        return result;
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
