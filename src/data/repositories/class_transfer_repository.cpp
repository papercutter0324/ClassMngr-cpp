#include "class_transfer_repository.h"

#include "core/platform/qt_platform_services.h"
#include "classmngr/engine/class_transfer_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/sqlite_database.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QTimeZone>

#include <chrono>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using EngineClassImportAction = classmngr::engine::ClassImportAction;
using EngineClassImportClassPreview =
    classmngr::engine::ClassImportClassPreview;
using EngineClassImportPlan = classmngr::engine::ClassImportPlan;
using EngineClassImportPreview = classmngr::engine::ClassImportPreview;
using EngineClassImportSummary = classmngr::engine::ClassImportSummary;
using EngineClassImportTeacherPreview =
    classmngr::engine::ClassImportTeacherPreview;
using EngineClassImportResolution = classmngr::engine::ClassImportResolution;
using EngineClassInfo = classmngr::engine::ClassInfo;
using EngineClassTime = classmngr::engine::ClassTime;
using EngineClassTransferClass = classmngr::engine::ClassTransferClass;
using EngineClassTransferEvaluation =
    classmngr::engine::ClassTransferEvaluation;
using EngineClassTransferPackage = classmngr::engine::ClassTransferPackage;
using EngineClassTransferService = classmngr::engine::ClassTransferService;
using EngineClassTransferTeacher = classmngr::engine::ClassTransferTeacher;
using EngineError = classmngr::engine::Error;
using EngineRoster = classmngr::engine::Roster;
using EngineTeacher = classmngr::engine::Teacher;
using EngineTeacherImportAction = classmngr::engine::TeacherImportAction;
using EngineTeacherImportResolution =
    classmngr::engine::TeacherImportResolution;

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

std::chrono::system_clock::time_point toEngineTimePoint(
    const QDateTime& source
    )
{
    if (!source.isValid())
    {
        // The retained Qt model permits an unset export timestamp and the
        // transfer service does not use it for validation or persistence.
        return {};
    }

    return std::chrono::system_clock::time_point(
        std::chrono::milliseconds(source.toMSecsSinceEpoch())
        );
}

std::optional<QDateTime> fromEngineTimePoint(
    const std::chrono::system_clock::time_point& source
    )
{
    const auto milliseconds = std::chrono::duration_cast<
        std::chrono::milliseconds>(source.time_since_epoch());
    const auto count = milliseconds.count();

    if (count < std::numeric_limits<qint64>::min()
        || count > std::numeric_limits<qint64>::max())
    {
        return std::nullopt;
    }

    const QDateTime result = QDateTime::fromMSecsSinceEpoch(
        static_cast<qint64>(count),
        QTimeZone::UTC
        );
    if (!result.isValid())
    {
        return std::nullopt;
    }

    return result;
}

EngineClassTime toEngineClassTime(
    const ClassTime& source
    )
{
    EngineClassTime result;
    result.day = toUtf8(source.day);
    result.startTime = toUtf8(source.startTime);
    result.endTime = toUtf8(source.endTime);
    return result;
}

ClassTime fromEngineClassTime(
    const EngineClassTime& source
    )
{
    ClassTime result;
    result.day = fromUtf8(source.day);
    result.startTime = fromUtf8(source.startTime);
    result.endTime = fromUtf8(source.endTime);
    return result;
}

std::vector<EngineClassTime> toEngineClassTimes(
    const QList<ClassTime>& source
    )
{
    std::vector<EngineClassTime> result;
    result.reserve(static_cast<std::size_t>(source.size()));
    for (const ClassTime& value : source)
    {
        result.push_back(toEngineClassTime(value));
    }
    return result;
}

QList<ClassTime> fromEngineClassTimes(
    const std::vector<EngineClassTime>& source
    )
{
    QList<ClassTime> result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const EngineClassTime& value : source)
    {
        result.append(fromEngineClassTime(value));
    }
    return result;
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
    result.classTimes = toEngineClassTimes(source.classTimes);
    result.intensiveTimes = toEngineClassTimes(source.intensiveTimes);
    result.notes = toUtf8(source.notes);
    result.timeFillerActivities = toUtf8(source.timeFillerActivities);
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
    result.classColor = fromUtf8(source.classColor);
    result.fontColor = fromUtf8(source.fontColor);
    result.classTimes = fromEngineClassTimes(source.classTimes);
    result.intensiveTimes = fromEngineClassTimes(source.intensiveTimes);
    result.notes = fromUtf8(source.notes);
    result.timeFillerActivities = fromUtf8(source.timeFillerActivities);
    return result;
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

