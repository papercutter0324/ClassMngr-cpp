#include "speaking_eval_repository.h"

#include "classmngr/engine/open_database.h"
#include "classmngr/engine/speaking_evaluation_persistence_service.h"
#include "classmngr/engine/sqlite_database.h"

#include <QByteArray>
#include <QObject>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using EngineError = classmngr::engine::Error;
using EngineSpeakingEvaluationCellChange =
    classmngr::engine::SpeakingEvaluationCellChange;
using EngineSpeakingEvaluationPersistenceService =
    classmngr::engine::SpeakingEvaluationPersistenceService;
using EngineSpeakingEvaluationRow =
    classmngr::engine::SpeakingEvaluationRow;
using EngineSpeakingEvaluationRows =
    classmngr::engine::SpeakingEvaluationRows;
using EngineSpeakingEvaluationScore =
    classmngr::engine::SpeakingEvaluationScore;

std::string toUtf8(
    const QString& value
    )
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(
    std::string_view value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

QString operationFailure(
    const QString& operation,
    const QString& detail = {},
    const QString& identity = {}
    )
{
    QString message = QObject::tr("%1 failed").arg(operation);
    const QString trimmedIdentity = identity.trimmed();
    if (!trimmedIdentity.isEmpty())
    {
        message += QObject::tr(" for %1").arg(identity);
    }

    const QString trimmedDetail = detail.trimmed();
    if (!trimmedDetail.isEmpty())
    {
        message += QStringLiteral(": ") + trimmedDetail;
    }

    return message;
}

QString engineErrorDetail(
    const EngineError& error
    )
{
    if (error.code == classmngr::engine::ErrorCode::NotFound)
    {
        return QObject::tr("no matching record exists.");
    }

    const QString detail = fromUtf8(error.message);
    if (!detail.trimmed().isEmpty())
    {
        return detail;
    }

    return QObject::tr("The engine reported a %1 error.")
        .arg(fromUtf8(classmngr::engine::errorCodeName(error.code)));
}

QString engineFailure(
    const QString& operation,
    const EngineError& error,
    const QString& identity = {}
    )
{
    return operationFailure(
        operation,
        engineErrorDetail(error),
        identity
        );
}

QString invalidArguments(
    const QString& operation
    )
{
    return QObject::tr(
        "%1 failed: invalid class id or evaluation name."
        ).arg(operation);
}

EngineSpeakingEvaluationRows toEngineRows(
    const SpeakingEvalRows& source
    )
{
    EngineSpeakingEvaluationRows result;
    result.reserve(static_cast<std::size_t>(source.size()));
    for (const QStringList& sourceRow : source)
    {
        EngineSpeakingEvaluationRow row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& value : sourceRow)
        {
            row.push_back(toUtf8(value));
        }
        result.push_back(std::move(row));
    }
    return result;
}

std::vector<EngineSpeakingEvaluationCellChange> toEngineDirtyCells(
    const QList<SpeakingEvalCellChange>& source
    )
{
    std::vector<EngineSpeakingEvaluationCellChange> result;
    result.reserve(static_cast<std::size_t>(source.size()));
    for (const SpeakingEvalCellChange& cell : source)
    {
        result.push_back({cell.row, cell.column});
    }
    return result;
}

SpeakingEvalRows fromEngineRows(
    const EngineSpeakingEvaluationRows& source
    )
{
    SpeakingEvalRows result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const EngineSpeakingEvaluationRow& sourceRow : source)
    {
        QStringList row;
        row.reserve(static_cast<qsizetype>(sourceRow.size()));
        for (const std::string& value : sourceRow)
        {
            row.append(fromUtf8(value));
        }
        result.append(std::move(row));
    }
    return result;
}

SpeakingEvalScore fromEngineScore(
    const EngineSpeakingEvaluationScore& source
    )
{
    return {
        fromUtf8(source.englishName),
        fromUtf8(source.koreanName),
        fromUtf8(source.finalGrade)
    };
}
} // namespace

SpeakingEvalRepository::SpeakingEvalRepository(const QString& databasePath)
    : m_databasePath(databasePath)
{
}

SpeakingEvalRepository::SpeakingEvalRepository(
    QSqlDatabase& database
    )
    : SpeakingEvalRepository(database.databaseName())
{
    m_compatibilityDatabaseWasOpen = database.isValid() && database.isOpen();
}

