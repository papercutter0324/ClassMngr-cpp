#include "teacher_repository.h"

#include "classmngr/engine/open_database.h"
#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/teacher_service.h"

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
using EngineTeacher = classmngr::engine::Teacher;
using EngineTeacherService = classmngr::engine::TeacherService;

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

EngineTeacher toEngineTeacher(
    const Teacher& source
    )
{
    EngineTeacher result;
    result.id = source.id;
    result.teacherKr = toUtf8(source.teacherKr);
    result.teacherEn = toUtf8(source.teacherEn);
    result.preferredRomanization = toUtf8(source.preferredRomanization);
    result.preferredName = toUtf8(source.preferredName);
    result.roomNumber = toUtf8(source.roomNumber);
    result.birthday = toUtf8(source.birthday);
    result.phoneNumber = toUtf8(source.phoneNumber);
    result.wifiName = toUtf8(source.wifiName);
    result.wifiPassword = toUtf8(source.wifiPassword);
    result.internetType = toUtf8(source.internetType);
    result.zoomId = toUtf8(source.zoomId);
    result.zoomPassword = toUtf8(source.zoomPassword);
    result.projectionType = toUtf8(source.projectionType);
    result.notes = toUtf8(source.notes);
    return result;
}

Teacher fromEngineTeacher(
    const EngineTeacher& source
    )
{
    Teacher result;
    result.id = source.id;
    result.teacherKr = fromUtf8(source.teacherKr);
    result.teacherEn = fromUtf8(source.teacherEn);
    result.preferredRomanization = fromUtf8(source.preferredRomanization);
    result.preferredName = fromUtf8(source.preferredName);
    result.roomNumber = fromUtf8(source.roomNumber);
    result.birthday = fromUtf8(source.birthday);
    result.phoneNumber = fromUtf8(source.phoneNumber);
    result.wifiName = fromUtf8(source.wifiName);
    result.wifiPassword = fromUtf8(source.wifiPassword);
    result.internetType = fromUtf8(source.internetType);
    result.zoomId = fromUtf8(source.zoomId);
    result.zoomPassword = fromUtf8(source.zoomPassword);
    result.projectionType = fromUtf8(source.projectionType);
    result.notes = fromUtf8(source.notes);
    return result;
}

QString teacherIdentity(
    int teacherId
    )
{
    return QObject::tr("teacher id %1").arg(teacherId);
}

QString teacherIdentity(
    const Teacher& teacher
    )
{
    const QString displayName = teacher.preferredDisplayName();
    if (displayName.isEmpty())
    {
        return QObject::tr("teacher");
    }

    return QObject::tr("teacher '%1'").arg(displayName);
}

QString invalidTeacherIdError(
    const QString& operation,
    int teacherId
    )
{
    return QObject::tr("%1 failed: invalid teacher id %2.")
        .arg(operation)
        .arg(teacherId);
}

QString operationFailure(
    const QString& operation,
    const QString& teacherContext,
    const QString& detail
    )
{
    QString message = QObject::tr("%1 failed").arg(operation);

    if (!teacherContext.trimmed().isEmpty())
    {
        message += QObject::tr(" for %1").arg(teacherContext);
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
    const QString& teacherContext,
    const EngineError& error
    )
{
    return operationFailure(
        operation,
        teacherContext,
        engineErrorDetail(error)
        );
}
} // namespace

TeacherRepository::TeacherRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

TeacherRepository::~TeacherRepository() = default;

Status TeacherRepository::ensureEngineDatabase(
    const QString& operation,
    const QString& teacherContext
    )
{
    if (!m_database.isValid() || !m_database.isOpen())
    {
        m_engineDatabase.reset();
        m_engineDatabasePath.clear();
        return std::unexpected(
            operationFailure(
                operation,
                teacherContext,
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
                teacherContext,
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
            engineFailure(operation, teacherContext, opened.error())
            );
    }
    if (*opened == nullptr)
    {
        return std::unexpected(
            operationFailure(
                operation,
                teacherContext,
                QObject::tr("The engine database could not be opened.")
                )
            );
    }

    m_engineDatabase = std::move(*opened);
    m_engineDatabasePath = databasePath;
    return {};
}

Result<int> TeacherRepository::createTeacher(
    const Teacher& teacher
    )
{
    const QString operation = QObject::tr("Creating teacher");
    const QString identity = teacherIdentity(teacher);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineTeacherService service(*m_engineDatabase);
    const classmngr::engine::Result<int> created = service.create(
        toEngineTeacher(teacher)
        );
    if (!created)
    {
        return std::unexpected(
            engineFailure(operation, identity, created.error())
            );
    }

    return *created;
}

Result<int> TeacherRepository::saveTeacher(
    const Teacher& teacher
    )
{
    const QString operation = QObject::tr("Saving teacher");
    const QString identity = teacher.id > 0
        ? teacherIdentity(teacher.id)
        : teacherIdentity(teacher);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineTeacherService service(*m_engineDatabase);
    const classmngr::engine::Result<int> saved = service.save(
        toEngineTeacher(teacher)
        );
    if (!saved)
    {
        return std::unexpected(
            engineFailure(operation, identity, saved.error())
            );
    }

    return *saved;
}

Status TeacherRepository::updateTeacher(
    const Teacher& teacher
    )
{
    const QString operation = QObject::tr("Updating teacher");
    if (teacher.id <= 0)
    {
        return std::unexpected(invalidTeacherIdError(operation, teacher.id));
    }

    const QString identity = teacherIdentity(teacher.id);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineTeacherService service(*m_engineDatabase);
    const classmngr::engine::Status updated = service.update(
        toEngineTeacher(teacher)
        );
    if (!updated)
    {
        return std::unexpected(
            engineFailure(operation, identity, updated.error())
            );
    }

    return {};
}

Result<Teacher> TeacherRepository::getTeacher(
    int teacherId
    )
{
    const QString operation = QObject::tr("Loading teacher");
    if (teacherId <= 0)
    {
        return std::unexpected(invalidTeacherIdError(operation, teacherId));
    }

    const QString identity = teacherIdentity(teacherId);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineTeacherService service(*m_engineDatabase);
    const classmngr::engine::Result<EngineTeacher> loaded = service.get(
        teacherId
        );
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, identity, loaded.error())
            );
    }

    return fromEngineTeacher(*loaded);
}

Result<QList<Teacher>> TeacherRepository::getAllTeachers()
{
    const QString operation = QObject::tr("Loading teachers");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineTeacherService service(*m_engineDatabase);
    const classmngr::engine::Result<std::vector<EngineTeacher>> loaded =
        service.list();
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, {}, loaded.error())
            );
    }

    QList<Teacher> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineTeacher& teacher : *loaded)
    {
        result.append(fromEngineTeacher(teacher));
    }

    return result;
}

Status TeacherRepository::deleteTeacher(
    int teacherId
    )
{
    const QString operation = QObject::tr("Deleting teacher");
    if (teacherId <= 0)
    {
        return std::unexpected(invalidTeacherIdError(operation, teacherId));
    }

    const QString identity = teacherIdentity(teacherId);
    const Status engineReady = ensureEngineDatabase(operation, identity);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineTeacherService service(*m_engineDatabase);
    const classmngr::engine::Status deleted = service.remove(teacherId);
    if (!deleted)
    {
        return std::unexpected(
            engineFailure(operation, identity, deleted.error())
            );
    }

    return {};
}