EngineRoster toEngineRoster(
    const Roster& source
    )
{
    EngineRoster result;
    result.columns.reserve(static_cast<std::size_t>(source.columns.size()));
    for (const QString& value : source.columns)
    {
        result.columns.push_back(toUtf8(value));
    }

    result.columnWidths.reserve(
        static_cast<std::size_t>(source.columnWidths.size())
        );
    for (const int value : source.columnWidths)
    {
        result.columnWidths.push_back(value);
    }

    result.rows.reserve(static_cast<std::size_t>(source.rows.size()));
    for (const QStringList& sourceRow : source.rows)
    {
        std::vector<std::string> row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& value : sourceRow)
        {
            row.push_back(toUtf8(value));
        }
        result.rows.push_back(std::move(row));
    }

    return result;
}

Roster fromEngineRoster(
    const EngineRoster& source
    )
{
    Roster result;
    result.columns.reserve(static_cast<qsizetype>(source.columns.size()));
    for (const std::string& value : source.columns)
    {
        result.columns.append(fromUtf8(value));
    }

    result.columnWidths.reserve(
        static_cast<qsizetype>(source.columnWidths.size())
        );
    for (const int value : source.columnWidths)
    {
        result.columnWidths.append(value);
    }

    result.rows.reserve(static_cast<qsizetype>(source.rows.size()));
    for (const std::vector<std::string>& sourceRow : source.rows)
    {
        QStringList row;
        row.reserve(static_cast<qsizetype>(sourceRow.size()));
        for (const std::string& value : sourceRow)
        {
            row.append(fromUtf8(value));
        }
        result.rows.append(std::move(row));
    }

    return result;
}

EngineClassTransferEvaluation toEngineEvaluation(
    const ClassTransferEvaluation& source
    )
{
    EngineClassTransferEvaluation result;
    result.name = toUtf8(source.name);
    result.rows.reserve(static_cast<std::size_t>(source.rows.size()));
    for (const QStringList& sourceRow : source.rows)
    {
        std::vector<std::string> row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& value : sourceRow)
        {
            row.push_back(toUtf8(value));
        }
        result.rows.push_back(std::move(row));
    }
    return result;
}

ClassTransferEvaluation fromEngineEvaluation(
    const EngineClassTransferEvaluation& source
    )
{
    ClassTransferEvaluation result;
    result.name = fromUtf8(source.name);
    result.rows.reserve(static_cast<qsizetype>(source.rows.size()));
    for (const std::vector<std::string>& sourceRow : source.rows)
    {
        QStringList row;
        row.reserve(static_cast<qsizetype>(sourceRow.size()));
        for (const std::string& value : sourceRow)
        {
            row.append(fromUtf8(value));
        }
        result.rows.append(std::move(row));
    }
    return result;
}

std::optional<EngineClassTransferPackage> toEnginePackage(
    const ClassTransferPackage& source
    )
{
    EngineClassTransferPackage result;
    result.version = source.version;
    result.exportedAtUtc = toEngineTimePoint(source.exportedAtUtc);

    result.teachers.reserve(static_cast<std::size_t>(source.teachers.size()));
    for (const ClassTransferTeacher& sourceTeacher : source.teachers)
    {
        EngineClassTransferTeacher teacher;
        teacher.key = toUtf8(sourceTeacher.key);
        teacher.teacher = toEngineTeacher(sourceTeacher.teacher);
        result.teachers.push_back(std::move(teacher));
    }

    result.classes.reserve(static_cast<std::size_t>(source.classes.size()));
    for (const ClassTransferClass& sourceClass : source.classes)
    {
        EngineClassTransferClass transferClass;
        transferClass.key = toUtf8(sourceClass.key);
        transferClass.name = toUtf8(sourceClass.name);
        transferClass.teacherKey = toUtf8(sourceClass.teacherKey);
        transferClass.info = toEngineClassInfo(sourceClass.info);
        transferClass.roster = toEngineRoster(sourceClass.roster);
        transferClass.evaluations.reserve(
            static_cast<std::size_t>(sourceClass.evaluations.size())
            );
        for (const ClassTransferEvaluation& evaluation :
             sourceClass.evaluations)
        {
            transferClass.evaluations.push_back(toEngineEvaluation(evaluation));
        }
        result.classes.push_back(std::move(transferClass));
    }

    return result;
}

