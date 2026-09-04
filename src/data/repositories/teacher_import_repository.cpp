#include "teacher_import_repository.h"

#include "classmngr/engine/open_database.h"
#include "classmngr/engine/sqlite_database.h"
#include "classmngr/engine/teacher_import_service.h"

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
using EngineNativeEnglishTeacher =
    classmngr::engine::NativeEnglishTeacher;
using EngineTeacher = classmngr::engine::Teacher;
using EngineTeacherImportPlan = classmngr::engine::TeacherImportPlan;
using EngineTeacherImportService =
    classmngr::engine::TeacherImportService;
using EngineTeacherImportSummary =
    classmngr::engine::TeacherImportSummary;

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

EngineNativeEnglishTeacher toEngineNativeEnglishTeacher(
    const NativeEnglishTeacher& source
    )
{
    return {
        source.id,
        toUtf8(source.name),
        toUtf8(source.position),
        toUtf8(source.phoneNumber),
        toUtf8(source.birthday),
        toUtf8(source.nationality),
        toUtf8(source.email)
    };
}

EngineGsTeamMember toEngineGsTeamMember(
    const GsTeamMember& source
    )
{
    return {
        source.id,
        toUtf8(source.name),
        toUtf8(source.koreanName),
        toUtf8(source.position),
        toUtf8(source.phoneNumber),
        toUtf8(source.birthday)
    };
}

EngineTeacherImportPlan toEnginePlan(
    const TeacherImportPlan& source
    )
{
    EngineTeacherImportPlan result;
    result.templateId = toUtf8(source.templateId);
    result.sourceDate = toUtf8(source.sourceDate.toString(Qt::ISODate));

    result.koreanTeachers.reserve(
        static_cast<std::size_t>(source.koreanTeachers.size())
        );
    for (const Teacher& teacher : source.koreanTeachers)
    {
        result.koreanTeachers.push_back(toEngineTeacher(teacher));
    }

    result.nativeEnglishTeachers.reserve(
        static_cast<std::size_t>(source.nativeEnglishTeachers.size())
        );
    for (const NativeEnglishTeacher& teacher : source.nativeEnglishTeachers)
    {
        result.nativeEnglishTeachers.push_back(
            toEngineNativeEnglishTeacher(teacher)
            );
    }

    result.gsTeamMembers.reserve(
        static_cast<std::size_t>(source.gsTeamMembers.size())
        );
    for (const GsTeamMember& member : source.gsTeamMembers)
    {
        result.gsTeamMembers.push_back(toEngineGsTeamMember(member));
    }

    return result;
}

TeacherImportCounts fromEngineCounts(
    const classmngr::engine::TeacherImportCounts& source
    )
{
    return {
        source.created,
        source.updated,
        source.unchanged
    };
}

TeacherImportSummary fromEngineSummary(
    const EngineTeacherImportSummary& source
    )
{
    return {
        fromEngineCounts(source.koreanTeachers),
        fromEngineCounts(source.nativeEnglishTeachers),
        fromEngineCounts(source.gsTeamMembers)
    };
}

QString operationFailure(
    const QString& operation,
    const QString& detail = {}
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
} // namespace

TeacherImportRepository::TeacherImportRepository(const QString& databasePath)
    : m_databasePath(databasePath)
{
}

TeacherImportRepository::TeacherImportRepository(
    QSqlDatabase& database
    )
    : TeacherImportRepository(database.databaseName())
{
    m_compatibilityDatabaseWasOpen = database.isValid() && database.isOpen();
}

TeacherImportRepository::~TeacherImportRepository() = default;

Status TeacherImportRepository::ensureEngineDatabase(
    const QString& operation
    )
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

Result<TeacherImportSummary> TeacherImportRepository::importTeachers(
    const TeacherImportPlan& plan
    )
{
    const QString operation = QObject::tr("Importing teachers");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineTeacherImportService service(*m_engineDatabase);
    const classmngr::engine::Result<EngineTeacherImportSummary> imported =
        service.importTeachers(toEnginePlan(plan));
    if (!imported)
    {
        return std::unexpected(engineFailure(operation, imported.error()));
    }

    return fromEngineSummary(*imported);
}
