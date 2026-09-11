#include "schedule_import_repository.h"

#include "classmngr/engine/open_database.h"
#include "classmngr/engine/schedule_import_service.h"
#include "classmngr/engine/sqlite_database.h"

#include <QByteArray>
#include <QObject>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using EngineClassTime = classmngr::engine::ClassTime;
using EngineError = classmngr::engine::Error;
using EngineIntensiveSlotState = classmngr::engine::IntensiveSlotState;
using EngineScheduleImportClassAction =
    classmngr::engine::ScheduleImportClassAction;
using EngineScheduleImportClassCandidate =
    classmngr::engine::ScheduleImportClassCandidate;
using EngineScheduleImportClassMatchConfidence =
    classmngr::engine::ScheduleImportClassMatchConfidence;
using EngineScheduleImportClassPreview =
    classmngr::engine::ScheduleImportClassPreview;
using EngineScheduleImportClassResolution =
    classmngr::engine::ScheduleImportClassResolution;
using EngineScheduleImportDiagnostic =
    classmngr::engine::ScheduleImportDiagnostic;
using EngineScheduleImportIntensiveMode =
    classmngr::engine::ScheduleImportIntensiveMode;
using EngineScheduleImportKind = classmngr::engine::ScheduleImportKind;
using EngineScheduleImportPlan = classmngr::engine::ScheduleImportPlan;
using EngineScheduleImportPreview =
    classmngr::engine::ScheduleImportPreview;
using EngineScheduleImportService =
    classmngr::engine::ScheduleImportService;
using EngineScheduleImportSummary =
    classmngr::engine::ScheduleImportSummary;
using EngineScheduleImportTeacherAction =
    classmngr::engine::ScheduleImportTeacherAction;
using EngineScheduleImportTeacherPreview =
    classmngr::engine::ScheduleImportTeacherPreview;
using EngineScheduleImportTeacherResolution =
    classmngr::engine::ScheduleImportTeacherResolution;
using EngineScheduleImportUserBlock =
    classmngr::engine::ScheduleImportUserBlock;

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

std::vector<std::string> toEngineStrings(
    const QStringList& source
    )
{
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(source.size()));
    for (const QString& value : source)
    {
        result.push_back(toUtf8(value));
    }
    return result;
}