std::optional<ClassTransferPackage> fromEnginePackage(
    const EngineClassTransferPackage& source
    )
{
    const std::optional<QDateTime> exportedAtUtc = fromEngineTimePoint(
        source.exportedAtUtc
        );
    if (!exportedAtUtc)
    {
        return std::nullopt;
    }

    ClassTransferPackage result;
    result.version = source.version;
    result.exportedAtUtc = *exportedAtUtc;

    result.teachers.reserve(static_cast<qsizetype>(source.teachers.size()));
    for (const EngineClassTransferTeacher& sourceTeacher : source.teachers)
    {
        ClassTransferTeacher teacher;
        teacher.key = fromUtf8(sourceTeacher.key);
        teacher.teacher = fromEngineTeacher(sourceTeacher.teacher);
        result.teachers.append(std::move(teacher));
    }

    result.classes.reserve(static_cast<qsizetype>(source.classes.size()));
    for (const EngineClassTransferClass& sourceClass : source.classes)
    {
        ClassTransferClass transferClass;
        transferClass.key = fromUtf8(sourceClass.key);
        transferClass.name = fromUtf8(sourceClass.name);
        transferClass.teacherKey = fromUtf8(sourceClass.teacherKey);
        transferClass.info = fromEngineClassInfo(sourceClass.info);
        transferClass.roster = fromEngineRoster(sourceClass.roster);
        transferClass.evaluations.reserve(
            static_cast<qsizetype>(sourceClass.evaluations.size())
            );
        for (const EngineClassTransferEvaluation& evaluation :
             sourceClass.evaluations)
        {
            transferClass.evaluations.append(fromEngineEvaluation(evaluation));
        }
        result.classes.append(std::move(transferClass));
    }

    return result;
}

QList<int> fromEngineIds(
    const std::vector<int>& source
    )
{
    QList<int> result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const int value : source)
    {
        result.append(value);
    }
    return result;
}

ClassImportTeacherPreview fromEngineTeacherPreview(
    const EngineClassImportTeacherPreview& source
    )
{
    ClassImportTeacherPreview result;
    result.teacherKey = fromUtf8(source.teacherKey);
    result.matchingTeacherIds = fromEngineIds(source.matchingTeacherIds);
    return result;
}

ClassImportClassPreview fromEngineClassPreview(
    const EngineClassImportClassPreview& source
    )
{
    ClassImportClassPreview result;
    result.packageClassIndex = source.packageClassIndex;
    result.matchingClassIds = fromEngineIds(source.matchingClassIds);
    return result;
}

ClassImportPreview fromEnginePreview(
    const EngineClassImportPreview& source
    )
{
    ClassImportPreview result;
    result.teachers.reserve(static_cast<qsizetype>(source.teachers.size()));
    for (const EngineClassImportTeacherPreview& teacher : source.teachers)
    {
        result.teachers.append(fromEngineTeacherPreview(teacher));
    }

    result.classes.reserve(static_cast<qsizetype>(source.classes.size()));
    for (const EngineClassImportClassPreview& transferClass : source.classes)
    {
        result.classes.append(fromEngineClassPreview(transferClass));
    }
    return result;
}

std::optional<EngineClassImportAction> toEngineClassImportAction(
    ClassImportAction source
    )
{
    switch (source)
    {
    case ClassImportAction::Create:
        return EngineClassImportAction::Create;
    case ClassImportAction::Replace:
        return EngineClassImportAction::Replace;
    case ClassImportAction::Skip:
        return EngineClassImportAction::Skip;
    }

    return std::nullopt;
}

std::optional<EngineTeacherImportAction> toEngineTeacherImportAction(
    TeacherImportAction source
    )
{
    switch (source)
    {
    case TeacherImportAction::Create:
        return EngineTeacherImportAction::Create;
    case TeacherImportAction::KeepExisting:
        return EngineTeacherImportAction::KeepExisting;
    case TeacherImportAction::ReplaceExisting:
        return EngineTeacherImportAction::ReplaceExisting;
    }

    return std::nullopt;
}

