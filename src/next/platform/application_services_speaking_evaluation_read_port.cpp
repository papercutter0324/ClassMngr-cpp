#include "next/platform/application_services_speaking_evaluation_read_port.h"

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/speaking_eval_repository.h"
#include "domain/models/speaking_evaluation.h"
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

std::string toStdString(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return bytes.toStdString();
}

ClassMngr::Next::Application::SpeakingEvaluationReadResult failure(
    const ClassMngr::Next::Domain::ErrorCode code,
    std::string message
    )
{
    if (message.empty())
    {
        message = "Speaking evaluation could not be loaded.";
    }

    return ClassMngr::Next::Application::SpeakingEvaluationReadResult::failure({
        .code = code,
        .message = std::move(message),
        .recoverable = code != ClassMngr::Next::Domain::ErrorCode::InvalidInput
    });
}

ClassMngr::Next::Application::SpeakingEvaluationReadResult
unavailableFailure()
{
    return ClassMngr::Next::Application::SpeakingEvaluationReadResult::failure({
        .code = ClassMngr::Next::Domain::ErrorCode::NotFound,
        .message = "The active database session for speaking evaluations "
                   "is unavailable.",
        .recoverable = true
    });
}

} // namespace

namespace ClassMngr::Next::Platform
{

ApplicationServicesSpeakingEvaluationReadPort::
ApplicationServicesSpeakingEvaluationReadPort(
    ApplicationServices& services
    ) noexcept
    : m_services(&services)
{
}

ApplicationServicesSpeakingEvaluationReadPort::
ApplicationServicesSpeakingEvaluationReadPort(
    ApplicationServices* services
    ) noexcept
    : m_services(services)
{
}

std::size_t
ApplicationServicesSpeakingEvaluationReadPort::successfulCompletedRows() const noexcept
{
    return m_successfulCompletedRows.load(std::memory_order_relaxed);
}

Application::SpeakingEvaluationReadResult
ApplicationServicesSpeakingEvaluationReadPort::readEvaluation(
    const Application::SpeakingEvaluationReadQuery& query
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
        Application::SpeakingEvaluationReadSnapshot snapshot{
            .classId = query.classId,
            .evaluationName = query.evaluationName
        };
        const SpeakingEvalRepository::ReadStreamResult loaded =
            repository->forEachSpeakingEvalRowDetailed(
                *classId,
                legacyText(query.evaluationName),
                [&snapshot](const QStringList& row)
                {
                    snapshot.rows.push_back(applicationRow(row));
                }
                );
        if (!loaded)
        {
            return failure(
                repositoryDomainErrorCode(loaded.error().code),
                toStdString(loaded.error().diagnostic)
                );
        }

        const std::size_t completedRows = snapshot.rows.size();
        auto result = Application::SpeakingEvaluationReadResult::success(
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

} // namespace ClassMngr::Next::Platform
