#include "class_info_repository.h"

#include "classmngr/engine/class_info_service.h"
#include "classmngr/engine/class_schedule_service.h"
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
using EngineClassConflict = classmngr::engine::ClassConflict;
using EngineClassInfo = classmngr::engine::ClassInfo;
using EngineClassInfoService = classmngr::engine::ClassInfoService;
using EngineClassScheduleService = classmngr::engine::ClassScheduleService;
using EngineClassTeacherAssignment =
    classmngr::engine::ClassTeacherAssignment;
using EngineClassTime = classmngr::engine::ClassTime;
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

EngineClassTime toEngineClassTime(
    const ClassTime& source
    )
{
    return {
        toUtf8(source.day),
        toUtf8(source.startTime),
        toUtf8(source.endTime)
    };
}

ClassTime fromEngineClassTime(
    const EngineClassTime& source
    )
{
    return {
        fromUtf8(source.day),
        fromUtf8(source.startTime),
        fromUtf8(source.endTime)
    };
}

EngineClassInfo toEngineClassInfo(
    const ClassInfo& source
    )
{
    EngineClassInfo result;
    result.classId = source.classId;
    result.teacherId = source.teacherId;
    result.teacherKr = toUtf8(source.teacherKr);
    result.teacherEn = toUtf8(source.teacherEn);
    result.teacherPreferredName = toUtf8(source.teacherPreferredName);
    result.roomNumber = toUtf8(source.roomNumber);
    result.wifiName = toUtf8(source.wifiName);
    result.wifiPassword = toUtf8(source.wifiPassword);
    result.internetType = toUtf8(source.internetType);
    result.zoomId = toUtf8(source.zoomId);
    result.zoomPassword = toUtf8(source.zoomPassword);
    result.projectionType = toUtf8(source.projectionType);
    result.classGrade = toUtf8(source.classGrade);
    result.classLevel = toUtf8(source.classLevel);
    result.readingBook = toUtf8(source.readingBook);
    result.essayBook = toUtf8(source.essayBook);
    result.classColor = toUtf8(source.classColor);
    result.fontColor = toUtf8(source.fontColor);
    result.notes = toUtf8(source.notes);
    result.timeFillerActivities = toUtf8(source.timeFillerActivities);

    result.classTimes.reserve(
        static_cast<std::size_t>(source.classTimes.size())
        );
    for (const ClassTime& time : source.classTimes)
    {
        result.classTimes.push_back(toEngineClassTime(time));
    }

    result.intensiveTimes.reserve(
        static_cast<std::size_t>(source.intensiveTimes.size())
        );
    for (const ClassTime& time : source.intensiveTimes)
    {
        result.intensiveTimes.push_back(toEngineClassTime(time));
    }

    return result;
}

ClassInfo fromEngineClassInfo(
    const EngineClassInfo& source
    )
{
    ClassInfo result;
    result.classId = source.classId;
    result.teacherId = source.teacherId;
    result.teacherKr = fromUtf8(source.teacherKr);
    result.teacherEn = fromUtf8(source.teacherEn);
    result.teacherPreferredName = fromUtf8(source.teacherPreferredName);
    result.roomNumber = fromUtf8(source.roomNumber);
    result.wifiName = fromUtf8(source.wifiName);
    result.wifiPassword = fromUtf8(source.wifiPassword);
    result.internetType = fromUtf8(source.internetType);
    result.zoomId = fromUtf8(source.zoomId);
    result.zoomPassword = fromUtf8(source.zoomPassword);
    result.projectionType = fromUtf8(source.projectionType);
    result.classGrade = fromUtf8(source.classGrade);
    result.classLevel = fromUtf8(source.classLevel);
    result.readingBook = fromUtf8(source.readingBook);
    result.essayBook = fromUtf8(source.essayBook);

    const QString classColor = fromUtf8(source.classColor);
    if (!classColor.isEmpty())
    {
        result.classColor = classColor;
    }

    const QString fontColor = fromUtf8(source.fontColor);
    if (!fontColor.isEmpty())
    {
        result.fontColor = fontColor;
    }

    result.notes = fromUtf8(source.notes);
    result.timeFillerActivities = fromUtf8(source.timeFillerActivities);

    result.classTimes.reserve(
        static_cast<qsizetype>(source.classTimes.size())
        );
    for (const EngineClassTime& time : source.classTimes)
    {
        result.classTimes.append(fromEngineClassTime(time));
    }

    result.intensiveTimes.reserve(
        static_cast<qsizetype>(source.intensiveTimes.size())
        );
    for (const EngineClassTime& time : source.intensiveTimes)
    {
        result.intensiveTimes.append(fromEngineClassTime(time));
    }

    return result;
}