std::optional<EngineClassImportPlan> toEnginePlan(
    const ClassImportPlan& source
    )
{
    EngineClassImportPlan result;
    result.classes.reserve(static_cast<std::size_t>(source.classes.size()));
    for (const ClassImportResolution& sourceResolution : source.classes)
    {
        const std::optional<EngineClassImportAction> action =
            toEngineClassImportAction(sourceResolution.action);
        if (!action)
        {
            return std::nullopt;
        }

        EngineClassImportResolution resolution;
        resolution.packageClassIndex = sourceResolution.packageClassIndex;
        resolution.action = *action;
        resolution.targetClassId = sourceResolution.targetClassId;
        result.classes.push_back(std::move(resolution));
    }

    result.teachers.reserve(static_cast<std::size_t>(source.teachers.size()));
    for (const TeacherImportResolution& sourceResolution : source.teachers)
    {
        const std::optional<EngineTeacherImportAction> action =
            toEngineTeacherImportAction(sourceResolution.action);
        if (!action)
        {
            return std::nullopt;
        }

        EngineTeacherImportResolution resolution;
        resolution.teacherKey = toUtf8(sourceResolution.teacherKey);
        resolution.action = *action;
        resolution.targetTeacherId = sourceResolution.targetTeacherId;
        result.teachers.push_back(std::move(resolution));
    }

    return result;
}

ClassImportSummary fromEngineSummary(
    const EngineClassImportSummary& source
    )
{
    ClassImportSummary result;
    result.createdClassIds = fromEngineIds(source.createdClassIds);
    result.replacedClassIds = fromEngineIds(source.replacedClassIds);
    result.skippedClassCount = source.skippedClassCount;
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

QString boundaryConversionFailure(
    const QString& operation
    )
{
    return operationFailure(
        operation,
        QObject::tr("The class transfer contains an unsupported or invalid value.")
        );
}
} // namespace

ClassTransferRepository::ClassTransferRepository(const QString& databasePath)
    : m_databasePath(databasePath)
{
}

ClassTransferRepository::ClassTransferRepository(
    QSqlDatabase& database
    )
    : ClassTransferRepository(database.databaseName())
{
    m_compatibilityDatabaseWasOpen = database.isValid() && database.isOpen();
}

ClassTransferRepository::~ClassTransferRepository() = default;

Status ClassTransferRepository::ensureEngineDatabase(
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

Result<ClassTransferPackage> ClassTransferRepository::buildPackage(
    const QList<int>& classIds
    )
{
    const QString operation = QObject::tr("Building class export package");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    std::vector<int> engineClassIds;
    engineClassIds.reserve(static_cast<std::size_t>(classIds.size()));
    for (const int classId : classIds)
    {
        engineClassIds.push_back(classId);
    }

    const classmngr::qt::QtClock clock;
    EngineClassTransferService service(*m_engineDatabase, clock);
    const classmngr::engine::Result<EngineClassTransferPackage> package =
        service.buildPackage(engineClassIds);
    if (!package)
    {
        return std::unexpected(engineFailure(operation, package.error()));
    }

    const std::optional<ClassTransferPackage> converted = fromEnginePackage(
        *package
        );
    if (!converted)
    {
        return std::unexpected(boundaryConversionFailure(operation));
    }
    return *converted;
}

Result<ClassImportPreview> ClassTransferRepository::previewImport(
    const ClassTransferPackage& package
    )
{
    const QString operation = QObject::tr("Previewing class import");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    const std::optional<EngineClassTransferPackage> enginePackage =
        toEnginePackage(package);
    if (!enginePackage)
    {
        return std::unexpected(boundaryConversionFailure(operation));
    }

    const classmngr::qt::QtClock clock;
    EngineClassTransferService service(*m_engineDatabase, clock);
    const classmngr::engine::Result<EngineClassImportPreview> preview =
        service.previewImport(*enginePackage);
    if (!preview)
    {
        return std::unexpected(engineFailure(operation, preview.error()));
    }

    return fromEnginePreview(*preview);
}

Result<ClassImportSummary> ClassTransferRepository::importClasses(
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    )
{
    const QString operation = QObject::tr("Importing classes");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    const std::optional<EngineClassTransferPackage> enginePackage =
        toEnginePackage(package);
    const std::optional<EngineClassImportPlan> enginePlan = toEnginePlan(plan);
    if (!enginePackage || !enginePlan)
    {
        return std::unexpected(boundaryConversionFailure(operation));
    }

    const classmngr::qt::QtClock clock;
    EngineClassTransferService service(*m_engineDatabase, clock);
    const classmngr::engine::Result<EngineClassImportSummary> imported =
        service.importClasses(*enginePackage, *enginePlan);
    if (!imported)
    {
        return std::unexpected(engineFailure(operation, imported.error()));
    }

    return fromEngineSummary(*imported);
}