QStringList fromEngineStrings(
    const std::vector<std::string>& source
    )
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const std::string& value : source)
    {
        result.append(fromUtf8(value));
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

std::optional<EngineScheduleImportKind> toEngineKind(
    ScheduleImportKind source
    )
{
    switch (source)
    {
    case ScheduleImportKind::Normal:
        return EngineScheduleImportKind::Normal;
    case ScheduleImportKind::Intensive:
        return EngineScheduleImportKind::Intensive;
    }

    return std::nullopt;
}

std::optional<ScheduleImportKind> fromEngineKind(
    EngineScheduleImportKind source
    )
{
    switch (source)
    {
    case EngineScheduleImportKind::Normal:
        return ScheduleImportKind::Normal;
    case EngineScheduleImportKind::Intensive:
        return ScheduleImportKind::Intensive;
    }

    return std::nullopt;
}

std::optional<EngineScheduleImportIntensiveMode> toEngineIntensiveMode(
    ScheduleImportIntensiveMode source
    )
{
    switch (source)
    {
    case ScheduleImportIntensiveMode::UpdateExisting:
        return EngineScheduleImportIntensiveMode::UpdateExisting;
    case ScheduleImportIntensiveMode::ReplaceWithNew:
        return EngineScheduleImportIntensiveMode::ReplaceWithNew;
    }

    return std::nullopt;
}

std::optional<EngineScheduleImportTeacherAction> toEngineTeacherAction(
    ScheduleImportTeacherAction source
    )
{
    switch (source)
    {
    case ScheduleImportTeacherAction::Reuse:
        return EngineScheduleImportTeacherAction::Reuse;
    case ScheduleImportTeacherAction::UpdateRoom:
        return EngineScheduleImportTeacherAction::UpdateRoom;
    case ScheduleImportTeacherAction::Create:
        return EngineScheduleImportTeacherAction::Create;
    case ScheduleImportTeacherAction::Skip:
        return EngineScheduleImportTeacherAction::Skip;
    }

    return std::nullopt;
}

std::optional<EngineScheduleImportClassAction> toEngineClassAction(
    ScheduleImportClassAction source
    )
{
    switch (source)
    {
    case ScheduleImportClassAction::UpdateExisting:
        return EngineScheduleImportClassAction::UpdateExisting;
    case ScheduleImportClassAction::CreateNew:
        return EngineScheduleImportClassAction::CreateNew;
    case ScheduleImportClassAction::Skip:
        return EngineScheduleImportClassAction::Skip;
    }

    return std::nullopt;
}

std::optional<ScheduleImportClassMatchConfidence> fromEngineConfidence(
    EngineScheduleImportClassMatchConfidence source
    )
{
    switch (source)
    {
    case EngineScheduleImportClassMatchConfidence::None:
        return ScheduleImportClassMatchConfidence::None;
    case EngineScheduleImportClassMatchConfidence::Possible:
        return ScheduleImportClassMatchConfidence::Possible;
    case EngineScheduleImportClassMatchConfidence::Confident:
        return ScheduleImportClassMatchConfidence::Confident;
    }

    return std::nullopt;
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

EngineIntensiveSlotState toEngineIntensiveSlotState(
    const IntensiveSlotState& source
    )
{
    EngineIntensiveSlotState result;
    result.day = toUtf8(source.day);
    result.startTime = toUtf8(source.startTime);
    result.state = toUtf8(source.state);
    return result;
}

IntensiveSlotState fromEngineIntensiveSlotState(
    const EngineIntensiveSlotState& source
    )
{
    IntensiveSlotState result;
    result.day = fromUtf8(source.day);
    result.startTime = fromUtf8(source.startTime);
    result.state = fromUtf8(source.state);
    return result;
}

std::vector<EngineIntensiveSlotState> toEngineIntensiveSlotStates(
    const QList<IntensiveSlotState>& source
    )
{
    std::vector<EngineIntensiveSlotState> result;
    result.reserve(static_cast<std::size_t>(source.size()));
    for (const IntensiveSlotState& value : source)
    {
        result.push_back(toEngineIntensiveSlotState(value));
    }
    return result;
}

QList<IntensiveSlotState> fromEngineIntensiveSlotStates(
    const std::vector<EngineIntensiveSlotState>& source
    )
{
    QList<IntensiveSlotState> result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const EngineIntensiveSlotState& value : source)
    {
        result.append(fromEngineIntensiveSlotState(value));
    }
    return result;
}

EngineScheduleImportDiagnostic toEngineDiagnostic(
    const ScheduleImportDiagnostic& source
    )
{
    EngineScheduleImportDiagnostic result;
    result.sheetName = toUtf8(source.sheetName);
    result.userName = toUtf8(source.userName);
    result.cellReference = toUtf8(source.cellReference);
    result.value = toUtf8(source.value);
    result.message = toUtf8(source.message);
    return result;
}

ScheduleImportDiagnostic fromEngineDiagnostic(
    const EngineScheduleImportDiagnostic& source
    )
{
    ScheduleImportDiagnostic result;
    result.sheetName = fromUtf8(source.sheetName);
    result.userName = fromUtf8(source.userName);
    result.cellReference = fromUtf8(source.cellReference);
    result.value = fromUtf8(source.value);
    result.message = fromUtf8(source.message);
    return result;
}

EngineScheduleImportClassCandidate toEngineClassCandidate(
    const ScheduleImportClassCandidate& source
    )
{
    EngineScheduleImportClassCandidate result;
    result.teacherKey = toUtf8(source.teacherKey);
    result.teacherKr = toUtf8(source.teacherKr);
    result.rooms = toEngineStrings(source.rooms);
    result.importedColors = toEngineStrings(source.importedColors);
    result.classGrade = toUtf8(source.classGrade);
    result.classLevel = toUtf8(source.classLevel);
    result.times = toEngineClassTimes(source.times);
    result.sourceCells = toEngineStrings(source.sourceCells);
    result.meetingPatternError = toUtf8(source.meetingPatternError);
    return result;
}

ScheduleImportClassCandidate fromEngineClassCandidate(
    const EngineScheduleImportClassCandidate& source
    )
{
    ScheduleImportClassCandidate result;
    result.teacherKey = fromUtf8(source.teacherKey);
    result.teacherKr = fromUtf8(source.teacherKr);
    result.rooms = fromEngineStrings(source.rooms);
    result.importedColors = fromEngineStrings(source.importedColors);
    result.classGrade = fromUtf8(source.classGrade);
    result.classLevel = fromUtf8(source.classLevel);
    result.times = fromEngineClassTimes(source.times);
    result.sourceCells = fromEngineStrings(source.sourceCells);
    result.meetingPatternError = fromUtf8(source.meetingPatternError);
    return result;
}

EngineScheduleImportUserBlock toEngineUserBlock(
    const ScheduleImportUserBlock& source
    )
{
    EngineScheduleImportUserBlock result;
    result.name = toUtf8(source.name);
    result.headerCell = toUtf8(source.headerCell);
    result.classes.reserve(static_cast<std::size_t>(source.classes.size()));
    for (const ScheduleImportClassCandidate& value : source.classes)
    {
        result.classes.push_back(toEngineClassCandidate(value));
    }
    result.intensiveSlotStates = toEngineIntensiveSlotStates(
        source.intensiveSlotStates
        );
    result.diagnostics.reserve(
        static_cast<std::size_t>(source.diagnostics.size())
        );
    for (const ScheduleImportDiagnostic& value : source.diagnostics)
    {
        result.diagnostics.push_back(toEngineDiagnostic(value));
    }
    return result;
}

ScheduleImportUserBlock fromEngineUserBlock(
    const EngineScheduleImportUserBlock& source
    )
{
    ScheduleImportUserBlock result;
    result.name = fromUtf8(source.name);
    result.headerCell = fromUtf8(source.headerCell);
    result.classes.reserve(static_cast<qsizetype>(source.classes.size()));
    for (const EngineScheduleImportClassCandidate& value : source.classes)
    {
        result.classes.append(fromEngineClassCandidate(value));
    }
    result.intensiveSlotStates = fromEngineIntensiveSlotStates(
        source.intensiveSlotStates
        );
    result.diagnostics.reserve(
        static_cast<qsizetype>(source.diagnostics.size())
        );
    for (const EngineScheduleImportDiagnostic& value : source.diagnostics)
    {
        result.diagnostics.append(fromEngineDiagnostic(value));
    }
    return result;
}

std::optional<EngineScheduleImportPlan> toEnginePlan(
    const ScheduleImportPlan& source
    )
{
    const std::optional<EngineScheduleImportKind> kind = toEngineKind(
        source.kind
        );
    const std::optional<EngineScheduleImportIntensiveMode> intensiveMode =
        toEngineIntensiveMode(source.intensiveMode);
    if (!kind || !intensiveMode)
    {
        return std::nullopt;
    }

    EngineScheduleImportPlan result;
    result.kind = *kind;
    result.intensiveMode = *intensiveMode;
    result.selectedUserName = toUtf8(source.selectedUserName);
    result.saveProfileNameIfBlank = source.saveProfileNameIfBlank;
    result.updateProfileName = source.updateProfileName;
    result.unknownCellsAcknowledged = source.unknownCellsAcknowledged;

    result.candidates.reserve(
        static_cast<std::size_t>(source.candidates.size())
        );
    for (const ScheduleImportClassCandidate& value : source.candidates)
    {
        result.candidates.push_back(toEngineClassCandidate(value));
    }

    result.intensiveSlotStates = toEngineIntensiveSlotStates(
        source.intensiveSlotStates
        );

    result.diagnostics.reserve(
        static_cast<std::size_t>(source.diagnostics.size())
        );
    for (const ScheduleImportDiagnostic& value : source.diagnostics)
    {
        result.diagnostics.push_back(toEngineDiagnostic(value));
    }

    result.teachers.reserve(
        static_cast<std::size_t>(source.teachers.size())
        );
    for (const ScheduleImportTeacherResolution& value : source.teachers)
    {
        const std::optional<EngineScheduleImportTeacherAction> action =
            toEngineTeacherAction(value.action);
        if (!action)
        {
            return std::nullopt;
        }

        EngineScheduleImportTeacherResolution converted;
        converted.teacherKey = toUtf8(value.teacherKey);
        converted.action = *action;
        converted.targetTeacherId = value.targetTeacherId;
        converted.selectedRoom = toUtf8(value.selectedRoom);
        result.teachers.push_back(std::move(converted));
    }

    result.classes.reserve(
        static_cast<std::size_t>(source.classes.size())
        );
    for (const ScheduleImportClassResolution& value : source.classes)
    {
        const std::optional<EngineScheduleImportClassAction> action =
            toEngineClassAction(value.action);
        if (!action)
        {
            return std::nullopt;
        }

        EngineScheduleImportClassResolution converted;
        converted.candidateIndex = value.candidateIndex;
        converted.action = *action;
        converted.targetClassId = value.targetClassId;
        converted.classColor = toUtf8(value.classColor);
        converted.fontColor = toUtf8(value.fontColor);
        result.classes.push_back(std::move(converted));
    }

    return result;
}

std::optional<ScheduleImportClassPreview> fromEngineClassPreview(
    const EngineScheduleImportClassPreview& source
    )
{
    const std::optional<ScheduleImportClassMatchConfidence> confidence =
        fromEngineConfidence(source.matchConfidence);
    if (!confidence)
    {
        return std::nullopt;
    }

    ScheduleImportClassPreview result;
    result.candidateIndex = source.candidateIndex;
    result.matchingClassIds = fromEngineIds(source.matchingClassIds);
    result.suggestedClassId = source.suggestedClassId;
    result.exactMatch = source.exactMatch;
    result.matchConfidence = *confidence;
    result.matchExplanation = fromUtf8(source.matchExplanation);
    return result;
}

std::optional<ScheduleImportPreview> fromEnginePreview(
    const EngineScheduleImportPreview& source
    )
{
    const std::optional<ScheduleImportKind> kind = fromEngineKind(
        source.kind
        );
    if (!kind)
    {
        return std::nullopt;
    }

    ScheduleImportPreview result;
    result.kind = *kind;
    result.inventory.classCount = source.inventory.classCount;
    result.inventory.hasRegularHours = source.inventory.hasRegularHours;
    result.inventory.hasIntensiveHours = source.inventory.hasIntensiveHours;
    result.user = fromEngineUserBlock(source.user);

    result.teachers.reserve(static_cast<qsizetype>(source.teachers.size()));
    for (const EngineScheduleImportTeacherPreview& value : source.teachers)
    {
        ScheduleImportTeacherPreview converted;
        converted.teacherKey = fromUtf8(value.teacherKey);
        converted.teacherKr = fromUtf8(value.teacherKr);
        converted.importedRooms = fromEngineStrings(value.importedRooms);
        converted.matchingTeacherIds = fromEngineIds(
            value.matchingTeacherIds
            );
        converted.affectedClassCount = value.affectedClassCount;
        result.teachers.append(std::move(converted));
    }

    result.classes.reserve(static_cast<qsizetype>(source.classes.size()));
    for (const EngineScheduleImportClassPreview& value : source.classes)
    {
        const std::optional<ScheduleImportClassPreview> converted =
            fromEngineClassPreview(value);
        if (!converted)
        {
            return std::nullopt;
        }
        result.classes.append(*converted);
    }

    result.initiallyAbsentClassIds = fromEngineIds(
        source.initiallyAbsentClassIds
        );
    return result;
}

ScheduleImportSummary fromEngineSummary(
    const EngineScheduleImportSummary& source
    )
{
    ScheduleImportSummary result;
    result.teachersCreated = source.teachersCreated;
    result.teachersUpdated = source.teachersUpdated;
    result.classesCreated = source.classesCreated;
    result.classesUpdated = source.classesUpdated;
    result.classesSkipped = source.classesSkipped;
    result.schedulesCleared = source.schedulesCleared;
    result.ignoredCells = source.ignoredCells;
    result.profileNameUpdated = source.profileNameUpdated;
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
        QObject::tr("The schedule import contains an unsupported value.")
        );
}
} // namespace

