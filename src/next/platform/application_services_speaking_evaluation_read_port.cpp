#include "next/platform/application_services_speaking_evaluation_read_port.h"

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/speaking_eval_repository.h"
#include "domain/models/speaking_evaluation.h"

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
        const Result<SpeakingEvalRows> loaded = repository->loadSpeakingEval(
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

} // namespace ClassMngr::Next::Platform