SpeakingEvalRepository::~SpeakingEvalRepository() = default;

Status SpeakingEvalRepository::ensureEngineDatabase(
    const QString& operation
    ) const
{
    if (!m_compatibilityDatabaseWasOpen)
    {
        m_engineDatabase.reset();
        m_engineDatabasePath.clear();
        return std::unexpected(
            operationFailure(
                operation,
                QObject::tr("No Teacher Profile is open.")
                )
            );
    }

    const QString databasePath = m_databasePath;
    if (databasePath.trimmed().isEmpty()
        || databasePath.trimmed() == QStringLiteral(":memory:"))
    {
        m_engineDatabase.reset();
        m_engineDatabasePath.clear();
        return std::unexpected(
            operationFailure(
                operation,
                QObject::tr("No database path is available.")
                )
            );
    }

    if (m_engineDatabase
        && m_engineDatabase->isOpen()
        && m_engineDatabasePath == databasePath)
    {
        return {};
    }

    m_engineDatabase.reset();
    m_engineDatabasePath.clear();

    auto opened = classmngr::engine::OpenDatabase::execute(toUtf8(databasePath));
    if (!opened)
    {
        return std::unexpected(engineFailure(operation, opened.error()));
    }
    if (*opened == nullptr)
    {
        return std::unexpected(
            operationFailure(
                operation,
                QObject::tr("The engine database could not be opened.")
                )
            );
    }

    m_engineDatabase = std::move(*opened);
    m_engineDatabasePath = databasePath;
    return {};
}

Status SpeakingEvalRepository::saveSpeakingEval(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells
    )
{
    const QString operation = QObject::tr("Saving speaking evaluation");
    if (classId <= 0 || evaluationName.trimmed().isEmpty())
    {
        return std::unexpected(invalidArguments(operation));
    }

    const QString identity = QObject::tr("evaluation '%1' for class id %2")
        .arg(evaluationName.trimmed())
        .arg(classId);
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineSpeakingEvaluationPersistenceService service(*m_engineDatabase);
    const classmngr::engine::Status saved = service.save(
        classId,
        toUtf8(evaluationName),
        toEngineRows(rows),
        toEngineDirtyCells(dirtyCells)
        );
    if (!saved)
    {
        return std::unexpected(engineFailure(
            operation,
            saved.error(),
            identity
            ));
    }

    return {};
}

Result<SpeakingEvalRows> SpeakingEvalRepository::loadSpeakingEval(
    int classId,
    const QString& evaluationName
    )
{
    const QString operation = QObject::tr("Loading speaking evaluation");
    if (classId <= 0 || evaluationName.trimmed().isEmpty())
    {
        return std::unexpected(invalidArguments(operation));
    }

    const QString identity = QObject::tr("class id %1, evaluation '%2'")
        .arg(classId)
        .arg(evaluationName.trimmed());
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineSpeakingEvaluationPersistenceService service(*m_engineDatabase);
    const classmngr::engine::Result<EngineSpeakingEvaluationRows> loaded =
        service.load(classId, toUtf8(evaluationName));
    if (!loaded)
    {
        return std::unexpected(engineFailure(
            operation,
            loaded.error(),
            identity
            ));
    }

    return fromEngineRows(*loaded);
}

Result<QList<SpeakingEvalScore>>
SpeakingEvalRepository::buildRosterScoreImport(
    int classId,
    const QString& evaluationName
    )
{
    const QString operation = QObject::tr("Loading speaking evaluation");
    if (classId <= 0 || evaluationName.trimmed().isEmpty())
    {
        return std::unexpected(invalidArguments(operation));
    }

    const QString identity = QObject::tr("class id %1, evaluation '%2'")
        .arg(classId)
        .arg(evaluationName.trimmed());
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineSpeakingEvaluationPersistenceService service(*m_engineDatabase);
    const classmngr::engine::Result<
        std::vector<EngineSpeakingEvaluationScore>> loaded =
        service.buildRosterScoreImport(classId, toUtf8(evaluationName));
    if (!loaded)
    {
        return std::unexpected(engineFailure(
            operation,
            loaded.error(),
            identity
            ));
    }

    QList<SpeakingEvalScore> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineSpeakingEvaluationScore& score : *loaded)
    {
        result.append(fromEngineScore(score));
    }
    return result;
}
