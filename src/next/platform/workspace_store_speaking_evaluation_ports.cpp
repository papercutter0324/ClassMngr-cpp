#include "next/platform/workspace_store_speaking_evaluation_ports.h"

#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "domain/models/speaking_evaluation.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QStringList>

#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{
namespace
{

using namespace Application;
using namespace Domain;

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

QString qtText(const std::u16string& value)
{
    return QString::fromUtf16(
        value.data(), static_cast<qsizetype>(value.size()));
}

std::optional<int> classIdAsInt(const Domain::ClassId& classId)
{
    return parseCanonicalPositiveDecimalId(classId.value());
}

OperationError repositoryError(
    const RepositoryError& error,
    const char* fallback
    )
{
    const std::string message = utf8(error.diagnostic);
    return {
        .code = repositoryDomainErrorCode(error.code),
        .message = message.empty() ? fallback : message,
        .recoverable = error.code != RepositoryErrorCode::InvalidInput
    };
}

SpeakingEvaluationReadResult readFailure(
    const ErrorCode code,
    std::string message,
    const bool recoverable = true
    )
{
    return SpeakingEvaluationReadResult::failure({
        .code = code,
        .message = std::move(message),
        .recoverable = recoverable
    });
}

Domain::Result<void> saveFailure(
    const ErrorCode code,
    std::string message,
    const bool recoverable = true
    )
{
    return Domain::Result<void>::failure({
        .code = code,
        .message = std::move(message),
        .recoverable = recoverable
    });
}

SpeakingEvaluationRosterNamesReadResult rosterNamesFailure(
    const ErrorCode code,
    std::string message,
    const bool recoverable = true
    )
{
    return SpeakingEvaluationRosterNamesReadResult::failure({
        .code = code,
        .message = std::move(message),
        .recoverable = recoverable
    });
}

std::vector<std::u16string> applicationRow(const QStringList& row)
{
    std::vector<std::u16string> converted;
    converted.reserve(static_cast<std::size_t>(row.size()));
    for (const QString& cell : row)
    {
        converted.push_back(cell.toStdU16String());
    }
    return converted;
}

SpeakingEvalRows repositoryRows(
    const std::vector<std::vector<std::u16string>>& rows
    )
{
    SpeakingEvalRows converted;
    converted.reserve(static_cast<qsizetype>(rows.size()));
    for (const std::vector<std::u16string>& sourceRow : rows)
    {
        QStringList row;
        row.reserve(static_cast<qsizetype>(sourceRow.size()));
        for (const std::u16string& cell : sourceRow)
        {
            row.append(qtText(cell));
        }
        converted.append(std::move(row));
    }
    return converted;
}

QList<SpeakingEvalCellChange> repositoryChanges(
    const std::vector<SpeakingEvaluationCellChange>& changes
    )
{
    QList<SpeakingEvalCellChange> converted;
    converted.reserve(static_cast<qsizetype>(changes.size()));
    for (const SpeakingEvaluationCellChange& change : changes)
    {
        converted.append({change.row.rawValue(), change.column.rawValue()});
    }
    return converted;
}

} // namespace

WorkspaceStoreSpeakingEvaluationReadPort::
WorkspaceStoreSpeakingEvaluationReadPort(QString databasePath)
    : m_databasePath(std::move(databasePath))
{
}

