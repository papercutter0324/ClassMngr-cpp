#include "native_english_teacher_repository.h"

#include "classmngr/engine/native_english_teacher_service.h"
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
using EngineNativeEnglishTeacher =
    classmngr::engine::NativeEnglishTeacher;
using EngineNativeEnglishTeacherService =
    classmngr::engine::NativeEnglishTeacherService;

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

EngineNativeEnglishTeacher toEngineNativeEnglishTeacher(
    const NativeEnglishTeacher& source
    )
{
    EngineNativeEnglishTeacher result;
    result.id = source.id;
    result.name = toUtf8(source.name);
    result.position = toUtf8(source.position);
    result.phoneNumber = toUtf8(source.phoneNumber);
    result.birthday = toUtf8(source.birthday);
    result.nationality = toUtf8(source.nationality);
    result.email = toUtf8(source.email);
    return result;
}

NativeEnglishTeacher fromEngineNativeEnglishTeacher(
    const EngineNativeEnglishTeacher& source
    )
{
    NativeEnglishTeacher result;
    result.id = source.id;
    result.name = fromUtf8(source.name);
    result.position = fromUtf8(source.position);
    result.phoneNumber = fromUtf8(source.phoneNumber);
    result.birthday = fromUtf8(source.birthday);
    result.nationality = fromUtf8(source.nationality);
    result.email = fromUtf8(source.email);
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

NativeEnglishTeacherRepository::NativeEnglishTeacherRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

NativeEnglishTeacherRepository::~NativeEnglishTeacherRepository() = default;

Status NativeEnglishTeacherRepository::ensureEngineDatabase(
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

Result<QList<NativeEnglishTeacher>>
NativeEnglishTeacherRepository::getAll() const
{
    const QString operation =
        QObject::tr("Loading Native English Teacher directory");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineNativeEnglishTeacherService service(*m_engineDatabase);
    const classmngr::engine::Result<
        std::vector<EngineNativeEnglishTeacher>> loaded = service.list();
    if (!loaded)
    {
        return std::unexpected(engineFailure(operation, loaded.error()));
    }

    QList<NativeEnglishTeacher> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineNativeEnglishTeacher& teacher : *loaded)
    {
        result.append(fromEngineNativeEnglishTeacher(teacher));
    }

    return result;
}

Status NativeEnglishTeacherRepository::saveDirectory(
    const QList<NativeEnglishTeacher>& teachers,
    const QList<int>& deletedIds
    )
{
    const QString operation =
        QObject::tr("Saving Native English Teacher directory");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return engineReady;
    }

    std::vector<EngineNativeEnglishTeacher> engineTeachers;
    engineTeachers.reserve(static_cast<std::size_t>(teachers.size()));
    for (const NativeEnglishTeacher& teacher : teachers)
    {
        engineTeachers.push_back(toEngineNativeEnglishTeacher(teacher));
    }

    std::vector<int> engineDeletedIds;
    engineDeletedIds.reserve(static_cast<std::size_t>(deletedIds.size()));
    for (const int id : deletedIds)
    {
        engineDeletedIds.push_back(id);
    }

    EngineNativeEnglishTeacherService service(*m_engineDatabase);
    const classmngr::engine::Status saved = service.saveDirectory(
        engineTeachers,
        engineDeletedIds
        );
    if (!saved)
    {
        return std::unexpected(engineFailure(operation, saved.error()));
    }

    return {};
}