ScheduleImportRepository::ScheduleImportRepository(const QString& databasePath)
    : m_databasePath(databasePath)
{
}

ScheduleImportRepository::ScheduleImportRepository(
    QSqlDatabase& database
    )
    : ScheduleImportRepository(database.databaseName())
{
    m_compatibilityDatabaseWasOpen = database.isValid() && database.isOpen();
}

ScheduleImportRepository::~ScheduleImportRepository() = default;

Status ScheduleImportRepository::ensureEngineDatabase(
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

Result<ScheduleImportPreview> ScheduleImportRepository::preview(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind
    )
{
    const QString operation = QObject::tr("Previewing schedule import");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    const std::optional<EngineScheduleImportKind> engineKind = toEngineKind(
        kind
        );
    if (!engineKind)
    {
        return std::unexpected(boundaryConversionFailure(operation));
    }

    const EngineScheduleImportUserBlock engineUser = toEngineUserBlock(user);
    EngineScheduleImportService service(*m_engineDatabase);
    const classmngr::engine::Result<EngineScheduleImportPreview> preview =
        service.previewImport(engineUser, *engineKind);
    if (!preview)
    {
        return std::unexpected(engineFailure(operation, preview.error()));
    }

    const std::optional<ScheduleImportPreview> converted = fromEnginePreview(
        *preview
        );
    if (!converted)
    {
        return std::unexpected(boundaryConversionFailure(operation));
    }
    return *converted;
}

Status ScheduleImportRepository::validateImport(
    const ScheduleImportPlan& plan
    )
{
    const QString operation = QObject::tr("Validating schedule import");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    const std::optional<EngineScheduleImportPlan> enginePlan = toEnginePlan(
        plan
        );
    if (!enginePlan)
    {
        return std::unexpected(boundaryConversionFailure(operation));
    }

    EngineScheduleImportService service(*m_engineDatabase);
    const classmngr::engine::Status validated = service.validateImport(
        *enginePlan
        );
    if (!validated)
    {
        return std::unexpected(engineFailure(operation, validated.error()));
    }

    return {};
}

Result<ScheduleImportSummary> ScheduleImportRepository::apply(
    const ScheduleImportPlan& plan
    )
{
    const QString operation = QObject::tr("Applying schedule import");
    const Status engineReady = ensureEngineDatabase(operation);
    if (!engineReady)
    {
        return std::unexpected(engineReady.error());
    }

    const std::optional<EngineScheduleImportPlan> enginePlan = toEnginePlan(
        plan
        );
    if (!enginePlan)
    {
        return std::unexpected(boundaryConversionFailure(operation));
    }

    EngineScheduleImportService service(*m_engineDatabase);
    const classmngr::engine::Result<EngineScheduleImportSummary> imported =
        service.importSchedule(*enginePlan);
    if (!imported)
    {
        return std::unexpected(engineFailure(operation, imported.error()));
    }

    return fromEngineSummary(*imported);
}