const char* operationPrefix(
    const QString& detail
    )
{
    constexpr const char* operations[] = {
        {"Saving class information"},
        {"Saving class notes"},
        {"Loading class information"},
        {"Loading regular class times"},
        {"Loading intensive class times"},
        {"Loading class teacher assignments"},
        {"Loading schedule class information"},
        {"Loading schedule class times"},
        {"Loading class time conflicts"},
        {"Starting class information save transaction"},
        {"Committing class information"},
        {"Deleting regular class times"},
        {"Inserting regular class time"},
        {"Deleting intensive class times"},
        {"Inserting intensive class time"}
    };

    for (const char* operation : operations)
    {
        if (detail.startsWith(QString::fromLatin1(operation)))
        {
            return operation;
        }
    }

    return nullptr;
}

QString localizedOperation(
    const QString& detail,
    const QString& fallback
    )
{
    const char* source = operationPrefix(detail);
    return source == nullptr ? fallback : QObject::tr(source);
}

QString engineFailure(
    const QString& fallbackOperation,
    int classId,
    const EngineError& error
    )
{
    const QString detail = fromUtf8(error.message);
    const char* source = operationPrefix(detail);
    QString remaining = detail;
    QString context;
    if (source != nullptr)
    {
        remaining = detail.mid(
            static_cast<qsizetype>(std::char_traits<char>::length(source))
            ).trimmed();
        const qsizetype separator = remaining.indexOf(QStringLiteral(": "));
        if (remaining.startsWith(QStringLiteral("for class id "))
            && separator >= 0)
        {
            context = remaining.left(separator);
            remaining = remaining.mid(separator + 2).trimmed();
        }
    }

    QString message = QObject::tr("%1 failed")
        .arg(localizedOperation(detail, fallbackOperation));

    if (!context.isEmpty())
    {
        message += QStringLiteral(" ") + context;
    }
    else if (classId > 0 && !detail.contains(QStringLiteral("class id ")))
    {
        message += QObject::tr(" for class id %1").arg(classId);
    }

    if (!remaining.isEmpty())
    {
        message += QStringLiteral(": ") + remaining;
    }

    return message;
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
} // namespace

ClassInfoRepository::ClassInfoRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

ClassInfoRepository::~ClassInfoRepository() = default;

Status ClassInfoRepository::ensureEngineDatabase(
    const QString& operation,
    int classId
    )
{
    if (!m_database.isValid() || !m_database.isOpen())
    {
        QString message = QObject::tr("%1 failed").arg(operation);
        if (classId > 0)
        {
            message += QObject::tr(" for class id %1").arg(classId);
        }
        message += QObject::tr(": No Teacher Profile is open.");
        return std::unexpected(message);
    }

    const QString databasePath = m_database.databaseName();
    if (databasePath.trimmed().isEmpty())
    {
        QString message = QObject::tr("%1 failed").arg(operation);
        if (classId > 0)
        {
            message += QObject::tr(" for class id %1").arg(classId);
        }
        message += QObject::tr(": No database path is available.");
        return std::unexpected(message);
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
    if (!opened || *opened == nullptr)
    {
        if (!opened)
        {
            return std::unexpected(
                engineFailure(operation, classId, opened.error())
                );
        }

        QString message = QObject::tr("%1 failed").arg(operation);
        if (classId > 0)
        {
            message += QObject::tr(" for class id %1").arg(classId);
        }
        message += QObject::tr(
            ": The engine database could not be opened."
            );
        return std::unexpected(message);
    }

    m_engineDatabase = std::move(*opened);
    m_engineDatabasePath = databasePath;
    return {};
}

Status ClassInfoRepository::saveClassInfo(
    const ClassInfo& info
    )
{
    const QString operation = QObject::tr("Saving class information");
    if (info.classId <= 0)
    {
        return std::unexpected(invalidClassIdError(operation, info.classId));
    }

    const Status engineReady = ensureEngineDatabase(operation, info.classId);
    if (!engineReady)
    {
        return engineReady;
    }

    EngineClassInfoService service(*m_engineDatabase);
    const classmngr::engine::Status saved = service.save(
        toEngineClassInfo(info)
        );
    if (!saved)
    {
        return std::unexpected(
            engineFailure(operation, info.classId, saved.error())
            );
    }

    return {};
}

Status ClassInfoRepository::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    const QString operation = QObject::tr("Saving class notes");
    if (classId <= 0)
    {
        return std::unexpected(invalidClassIdError(operation, classId));
    }

    const Status engineReady = ensureEngineDatabase(operation, classId);
    if (!engineReady)
    {
        return engineReady;
    }

    const std::string encodedNotes = toUtf8(notes);
    const std::string encodedActivities = toUtf8(timeFillerActivities);
    EngineClassInfoService service(*m_engineDatabase);
    const classmngr::engine::Status saved = service.saveNotes(
        classId,
        encodedNotes,
        encodedActivities
        );
    if (!saved)
    {
        return std::unexpected(
            engineFailure(operation, classId, saved.error())
            );
    }

    return {};
}

