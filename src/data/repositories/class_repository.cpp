#include "class_repository.h"

#include "classmngr/engine/class_repository.h"
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
using EngineClassroom = classmngr::engine::Classroom;
using EngineClassRepository = classmngr::engine::ClassRepository;
using EngineError = classmngr::engine::Error;

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

Classroom fromEngineClassroom(
    const EngineClassroom& source
    )
{
    Classroom result;
    result.id = source.id;
    result.name = fromUtf8(source.name);
    return result;
}

QString classIdentity(
    int classId
    )
{
    return QObject::tr("class id %1").arg(classId);
}

QString invalidClassIdError(
    const QString& operation,
    int classId
    )
{
    return QObject::tr("%1 failed: invalid class id %2.")
        .arg(operation)
        .arg(classId);
}

QString operationFailure(
    const QString& operation,
    const QString& classContext,
    const QString& detail
    )
{
    QString message = QObject::tr("%1 failed").arg(operation);

    if (!classContext.trimmed().isEmpty())
    {
        message += QObject::tr(" for %1").arg(classContext);
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
    const QString& classContext,
    const EngineError& error
    )
{
    return operationFailure(
        operation,
        classContext,
        engineErrorDetail(error)
        );
}
} // namespace

ClassRepository::ClassRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

ClassRepository::~ClassRepository() = default;

Status ClassRepository::ensureEngineDatabase(
    const QString& operation,
    const QString& classContext
    )
{
    if (!m_database.isValid() || !m_database.isOpen())
    {
        m_engineDatabase.reset();
        m_engineDatabasePath.clear();
        return std::unexpected(
            operationFailure(
                operation,
                classContext,
                QObject::tr("No Teacher Profile is open.")
                )
            );
    }

    const QString databasePath = m_database.databaseName();
    if (databasePath.trimmed().isEmpty())
    {
        m_engineDatabase.reset();
        m_engineDatabasePath.clear();
        return std::unexpected(
            operationFailure(
                operation,
                classContext,
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

    const std::string encodedPath = toUtf8(databasePath);
    auto opened = classmngr::engine::OpenDatabase::execute(encodedPath);
    if (!opened)
    {
        return std::unexpected(
            engineFailure(operation, classContext, opened.error())
            );
    }
    if (*opened == nullptr)
    {
        return std::unexpected(
            operationFailure(
                operation,
                classContext,
                QObject::tr("The engine database could not be opened.")
                )
            );
    }

    m_engineDatabase = std::move(*opened);
    m_engineDatabasePath = databasePath;
    return {};
}

Result<int> ClassRepository::createClass(
    const QString& name
    )
{
    const QString operation = QObject::tr("Creating class");
    const QString identity = QObject::tr("class name '%1'")
        .arg(name);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineClassRepository repository(*m_engineDatabase);
    const classmngr::engine::Result<int> created = repository.create(
        toUtf8(name)
        );
    if (!created)
    {
        return std::unexpected(
            engineFailure(operation, identity, created.error())
            );
    }

    return *created;
}

Result<QList<Classroom>> ClassRepository::getClasses()
{
    const QString operation = QObject::tr("Loading classes");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineClassRepository repository(*m_engineDatabase);
    const classmngr::engine::Result<std::vector<EngineClassroom>> loaded =
        repository.list();
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, {}, loaded.error())
            );
    }

    QList<Classroom> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineClassroom& classroom : *loaded)
    {
        result.append(fromEngineClassroom(classroom));
    }

    return result;
}

Result<Classroom> ClassRepository::getClassById(
    int classId
    )
{
    const QString operation = QObject::tr("Loading class");
    if (classId <= 0)
    {
        return std::unexpected(invalidClassIdError(operation, classId));
    }

    const QString identity = classIdentity(classId);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineClassRepository repository(*m_engineDatabase);
    const classmngr::engine::Result<EngineClassroom> loaded =
        repository.get(classId);
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, identity, loaded.error())
            );
    }

    return fromEngineClassroom(*loaded);
}

Status ClassRepository::updateClassName(
    int classId,
    const QString& name
    )
{
    const QString operation = QObject::tr("Renaming class");
    if (classId <= 0)
    {
        return std::unexpected(invalidClassIdError(operation, classId));
    }

    const QString identity = classIdentity(classId);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineClassRepository repository(*m_engineDatabase);
    const classmngr::engine::Status renamed = repository.rename(
        classId,
        toUtf8(name)
        );
    if (!renamed)
    {
        return std::unexpected(
            engineFailure(operation, identity, renamed.error())
            );
    }

    return {};
}

Status ClassRepository::deleteClass(
    int classId
    )
{
    const QString operation = QObject::tr("Deleting class");
    if (classId <= 0)
    {
        return std::unexpected(invalidClassIdError(operation, classId));
    }

    const QString identity = classIdentity(classId);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineClassRepository repository(*m_engineDatabase);
    const classmngr::engine::Status deleted = repository.remove(classId);
    if (!deleted)
    {
        return std::unexpected(
            engineFailure(operation, identity, deleted.error())
            );
    }

    return {};
}
