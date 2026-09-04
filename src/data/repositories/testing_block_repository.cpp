#include "testing_block_repository.h"

#include "classmngr/engine/open_database.h"
#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/testing_block_service.h"

#include <QByteArray>
#include <QObject>

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using EngineError = classmngr::engine::Error;
using EngineTestingAssignment = classmngr::engine::TestingAssignment;
using EngineTestingBlock = classmngr::engine::TestingBlock;
using EngineTestingBlockService =
    classmngr::engine::TestingBlockService;

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

TestingAssignment fromEngineAssignment(
    const EngineTestingAssignment& source
    )
{
    TestingAssignment result;
    result.day = fromUtf8(source.day);
    result.startTime = fromUtf8(source.startTime);
    result.kind = source.kind
        == classmngr::engine::TestingAssignmentKind::SpecialClass
        ? TestingAssignmentKind::SpecialClass
        : TestingAssignmentKind::PlainTesting;
    result.room = fromUtf8(source.room);
    result.classId = source.classId;
    return result;
}

TestingBlock fromEngineBlock(
    const EngineTestingBlock& source
    )
{
    TestingBlock result;
    result.day = fromUtf8(source.day);
    result.startTime = fromUtf8(source.startTime);
    result.room = fromUtf8(source.room);
    return result;
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

QString slotIdentity(
    const QString& day,
    const QString& startTime
    )
{
    return QObject::tr("slot %1 at %2").arg(day, startTime);
}
} // namespace

TestingBlockRepository::TestingBlockRepository(const QString& databasePath)
    : m_databasePath(databasePath)
{
}

TestingBlockRepository::TestingBlockRepository(
    QSqlDatabase& database
    )
    : TestingBlockRepository(database.databaseName())
{
    m_compatibilityDatabaseWasOpen = database.isValid() && database.isOpen();
}

TestingBlockRepository::~TestingBlockRepository() = default;

Status TestingBlockRepository::ensureEngineDatabase(
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

    auto opened = classmngr::engine::OpenDatabase::execute(
        toUtf8(databasePath)
        );
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

Result<QList<TestingAssignment>>
TestingBlockRepository::loadTestingAssignments()
{
    const QString operation = QObject::tr("Loading testing blocks");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineTestingBlockService service(*m_engineDatabase);
    const classmngr::engine::Result<
        std::vector<EngineTestingAssignment>> loaded =
        service.listAssignments();
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }

    QList<TestingAssignment> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineTestingAssignment& assignment : *loaded)
    {
        result.append(fromEngineAssignment(assignment));
    }

    return result;
}

Result<QList<TestingBlock>> TestingBlockRepository::loadTestingBlocks()
{
    const QString operation = QObject::tr("Loading testing blocks");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineTestingBlockService service(*m_engineDatabase);
    const classmngr::engine::Result<std::vector<EngineTestingBlock>> loaded =
        service.listBlocks();
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }

    QList<TestingBlock> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineTestingBlock& block : *loaded)
    {
        result.append(fromEngineBlock(block));
    }

    return result;
}

Status TestingBlockRepository::saveTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room,
    bool replaceExisting
    )
{
    const QString operation = QObject::tr("Saving the testing block");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineTestingBlockService service(*m_engineDatabase);
    const classmngr::engine::Status saved = service.saveBlock(
        toUtf8(day),
        toUtf8(startTime),
        toUtf8(room),
        replaceExisting
        );
    if (!saved)
    {
        return std::unexpected(engineFailure(
            operation,
            saved.error(),
            slotIdentity(day, startTime)
            ));
    }

    return {};
}

Status TestingBlockRepository::assignTestingClass(
    const QString& day,
    const QString& startTime,
    int classId,
    bool replaceExisting
    )
{
    const QString operation = QObject::tr("Assigning the testing class");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineTestingBlockService service(*m_engineDatabase);
    const classmngr::engine::Status assigned = service.assignClass(
        toUtf8(day),
        toUtf8(startTime),
        classId,
        replaceExisting
        );
    if (!assigned)
    {
        return std::unexpected(engineFailure(
            operation,
            assigned.error(),
            slotIdentity(day, startTime)
            ));
    }

    return {};
}

Status TestingBlockRepository::deleteTestingAssignment(
    const QString& day,
    const QString& startTime
    )
{
    const QString operation = QObject::tr("Removing the testing assignment");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineTestingBlockService service(*m_engineDatabase);
    const classmngr::engine::Status deleted = service.deleteAssignment(
        toUtf8(day),
        toUtf8(startTime)
        );
    if (!deleted)
    {
        return std::unexpected(engineFailure(
            operation,
            deleted.error(),
            slotIdentity(day, startTime)
            ));
    }

    return {};
}

Status TestingBlockRepository::deleteTestingBlock(
    const QString& day,
    const QString& startTime
    )
{
    const QString operation = QObject::tr("Removing the testing block");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineTestingBlockService service(*m_engineDatabase);
    const classmngr::engine::Status deleted = service.deleteBlock(
        toUtf8(day),
        toUtf8(startTime)
        );
    if (!deleted)
    {
        return std::unexpected(engineFailure(
            operation,
            deleted.error(),
            slotIdentity(day, startTime)
            ));
    }

    return {};
}

Status TestingBlockRepository::clearTestingAssignments()
{
    const QString operation = QObject::tr("Clearing the testing layout");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineTestingBlockService service(*m_engineDatabase);
    const classmngr::engine::Status cleared = service.clearAssignments();
    if (!cleared)
    {
        return std::unexpected(engineFailure(operation, cleared.error()));
    }

    return {};
}

Status TestingBlockRepository::clearTestingBlocks()
{
    const QString operation = QObject::tr("Clearing the testing layout");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineTestingBlockService service(*m_engineDatabase);
    const classmngr::engine::Status cleared = service.clearBlocks();
    if (!cleared)
    {
        return std::unexpected(engineFailure(operation, cleared.error()));
    }

    return {};
}