Result<ClassInfo> ClassInfoRepository::loadClassInfo(
    int classId
    )
{
    const QString operation = QObject::tr("Loading class information");
    if (classId <= 0)
    {
        return std::unexpected(invalidClassIdError(operation, classId));
    }

    const Status engineReady = ensureEngineDatabase(operation, classId);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineClassInfoService service(*m_engineDatabase);
    const classmngr::engine::Result<EngineClassInfo> loaded = service.load(
        classId
        );
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, classId, loaded.error())
            );
    }

    return fromEngineClassInfo(*loaded);
}

Result<QList<ClassTeacherAssignment>>
ClassInfoRepository::loadClassTeacherAssignments()
{
    const QString operation =
        QObject::tr("Loading class teacher assignments");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineClassScheduleService service(*m_engineDatabase);
    const classmngr::engine::Result<
        std::vector<EngineClassTeacherAssignment>> loaded =
        service.loadClassTeacherAssignments();
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, -1, loaded.error())
            );
    }

    QList<ClassTeacherAssignment> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineClassTeacherAssignment& assignment : *loaded)
    {
        result.append({assignment.classId, assignment.teacherId});
    }

    return result;
}

Result<QList<ClassInfo>> ClassInfoRepository::loadScheduleClassInfos()
{
    const QString operation =
        QObject::tr("Loading schedule class information");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    EngineClassScheduleService service(*m_engineDatabase);
    const classmngr::engine::Result<std::vector<EngineClassInfo>> loaded =
        service.loadScheduleClassInfos();
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, -1, loaded.error())
            );
    }

    QList<ClassInfo> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineClassInfo& info : *loaded)
    {
        result.append(fromEngineClassInfo(info));
    }

    return result;
}

Result<QList<ClassConflict>> ClassInfoRepository::getClassTimeConflicts(
    int classId,
    const QList<ClassTime>& times,
    ScheduleType type
    )
{
    const QString operation = QObject::tr("Loading class time conflicts");
    if (classId <= 0)
    {
        return std::unexpected(invalidClassIdError(operation, classId));
    }

    const Status engineReady = ensureEngineDatabase(operation, classId);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    std::vector<EngineClassTime> engineTimes;
    engineTimes.reserve(static_cast<std::size_t>(times.size()));
    for (const ClassTime& time : times)
    {
        engineTimes.push_back(toEngineClassTime(time));
    }

    const classmngr::engine::ScheduleType engineType =
        type == ScheduleType::Regular
            ? classmngr::engine::ScheduleType::Regular
            : classmngr::engine::ScheduleType::Intensive;

    EngineClassScheduleService service(*m_engineDatabase);
    const classmngr::engine::Result<std::vector<EngineClassConflict>> loaded =
        service.getClassTimeConflicts(classId, engineTimes, engineType);
    if (!loaded)
    {
        return std::unexpected(
            engineFailure(operation, classId, loaded.error())
            );
    }

    QList<ClassConflict> result;
    result.reserve(static_cast<qsizetype>(loaded->size()));
    for (const EngineClassConflict& conflict : *loaded)
    {
        result.append({
            conflict.classId,
            fromUtf8(conflict.className),
            fromUtf8(conflict.day),
            fromUtf8(conflict.startTime),
            fromUtf8(conflict.endTime),
            fromUtf8(conflict.conflictingClassName)
        });
    }

    return result;
}
