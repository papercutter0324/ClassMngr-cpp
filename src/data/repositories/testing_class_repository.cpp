#include "testing_class_repository.h"

#include "classmngr/engine/open_database.h"
#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/testing_class_service.h"

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
using EngineTestingClass = classmngr::engine::TestingClass;
using EngineTestingClassService =
    classmngr::engine::TestingClassService;

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

EngineTestingClass toEngineTestingClass(
    const TestingClass& source
    )
{
    EngineTestingClass result;
    result.classId = source.classId;
    result.name = toUtf8(source.name);
    result.grade = toUtf8(source.grade);
    result.level = toUtf8(source.level);
    result.room = toUtf8(source.room);
    result.teacherId = source.teacherId;
    result.classColor = toUtf8(source.classColor);
    result.fontColor = toUtf8(source.fontColor);
    result.notes = toUtf8(source.notes);
    return result;
}

TestingClass fromEngineTestingClass(
    const EngineTestingClass& source
    )
{
    TestingClass result;
    result.classId = source.classId;
    result.name = fromUtf8(source.name);
    result.grade = fromUtf8(source.grade);
    result.level = fromUtf8(source.level);
    result.room = fromUtf8(source.room);
    result.teacherId = source.teacherId;
    result.classColor = fromUtf8(source.classColor);
    result.fontColor = fromUtf8(source.fontColor);
    result.notes = fromUtf8(source.notes);
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

QString testingClassIdentity(
    int classId
    )
{
    return QObject::tr("testing class id %1").arg(classId);
}

QString testingClassIdentity(
    const TestingClass& testingClass
    )
{
    const QString displayName = testingClass.name.trimmed();
    if (!displayName.isEmpty())
    {
        return QObject::tr("testing class '%1'").arg(displayName);
    }

    return QObject::tr("testing class");
}
} // namespace

TestingClassRepository::TestingClassRepository(const QString& databasePath)
    : m_databasePath(databasePath)
{
}

TestingClassRepository::TestingClassRepository(
    QSqlDatabase& database
    )
    : TestingClassRepository(database.databaseName())
{
    m_compatibilityDatabaseWasOpen = database.isValid() && database.isOpen();
}

TestingClassRepository::~TestingClassRepository() = default;

Status TestingClassRepository::ensureEngineDatabase(
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

Result<int> TestingClassRepository::createTestingClass(
    const TestingClass& testingClass,
    const QString& assignmentDay,
    const QString& assignmentStartTime
    )
{
    const QString operation = QObject::tr("Creating testing class");
    const QString identity = testingClassIdentity(testingClass);
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineTestingClassService service(*m_engineDatabase);
    const classmngr::engine::Result<int> created = service.create(
        toEngineTestingClass(testingClass),
        toUtf8(assignmentDay),
        toUtf8(assignmentStartTime)
        );
    if (!created)
    {
        return std::unexpected(
            engineFailure(operation, created.error(), identity)
            );
    }

    return *created;
}

Status TestingClassRepository::updateTestingClass(
    const TestingClass& testingClass
    )
{
    const QString operation = QObject::tr("Updating testing class");
    const QString identity = testingClass.classId > 0
        ? testingClassIdentity(testingClass.classId)
        : testingClassIdentity(testingClass);
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineTestingClassService service(*m_engineDatabase);
    const classmngr::engine::Status updated = service.update(
        toEngineTestingClass(testingClass)
        );
    if (!updated)
    {
        return std::unexpected(
            engineFailure(operation, updated.error(), identity)
            );
    }

    return {};
}

Result<TestingClass> TestingClassRepository::loadTestingClass(
    int classId
    )
{
    const QString operation = QObject::tr("Loading testing class");
    const QString identity = testingClassIdentity(classId);
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineTestingClassService service(*m_engineDatabase);
    const classmngr::engine::Result<EngineTestingClass> loaded = service.get(
        classId
        );
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, loaded.error(), identity)
            );
    }

    return fromEngineTestingClass(*loaded);
}

Result<QList<TestingClass>>
TestingClassRepository::loadTestingClasses()
{
    const QString operation = QObject::tr("Loading testing classes");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineTestingClassService service(*m_engineDatabase);
    const classmngr::engine::Result<
        std::vector<EngineTestingClass>> loaded = service.list();
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }

    QList<TestingClass> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineTestingClass& testingClass : *loaded)
    {
        result.append(fromEngineTestingClass(testingClass));
    }

    return result;
}

Status TestingClassRepository::deleteTestingClass(
    int classId
    )
{
    const QString operation = QObject::tr("Deleting testing class");
    const QString identity = testingClassIdentity(classId);
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineTestingClassService service(*m_engineDatabase);
    const classmngr::engine::Status deleted = service.remove(classId);
    if (!deleted)
    {
        return std::unexpected(
            engineFailure(operation, deleted.error(), identity)
            );
    }

    return {};
}

Result<bool> TestingClassRepository::isTestingClass(
    int classId
    )
{
    const QString operation = QObject::tr("Checking testing class");
    const QString identity = testingClassIdentity(classId);
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineTestingClassService service(*m_engineDatabase);
    const classmngr::engine::Result<bool> exists =
        service.isTestingClass(classId);
    if (!exists)
    {
        return std::unexpected(
            engineFailure(operation, exists.error(), identity)
            );
    }

    return *exists;
}
