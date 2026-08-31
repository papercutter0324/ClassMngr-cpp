#include "gs_team_repository.h"

#include "classmngr/engine/gs_team_service.h"
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
using EngineGsTeamMember = classmngr::engine::GsTeamMember;
using EngineGsTeamService = classmngr::engine::GsTeamService;

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

EngineGsTeamMember toEngineGsTeamMember(
    const GsTeamMember& source
    )
{
    EngineGsTeamMember result;
    result.id = source.id;
    result.name = toUtf8(source.name);
    result.koreanName = toUtf8(source.koreanName);
    result.position = toUtf8(source.position);
    result.phoneNumber = toUtf8(source.phoneNumber);
    result.birthday = toUtf8(source.birthday);
    return result;
}

GsTeamMember fromEngineGsTeamMember(
    const EngineGsTeamMember& source
    )
{
    GsTeamMember result;
    result.id = source.id;
    result.name = fromUtf8(source.name);
    result.koreanName = fromUtf8(source.koreanName);
    result.position = fromUtf8(source.position);
    result.phoneNumber = fromUtf8(source.phoneNumber);
    result.birthday = fromUtf8(source.birthday);
    return result;
}

QString operationFailure(
    const QString& operation,
    const QString& detail
    )
{
    QString message = QObject::tr("%1 failed").arg(operation);
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
    const EngineError& error
    )
{
    return operationFailure(operation, engineErrorDetail(error));
}
}

GsTeamRepository::GsTeamRepository(QSqlDatabase& database)
    : m_database(database)
{
}

GsTeamRepository::~GsTeamRepository() = default;

Status GsTeamRepository::ensureEngineDatabase(
    const QString& operation
    ) const
{
    if (!m_database.isValid() || !m_database.isOpen())
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

    const QString databasePath = m_database.databaseName();
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

Result<QList<GsTeamMember>> GsTeamRepository::getAll() const
{
    const QString operation =
        QObject::tr("Loading GS Team directory");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineGsTeamService service(*m_engineDatabase);
    const classmngr::engine::Result<
        std::vector<EngineGsTeamMember>> loaded = service.list();
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }

    QList<GsTeamMember> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineGsTeamMember& member : *loaded)
    {
        result.append(fromEngineGsTeamMember(member));
    }

    return result;
}

Status GsTeamRepository::saveDirectory(
    const QList<GsTeamMember>& members,
    const QList<int>& deletedIds
    )
{
    const QString operation =
        QObject::tr("Saving GS Team directory");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    std::vector<EngineGsTeamMember> engineMembers;
    engineMembers.reserve(static_cast<std::size_t>(members.size()));
    for (const GsTeamMember& member : members)
    {
        engineMembers.push_back(toEngineGsTeamMember(member));
    }

    std::vector<int> engineDeletedIds;
    engineDeletedIds.reserve(static_cast<std::size_t>(deletedIds.size()));
    for (const int id : deletedIds)
    {
        engineDeletedIds.push_back(id);
    }

    EngineGsTeamService service(*m_engineDatabase);
    const classmngr::engine::Status saved = service.saveDirectory(
        engineMembers,
        engineDeletedIds
        );
    if (!saved)
    {
        return std::unexpected(engineFailure(operation, saved.error()));
    }

    return {};
}
