#include "next/platform/application_services_speaking_evaluation_save_port.h"

#include "data/database/database_session.h"
#include "data/repositories/speaking_eval_repository.h"
#include "domain/models/speaking_evaluation.h"

#include <QByteArray>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

namespace Domain = ClassMngr::Next::Domain;

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

SpeakingEvalRows legacyRows(
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

QList<SpeakingEvalCellChange> legacyChanges(
    const std::vector<
        ClassMngr::Next::Application::SpeakingEvaluationCellChange
        >& changes
    )
{
    QList<SpeakingEvalCellChange> legacy;
    legacy.reserve(static_cast<qsizetype>(changes.size()));
    for (const auto& change : changes)
    {
        legacy.append({change.row, change.column});
    }

    return legacy;
}

std::string toStdString(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return bytes.toStdString();
}

Domain::Result<void> failure(
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

Domain::Result<void> unavailableFailure()
{
    return failure(
        Domain::ErrorCode::NotFound,
        "The speaking evaluation service is unavailable."
        );
}

} // namespace

namespace ClassMngr::Next::Platform
{

Domain::Result<void> ApplicationServicesSpeakingEvaluationSavePort::
saveEvaluation(
    const Application::SpeakingEvaluationSaveRequest& request
    ) const
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

    if (!m_session || !m_session->isOpen())
    {
        return unavailableFailure();
    }

    try
    {
        SpeakingEvalRepository* const repository =
            m_session->speakingEvalRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "No Teacher Profile service is available."
                );
        }

        const Status saved = repository->saveSpeakingEval(
            *classId,
            legacyText(request.evaluationName),
            legacyRows(request.evaluation.rows),
            legacyChanges(request.evaluation.changedCells)
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

} // namespace ClassMngr::Next::Platform