Application::SpeakingEvaluationReadResult
WorkspaceStoreSpeakingEvaluationReadPort::readEvaluation(
    const Application::SpeakingEvaluationReadQuery& query
    ) const
{
    const std::optional<int> classId = classIdAsInt(query.classId);
    if (!classId)
    {
        return readFailure(
            ErrorCode::InvalidInput,
            "Class ID must be a canonical positive integer.",
            false
            );
    }
    if (m_databasePath.trimmed().isEmpty())
    {
        return readFailure(
            ErrorCode::NotFound,
            "The active workspace is unavailable.");
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(m_databasePath);
    if (!opened)
    {
        return SpeakingEvaluationReadResult::failure(repositoryError(
            opened.error(), "The active workspace could not be opened."));
    }

    SpeakingEvalRepository* const repository = store.speakingEvalRepository();
    if (!repository)
    {
        return readFailure(
            ErrorCode::NotFound,
            "The speaking evaluation repository is unavailable.");
    }

    try
    {
        Application::SpeakingEvaluationReadSnapshot snapshot{
            .classId = query.classId,
            .evaluationName = query.evaluationName,
            .rows = {}
        };
        snapshot.rows.reserve(25);
        const auto loaded = repository->forEachSpeakingEvalRowDetailed(
            *classId,
            qtText(query.evaluationName),
            [&snapshot](const QStringList& row)
            {
                snapshot.rows.push_back(applicationRow(row));
            });
        if (!loaded)
        {
            return readFailure(
                repositoryDomainErrorCode(loaded.error().code),
                utf8(loaded.error().diagnostic),
                loaded.error().code != RepositoryErrorCode::InvalidInput);
        }
        return SpeakingEvaluationReadResult::success(std::move(snapshot));
    }
    catch (const std::exception&)
    {
        return readFailure(
            ErrorCode::Technical,
            "Speaking evaluation could not be loaded.");
    }
    catch (...)
    {
        return readFailure(
            ErrorCode::Technical,
            "Speaking evaluation could not be loaded.");
    }
}

WorkspaceStoreSpeakingEvaluationSavePort::
WorkspaceStoreSpeakingEvaluationSavePort(QString databasePath)
    : m_databasePath(std::move(databasePath))
{
}

Domain::Result<void> WorkspaceStoreSpeakingEvaluationSavePort::saveEvaluation(
    const Application::SpeakingEvaluationSaveRequest& request
    ) const
{
    const std::optional<int> classId = classIdAsInt(request.classId);
    if (!classId)
    {
        return saveFailure(
            ErrorCode::InvalidInput,
            "Class ID must be a canonical positive integer.",
            false);
    }
    if (m_databasePath.trimmed().isEmpty())
    {
        return saveFailure(
            ErrorCode::NotFound,
            "The active workspace is unavailable.");
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(m_databasePath);
    if (!opened)
    {
        return Domain::Result<void>::failure(repositoryError(
            opened.error(), "The active workspace could not be opened."));
    }

    SpeakingEvalRepository* const repository = store.speakingEvalRepository();
    if (!repository)
    {
        return saveFailure(
            ErrorCode::NotFound,
            "The speaking evaluation repository is unavailable.");
    }

    try
    {
        const RepositoryStatus saved = repository->saveSpeakingEvalDetailed(
            *classId,
            qtText(request.evaluationName),
            repositoryRows(request.evaluation.rows),
            repositoryChanges(request.evaluation.changedCells));
        if (!saved)
        {
            return saveFailure(
                repositoryDomainErrorCode(saved.error().code),
                utf8(saved.error().diagnostic),
                saved.error().code != RepositoryErrorCode::InvalidInput);
        }
        return Domain::Result<void>::success();
    }
    catch (const std::exception&)
    {
        return saveFailure(
            ErrorCode::Technical,
            "Speaking evaluation could not be saved.");
    }
    catch (...)
    {
        return saveFailure(
            ErrorCode::Technical,
            "Speaking evaluation could not be saved.");
    }
}

WorkspaceStoreSpeakingEvaluationRosterNamesReadPort::
WorkspaceStoreSpeakingEvaluationRosterNamesReadPort(QString databasePath)
    : m_databasePath(std::move(databasePath))
{
}

Application::SpeakingEvaluationRosterNamesReadResult
WorkspaceStoreSpeakingEvaluationRosterNamesReadPort::
readSpeakingEvaluationRosterNames(
    const Application::SpeakingEvaluationRosterNamesReadRequest& request
    ) const
{
    const std::optional<int> classId = classIdAsInt(request.classId);
    if (!classId)
    {
        return rosterNamesFailure(
            ErrorCode::InvalidInput,
            "Class ID must be a canonical positive integer.",
            false);
    }
    if (m_databasePath.trimmed().isEmpty())
    {
        return rosterNamesFailure(
            ErrorCode::NotFound,
            "The active workspace is unavailable.");
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(m_databasePath);
    if (!opened)
    {
        return SpeakingEvaluationRosterNamesReadResult::failure(
            repositoryError(opened.error(),
                "The active workspace could not be opened."));
    }

    RosterRepository* const repository = store.rosterRepository();
    if (!repository)
    {
        return rosterNamesFailure(
            ErrorCode::NotFound,
            "The roster repository is unavailable.");
    }

    try
    {
        const auto loaded = repository->loadSpeakingEvaluationRosterNamesDetailed(
            *classId);
        if (!loaded)
        {
            return rosterNamesFailure(
                repositoryDomainErrorCode(loaded.error().code),
                utf8(loaded.error().diagnostic),
                loaded.error().code != RepositoryErrorCode::InvalidInput);
        }
        if (loaded->parentMissing)
        {
            return rosterNamesFailure(
                ErrorCode::NotFound,
                "The selected class was not found.",
                false);
        }

        SpeakingEvaluationRosterNamesReadSnapshot snapshot(request.classId);
        snapshot.hasEnglishColumn = loaded->rosterNames.hasEnglishColumn;
        snapshot.hasKoreanColumn = loaded->rosterNames.hasKoreanColumn;
        snapshot.rows.reserve(
            static_cast<std::size_t>(loaded->rosterNames.rows.size()));
        for (const auto& row : loaded->rosterNames.rows)
        {
            snapshot.rows.push_back({
                row.englishName.toStdU16String(),
                row.koreanName.toStdU16String()
            });
        }
        return SpeakingEvaluationRosterNamesReadResult::success(
            std::move(snapshot));
    }
    catch (const std::exception&)
    {
        return rosterNamesFailure(
            ErrorCode::Technical,
            "Roster names could not be loaded.");
    }
    catch (...)
    {
        return rosterNamesFailure(
            ErrorCode::Technical,
            "Roster names could not be loaded.");
    }
}

WorkspaceStoreSpeakingEvaluationClassGradeReadPort::
WorkspaceStoreSpeakingEvaluationClassGradeReadPort(QString databasePath)
    : m_databasePath(std::move(databasePath))
{
}

Application::SelectedClassGradeReadResult
WorkspaceStoreSpeakingEvaluationClassGradeReadPort::
readSelectedClassGrade(const Domain::ClassId& classId) const
{
    const std::optional<int> parsedClassId = classIdAsInt(classId);
    if (!parsedClassId)
    {
        return Application::SelectedClassGradeReadResult::failure({
            .code = ErrorCode::InvalidInput,
            .message = "Class ID must be a canonical positive integer.",
            .recoverable = false
        });
    }
    if (m_databasePath.trimmed().isEmpty())
    {
        return Application::SelectedClassGradeReadResult::failure({
            .code = ErrorCode::NotFound,
            .message = "The active workspace is unavailable.",
            .recoverable = true
        });
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(m_databasePath);
    if (!opened)
    {
        return Application::SelectedClassGradeReadResult::failure(repositoryError(
            opened.error(), "The active workspace could not be opened."));
    }

    ClassInfoRepository* const repository = store.classInfoRepository();
    if (!repository)
    {
        return Application::SelectedClassGradeReadResult::failure({
            .code = ErrorCode::NotFound,
            .message = "The class information repository is unavailable.",
            .recoverable = true
        });
    }

    try
    {
        const auto loaded = repository->loadSelectedClassGradeRecordDetailed(
            *parsedClassId);
        if (!loaded)
        {
            return Application::SelectedClassGradeReadResult::failure(
                repositoryError(loaded.error(),
                    "The class grade could not be loaded."));
        }
        if (loaded->parentMissing)
        {
            return Application::SelectedClassGradeReadResult::failure({
                .code = ErrorCode::NotFound,
                .message = "The selected class was not found.",
                .recoverable = false
            });
        }

        const QByteArray gradeBytes = loaded->record.classGrade.toUtf8();
        Application::SelectedClassGradeReadSnapshot snapshot{
            .classId = classId,
            .classGrade = Application::SelectedClassGradeValue(
                std::string(gradeBytes.constData(),
                    static_cast<std::size_t>(gradeBytes.size())))
        };
        return Application::SelectedClassGradeReadResult::success(
            std::move(snapshot));
    }
    catch (const std::exception&)
    {
        return Application::SelectedClassGradeReadResult::failure({
            .code = ErrorCode::Technical,
            .message = "The class grade could not be loaded.",
            .recoverable = true
        });
    }
    catch (...)
    {
        return Application::SelectedClassGradeReadResult::failure({
            .code = ErrorCode::Technical,
            .message = "The class grade could not be loaded.",
            .recoverable = true
        });
    }
}

} // namespace ClassMngr::Next::Platform
