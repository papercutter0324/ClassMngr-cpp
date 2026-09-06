#include "intensive_slot_state_repository.h"

#include "classmngr/engine/intensive_slot_state_service.h"
#include "classmngr/engine/open_database.h"
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
using EngineIntensiveSlotState = classmngr::engine::IntensiveSlotState;
using EngineIntensiveSlotStateService =
    classmngr::engine::IntensiveSlotStateService;

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

EngineIntensiveSlotState toEngineState(
    const QString& day,
    const QString& startTime,
    const QString& state
    )
{
    return {
        toUtf8(day),
        toUtf8(startTime),
        toUtf8(state)
    };
}

IntensiveSlotState fromEngineState(
    const EngineIntensiveSlotState& source
    )
{
    IntensiveSlotState result;
    result.day = fromUtf8(source.day);
    result.startTime = fromUtf8(source.startTime);
    result.state = fromUtf8(source.state);
    return result;
}
} // namespace

IntensiveSlotStateRepository::IntensiveSlotStateRepository(const QString& databasePath)
    : m_databasePath(databasePath)
{
}

IntensiveSlotStateRepository::IntensiveSlotStateRepository(
    QSqlDatabase& database
    )
    : IntensiveSlotStateRepository(database.databaseName())
{
    m_compatibilityDatabaseWasOpen = database.isValid() && database.isOpen();
}

IntensiveSlotStateRepository::~IntensiveSlotStateRepository() = default;

Status IntensiveSlotStateRepository::ensureEngineDatabase(
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

Result<QList<IntensiveSlotState>>
IntensiveSlotStateRepository::loadIntensiveSlotStates()
{
    const QString operation =
        QObject::tr("Loading intensive slot states");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineIntensiveSlotStateService service(*m_engineDatabase);
    const classmngr::engine::Result<
        std::vector<EngineIntensiveSlotState>> loaded = service.list();
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }

    QList<IntensiveSlotState> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineIntensiveSlotState& state : *loaded)
    {
        result.append(fromEngineState(state));
    }

    return result;
}

Status IntensiveSlotStateRepository::saveIntensiveSlotState(
    const QString& day,
    const QString& startTime,
    const QString& state,
    const QString& defaultState
    )
{
    const QString operation = state == defaultState
        ? QObject::tr("Deleting intensive slot state")
        : QObject::tr("Saving intensive slot state");
    const QString identity = QObject::tr("intensive slot %1 at %2")
        .arg(day, startTime);
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    const EngineIntensiveSlotState converted = toEngineState(
        day,
        startTime,
        state
        );
    EngineIntensiveSlotStateService service(*m_engineDatabase);
    const classmngr::engine::Status saved = service.save(
        converted.day,
        converted.startTime,
        converted.state,
        toUtf8(defaultState)
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
