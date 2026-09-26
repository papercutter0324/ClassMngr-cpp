#include "schedule_import_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/teacher_repository.h"
#include "core/startup_profiler.h"
#include "features/classes/config/class_info_config.h"
#include "features/schedule/services/schedule_import_plan_validator.h"
#include "features/teacher/import/teacher_import_name_utils.h"
#include "next/application/schedule_import_matching_projection.h"
#include "next/application/schedule_import_state_validation.h"
#include "next/domain/schedule_entry.h"
#include "next/domain/schedule_time.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTime>

#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace Domain = ClassMngr::Next::Domain;

namespace
{
QString teacherKey(
    const QString& value
    )
{
    return TeacherImportNameUtils::hangulOnly(value);
}

std::vector<ScheduleImportMatchingTime> matchingTimes(
    const QList<ClassTime>& times
    )
{
    std::vector<ScheduleImportMatchingTime> result;
    result.reserve(static_cast<std::size_t>(times.size()));
    for (const ClassTime& time : times)
    {
        result.push_back({time.day.toStdString()});
    }
    return result;
}

std::u16string matchingKey(const QString& value)
{
    return value.simplified().toCaseFolded().toStdU16String();
}

Domain::TeacherId teacherDomainId(const int legacyId)
{
    return Domain::TeacherId::fromString(std::to_string(legacyId)).value();
}

Domain::ClassId classDomainId(const int legacyId)
{
    return Domain::ClassId::fromString(std::to_string(legacyId)).value();
}

template <typename Id>
std::optional<int> legacyId(const Id& id)
{
    const std::string& value = id.value();
    int result = 0;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        result
        );
    if (error != std::errc{} || end != value.data() + value.size())
    {
        return std::nullopt;
    }
    return result;
}

ScheduleImportMatchingInput matchingInput(
    const ScheduleImportUserBlock& user,
    const ScheduleImportKind kind,
    const QList<Teacher>& teachers,
    const QList<Classroom>& classrooms,
    const QHash<int, ClassInfo>& classInfo
    )
{
    ScheduleImportMatchingInput result;
    result.kind = kind == ScheduleImportKind::Intensive
        ? ScheduleImportMatchingKind::Intensive
        : ScheduleImportMatchingKind::Normal;
    result.candidates.reserve(static_cast<std::size_t>(user.classes.size()));
    for (const ScheduleImportClassCandidate& candidate : user.classes)
    {
        ScheduleImportMatchingCandidate projected;
        projected.teacherKey = candidate.teacherKey.toStdU16String();
        projected.teacherName = candidate.teacherKr.toStdU16String();
        projected.gradeMatchKey = matchingKey(candidate.classGrade);
        projected.levelMatchKey = matchingKey(candidate.classLevel);
        projected.rooms.reserve(static_cast<std::size_t>(candidate.rooms.size()));
        projected.roomMatchKeys.reserve(
            static_cast<std::size_t>(candidate.rooms.size())
            );
        for (const QString& room : candidate.rooms)
        {
            projected.rooms.push_back(room.toStdU16String());
            projected.roomMatchKeys.push_back(matchingKey(room));
        }
        projected.times = matchingTimes(candidate.times);
        result.candidates.push_back(std::move(projected));
    }

    result.teachers.reserve(static_cast<std::size_t>(teachers.size()));
    for (const Teacher& teacher : teachers)
    {
        result.teachers.push_back({
            teacherDomainId(teacher.id),
            teacher.teacherKr.toStdU16String()
        });
    }

    result.classes.reserve(static_cast<std::size_t>(classrooms.size()));
    for (const Classroom& classroom : classrooms)
    {
        const ClassInfo info = classInfo.value(classroom.id);
        ScheduleImportMatchingClass projected{
            classDomainId(classroom.id),
            teacherDomainId(info.teacherId),
            classroom.id > 0,
            matchingKey(info.roomNumber),
            matchingKey(info.classGrade),
            matchingKey(info.classLevel),
            matchingTimes(info.classTimes),
            matchingTimes(info.intensiveTimes)
        };
        result.classes.push_back(std::move(projected));
    }
    return result;
}

QString matchExplanation(
    const ScheduleImportMatchingExplanation explanation
    )
{
    switch (explanation)
    {
    case ScheduleImportMatchingExplanation::Exact:
        return QObject::tr(
            "One existing class matches the imported grade, level, Korean teacher, room, and meeting days."
            );
    case ScheduleImportMatchingExplanation::PossibleWithTargetHours:
        return QObject::tr(
            "Possible existing classes share the imported grade and level and have a compatible weekday group."
            );
    case ScheduleImportMatchingExplanation::PossibleWithOtherHours:
        return QObject::tr(
            "Possible existing classes have hours only in the other schedule type; their grade, level, and weekday group are compatible."
            );
    case ScheduleImportMatchingExplanation::PossibleWithoutHours:
        return QObject::tr(
            "Possible existing classes share the imported grade and level but have no schedule hours to compare."
            );
    case ScheduleImportMatchingExplanation::None:
        return QObject::tr(
            "No existing class has the same grade and level with a compatible weekday group."
            );
    }
    return {};
}

QString queryFailure(
    const QSqlQuery& query,
    const QString& action
    )
{
    return SqlQueryUtils::errorFor(query, action).userMessage();
}

int dayIndex(
    const QString& day
    )
{
    static const QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
    return days.indexOf(day);
}

std::string utf8String(
    const QString& value
    )
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(
        utf8.constData(),
        static_cast<std::size_t>(utf8.size())
        );
}

QString qString(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

QString normalizedIdentity(
    const QString& value
    )
{
    return value.simplified().toCaseFolded();
}

int timeMinutes(
    const QString& value
    )
{
    for (const QString& format : {
             QStringLiteral("h:mm AP"), QStringLiteral("h:mmAP"),
             QStringLiteral("H:mm"), QStringLiteral("HH:mm")
             })
    {
        const QTime time = QTime::fromString(value.trimmed(), format);
        if (time.isValid())
        {
            return time.hour() * 60 + time.minute();
        }
    }
    return -1;
}

struct PersistedScheduleEntry final
{
    Domain::ScheduleEntry fact;
    // Keep the legacy SQL representation at this adapter edge verbatim.
    ClassTime sourceTime;
};

std::optional<std::vector<PersistedScheduleEntry>> resolvedScheduleEntries(
    const QHash<int, QList<ClassTime>>& finalTimes
    )
{
    std::vector<PersistedScheduleEntry> result;
    for (auto iterator = finalTimes.cbegin();
         iterator != finalTimes.cend();
         ++iterator)
    {
        const int legacyClassId = iterator.key();
        if (legacyClassId <= 0)
        {
            return std::nullopt;
        }

        const auto classId = Domain::ClassId::fromString(
            std::to_string(legacyClassId)
            );
        if (!classId)
        {
            return std::nullopt;
        }

        for (const ClassTime& sourceTime : iterator.value())
        {
            const auto scheduleTime = Domain::ScheduleTime::fromMinutes(
                dayIndex(sourceTime.day),
                timeMinutes(sourceTime.startTime),
                timeMinutes(sourceTime.endTime)
                );
            if (!scheduleTime)
            {
                return std::nullopt;
            }

            result.push_back(
                {
                    Domain::ScheduleEntry(*classId, *scheduleTime),
                    sourceTime
                }
                );
        }
    }
    return result;
}

ScheduleImportStateTime stateTime(
    const ClassTime& time
    )
{
    return {
        dayIndex(time.day),
        timeMinutes(time.startTime),
        timeMinutes(time.endTime),
        utf8String(time.day),
        utf8String(time.startTime),
        utf8String(time.endTime)
    };
}

std::vector<ScheduleImportStateTime> stateTimes(
    const QList<ClassTime>& times
    )
{
    std::vector<ScheduleImportStateTime> result;
    result.reserve(static_cast<std::size_t>(times.size()));
    for (const ClassTime& time : times)
    {
        result.push_back(stateTime(time));
    }
    return result;
}

ScheduleImportStateValidationRequest stateValidationRequest(
    const ScheduleImportPlan& plan,
    const ValidatedScheduleImportPlan& validatedPlan,
    const QList<Teacher>& existingTeachers,
    const QList<Classroom>& existingClasses,
    const QHash<int, ClassInfo>& existingInfo
    )
{
    ScheduleImportStateValidationRequest request;
    request.kind = plan.kind == ScheduleImportKind::Intensive
        ? ScheduleImportStateKind::Intensive
        : ScheduleImportStateKind::Normal;
    request.intensiveMode = plan.intensiveMode
            == ScheduleImportIntensiveMode::ReplaceWithNew
        ? ScheduleImportStateIntensiveMode::ReplaceWithNew
        : ScheduleImportStateIntensiveMode::UpdateExisting;

    request.candidates.reserve(static_cast<std::size_t>(plan.candidates.size()));
    for (const ScheduleImportClassCandidate& candidate : plan.candidates)
    {
        const QString label = QStringLiteral("%1 %2")
            .arg(candidate.classGrade, candidate.classLevel);
        request.candidates.push_back(
            {
                utf8String(candidate.teacherKey),
                utf8String(normalizedIdentity(candidate.classGrade)),
                utf8String(normalizedIdentity(candidate.classLevel)),
                utf8String(label),
                stateTimes(candidate.times)
            }
            );
    }

    request.teacherResolutions.reserve(
        static_cast<std::size_t>(validatedPlan.teacherResolutions.size())
        );
    for (auto iterator = validatedPlan.teacherResolutions.cbegin();
         iterator != validatedPlan.teacherResolutions.cend();
         ++iterator)
    {
        const auto& resolution = iterator.value();
        ScheduleImportStateTeacherAction action =
            ScheduleImportStateTeacherAction::Create;
        switch (resolution.action)
        {
        case ScheduleImportTeacherAction::Reuse:
            action = ScheduleImportStateTeacherAction::Reuse;
            break;
        case ScheduleImportTeacherAction::UpdateRoom:
            action = ScheduleImportStateTeacherAction::UpdateRoom;
            break;
        case ScheduleImportTeacherAction::Create:
            action = ScheduleImportStateTeacherAction::Create;
            break;
        case ScheduleImportTeacherAction::Skip:
            action = ScheduleImportStateTeacherAction::Skip;
            break;
        }
        // Reuse and room-update actions always name a target, even when the
        // legacy value is nonpositive. Create and skip use nonpositive values
        // as their no-target sentinel; positive values remain visible to the
        // Application validator so it can preserve the existing error.
        std::optional<Domain::TeacherId> targetTeacherId;
        if (action == ScheduleImportStateTeacherAction::Reuse
            || action == ScheduleImportStateTeacherAction::UpdateRoom
            || resolution.targetTeacherId > 0)
        {
            targetTeacherId = teacherDomainId(resolution.targetTeacherId);
        }
        request.teacherResolutions.push_back(
            {
                utf8String(resolution.teacherKey),
                action,
                std::move(targetTeacherId),
                !resolution.selectedRoom.trimmed().isEmpty()
            }
            );
    }

    request.classResolutions.reserve(
        static_cast<std::size_t>(validatedPlan.classResolutions.size())
        );
    for (int index = 0; index < plan.candidates.size(); ++index)
    {
        const auto resolution = validatedPlan.classResolutions.value(index);
        ScheduleImportStateClassAction action =
            ScheduleImportStateClassAction::CreateNew;
        switch (resolution.action)
        {
        case ScheduleImportClassAction::UpdateExisting:
            action = ScheduleImportStateClassAction::UpdateExisting;
            break;
        case ScheduleImportClassAction::CreateNew:
            action = ScheduleImportStateClassAction::CreateNew;
            break;
        case ScheduleImportClassAction::Skip:
            action = ScheduleImportStateClassAction::Skip;
            break;
        }
        // Updating an existing class always carries the selected legacy ID.
        // Create-new and skip use nonpositive values to mean no selected
        // target; positive values are retained for the existing validation.
        std::optional<Domain::ClassId> targetClassId;
        if (action == ScheduleImportStateClassAction::UpdateExisting
            || resolution.targetClassId > 0)
        {
            targetClassId = classDomainId(resolution.targetClassId);
        }
        request.classResolutions.push_back(
            {
                static_cast<std::size_t>(index),
                action,
                std::move(targetClassId)
            }
            );
    }

    request.existingTeachers.reserve(
        static_cast<std::size_t>(existingTeachers.size())
        );
    for (const Teacher& teacher : existingTeachers)
    {
        request.existingTeachers.push_back(
            {
                teacherDomainId(teacher.id),
                utf8String(teacherKey(teacher.teacherKr))
            }
            );
    }

    request.existingClasses.reserve(
        static_cast<std::size_t>(existingClasses.size())
        );
    for (const Classroom& classroom : existingClasses)
    {
        const ClassInfo info = existingInfo.value(classroom.id);
        const QString label = QStringLiteral("%1 %2")
            .arg(info.classGrade, info.classLevel)
            .simplified();
        request.existingClasses.push_back(
            {
                classDomainId(classroom.id),
                teacherDomainId(info.teacherId),
                utf8String(normalizedIdentity(info.classGrade)),
                utf8String(normalizedIdentity(info.classLevel)),
                utf8String(label),
                stateTimes(info.classTimes),
                stateTimes(info.intensiveTimes)
            }
            );
    }
    return request;
}

QString stateValidationMessage(
    const ScheduleImportStateValidationError& error
    )
{
    switch (error.code)
    {
    case ScheduleImportStateValidationErrorCode::SelectedTeacherUnavailable:
        return QObject::tr("A selected Korean teacher is no longer available.");
    case ScheduleImportStateValidationErrorCode::InvalidTeacherTarget:
        return QObject::tr(
            "A created or skipped Korean teacher cannot have an existing target."
            );
    case ScheduleImportStateValidationErrorCode::MissingTeacherRoom:
        return QObject::tr("Choose a room before updating a Korean teacher.");
    case ScheduleImportStateValidationErrorCode::SelectedClassUnavailable:
        return QObject::tr("A selected class is no longer available.");
    case ScheduleImportStateValidationErrorCode::CreateNewClassHasTarget:
        return QObject::tr(
            "A class created as new cannot have an existing target."
            );
    case ScheduleImportStateValidationErrorCode::SkippedClassNotUniqueExactMatch:
        return QObject::tr(
            "A skipped imported class can preserve only its unique exact existing match."
            );
    case ScheduleImportStateValidationErrorCode::InvalidProjectedTime:
        return QObject::tr("%1 contains an invalid time: %2 %3–%4")
            .arg(
                qString(error.classLabel),
                qString(error.day),
                qString(error.startTime),
                qString(error.endTime)
                );
    case ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap:
        return QObject::tr(
            "The proposed schedule overlaps: %1 conflicts with %2 on %3."
            )
            .arg(
                qString(error.classLabel),
                qString(error.conflictingClassLabel),
                qString(error.day)
                );
    }
    return QObject::tr("The proposed schedule is invalid.");
}

QString normalizedHexColor(
    const QString& value
    )
{
    static const QRegularExpression expression(
        QStringLiteral("^#[0-9A-Fa-f]{6}$")
        );
    const QString color =
        value.trimmed();
    return expression.match(color).hasMatch()
        ? color.toUpper()
        : QString();
}

QList<ClassTime> selectedTimes(
    const ClassInfo& info,
    ScheduleImportKind kind
    )
{
    return kind == ScheduleImportKind::Intensive
        ? info.intensiveTimes
        : info.classTimes;
}

Status writeScheduleEntries(
    QSqlDatabase& database,
    const QString& table,
    const std::vector<PersistedScheduleEntry>& entries
    )
{
    QSqlQuery query(database);
    query.prepare(
        QStringLiteral(
            "INSERT INTO %1 "
            "(class_id, day, start_time, end_time) "
            "VALUES (?, ?, ?, ?)"
            )
            .arg(table)
        );

    for (const PersistedScheduleEntry& entry : entries)
    {
        bool classIdOk = false;
        const int classId = QString::fromStdString(
            entry.fact.classId().value()
            ).toInt(&classIdOk);
        const ClassTime& sourceTime = entry.sourceTime;
        const Domain::ScheduleTime& scheduleTime =
            entry.fact.scheduleTime();
        if (
            !classIdOk
            || classId <= 0
            || dayIndex(sourceTime.day) != scheduleTime.weekdayIndex()
            || timeMinutes(sourceTime.startTime)
                != scheduleTime.startMinute()
            || timeMinutes(sourceTime.endTime)
                != scheduleTime.endMinute()
            )
        {
            return std::unexpected(
                QObject::tr(
                    "A resolved schedule entry cannot be persisted."
                    )
                );
        }

        query.bindValue(0, classId);
        query.bindValue(1, sourceTime.day);
        query.bindValue(2, sourceTime.startTime);
        query.bindValue(3, sourceTime.endTime);

        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Writing imported class times")
                    )
                );
        }
    }

    return {};
}

Status writeIntensiveSlotStates(
    QSqlDatabase& database,
    const QList<IntensiveSlotState>& states
    )
{
    static const QSet<QString> validStates{
        QStringLiteral("empty"),
        QStringLiteral("essay"),
        QStringLiteral("lunch")
    };

    QSet<QString> keys;
    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO intensive_slot_states (day, start_time, state)
        VALUES (?, ?, ?)
    )");

    for (const IntensiveSlotState& state : states)
    {
        const int day = dayIndex(state.day);
        const QTime startTime =
            QTime::fromString(
                state.startTime,
                QStringLiteral("HH:mm")
                );
        const QString key =
            state.day + QLatin1Char('\x1f') + state.startTime;
        if (
            day < 0
            || !startTime.isValid()
            || !validStates.contains(state.state)
            || keys.contains(key)
            )
        {
            return std::unexpected(
                QObject::tr("The import contains an invalid intensive slot state.")
                );
        }
        keys.insert(key);

        query.bindValue(0, state.day);
        query.bindValue(1, state.startTime);
        query.bindValue(2, state.state);
        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Writing imported intensive slot states")
                    )
                );
        }
    }

    return {};
}
}

ScheduleImportRepository::ScheduleImportRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<ScheduleImportPreview> ScheduleImportRepository::preview(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(
            QObject::tr("No Teacher Profile is open.")
            );
    }

    TeacherRepository teacherRepository(m_database);
    ClassRepository classRepository(m_database);
    ClassInfoRepository classInfoRepository(m_database);
    const Result<QList<Teacher>> teachers =
        teacherRepository.getAllTeachers();
    if (!teachers)
    {
        return std::unexpected(teachers.error());
    }

    const Result<QList<Classroom>> classrooms =
        classRepository.getClasses();
    if (!classrooms)
    {
        return std::unexpected(classrooms.error());
    }

    QHash<int, ClassInfo> classInfo;
    for (const Classroom& classroom : *classrooms)
    {
        const Result<ClassInfo> info =
            classInfoRepository.loadClassInfo(classroom.id);
        if (!info)
        {
            return std::unexpected(info.error());
        }

        classInfo.insert(classroom.id, *info);
    }

    const ScheduleImportMatchingProjection projection =
        projectScheduleImportMatching(
            matchingInput(
                user,
                kind,
                *teachers,
                *classrooms,
                classInfo
                )
            );

    ScheduleImportPreview result;
    result.kind = kind;
    result.user = user;
    result.inventory.classCount =
        static_cast<int>(projection.inventory.classCount);
    result.inventory.hasRegularHours =
        projection.inventory.hasRegularHours;
    result.inventory.hasIntensiveHours =
        projection.inventory.hasIntensiveHours;
    for (const ScheduleImportMatchingTeacherProjection& teacher :
         projection.teachers)
    {
        ScheduleImportTeacherPreview projected;
        projected.teacherKey = QString::fromStdU16String(teacher.teacherKey);
        projected.teacherKr = QString::fromStdU16String(teacher.teacherName);
        projected.matchingTeacherIds.reserve(
            static_cast<qsizetype>(teacher.matchingTeacherIds.size())
            );
        for (const Domain::TeacherId& teacherId : teacher.matchingTeacherIds)
        {
            if (const auto value = legacyId(teacherId))
            {
                projected.matchingTeacherIds.append(*value);
            }
        }
        projected.importedRooms.reserve(
            static_cast<qsizetype>(teacher.importedRooms.size())
            );
        for (const std::u16string& room : teacher.importedRooms)
        {
            projected.importedRooms.append(
                QString::fromStdU16String(room)
                );
        }
        projected.affectedClassCount =
            static_cast<int>(teacher.affectedClassCount);
        result.teachers.append(std::move(projected));
    }
    for (const ScheduleImportMatchingClassProjection& value :
         projection.classes)
    {
        ScheduleImportClassPreview projected;
        projected.candidateIndex = static_cast<int>(value.candidateIndex);
        projected.matchingClassIds.reserve(
            static_cast<qsizetype>(value.matchingClassIds.size())
            );
        for (const Domain::ClassId& classId : value.matchingClassIds)
        {
            if (const auto legacy = legacyId(classId))
            {
                projected.matchingClassIds.append(*legacy);
            }
        }
        if (value.suggestedClassId)
        {
            projected.suggestedClassId =
                legacyId(*value.suggestedClassId).value_or(-1);
        }
        projected.exactMatch = value.exactMatch;
        switch (value.confidence)
        {
        case ScheduleImportMatchingConfidence::None:
            projected.matchConfidence =
                ScheduleImportClassMatchConfidence::None;
            break;
        case ScheduleImportMatchingConfidence::Possible:
            projected.matchConfidence =
                ScheduleImportClassMatchConfidence::Possible;
            break;
        case ScheduleImportMatchingConfidence::Confident:
            projected.matchConfidence =
                ScheduleImportClassMatchConfidence::Confident;
            break;
        }
        projected.matchExplanation = matchExplanation(value.explanation);
        result.classes.append(std::move(projected));
    }
    result.initiallyAbsentClassIds.reserve(
        static_cast<qsizetype>(projection.initiallyAbsentClassIds.size())
        );
    for (const Domain::ClassId& classId : projection.initiallyAbsentClassIds)
    {
        if (const auto legacy = legacyId(classId))
        {
            result.initiallyAbsentClassIds.append(*legacy);
        }
    }
    return result;
}

Result<ScheduleImportSummary> ScheduleImportRepository::apply(
    const ScheduleImportPlan& plan
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(
            QObject::tr("No Teacher Profile is open.")
            );
    }

    const Result<ValidatedScheduleImportPlan> validatedPlan =
        ScheduleImportPlanValidator::validate(plan);
    if (!validatedPlan)
    {
        return std::unexpected(validatedPlan.error());
    }
    const auto& teacherResolutions =
        validatedPlan->teacherResolutions;
    const auto& classResolutions =
        validatedPlan->classResolutions;

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr(
                "Unable to start the schedule import transaction."
                )
            );
    }

    TeacherRepository teacherRepository(m_database);
    ClassRepository classRepository(m_database);
    ClassInfoRepository classInfoRepository(m_database);
    const Result<QList<Teacher>> existingTeachers =
        teacherRepository.getAllTeachers();
    if (!existingTeachers)
    {
        return std::unexpected(existingTeachers.error());
    }

    const Result<QList<Classroom>> existingClasses =
        classRepository.getClasses();
    if (!existingClasses)
    {
        return std::unexpected(existingClasses.error());
    }

    QHash<int, ClassInfo> existingInfo;
    for (const Classroom& classroom : *existingClasses)
    {
        const Result<ClassInfo> info =
            classInfoRepository.loadClassInfo(classroom.id);
        if (!info)
        {
            return std::unexpected(info.error());
        }

        existingInfo.insert(classroom.id, *info);
    }

    StartupProfiler::recordScheduleImportApplyInputs(
        existingTeachers->size(),
        existingClasses->size(),
        existingInfo.size()
        );

    const ScheduleImportStateValidationRequest currentRequest =
        stateValidationRequest(
            plan,
            *validatedPlan,
            *existingTeachers,
            *existingClasses,
            existingInfo
            );
    const auto projectedScheduleRows =
        projectScheduleImportStateSchedules(currentRequest);
    const auto currentState = validateScheduleImportState(
        currentRequest,
        projectedScheduleRows
        );
    if (currentState)
    {
        return std::unexpected(stateValidationMessage(*currentState));
    }

    ScheduleImportSummary summary;
    summary.ignoredCells =
        plan.diagnostics.size();
    QHash<QString, int> resolvedTeacherIds;
    QSqlQuery query(m_database);

    for (
        auto iterator = teacherResolutions.cbegin();
        iterator != teacherResolutions.cend();
        ++iterator
        )
    {
        const ScheduleImportTeacherResolution& resolution =
            iterator.value();

        if (
            resolution.action
                == ScheduleImportTeacherAction::Skip
            )
        {
            resolvedTeacherIds.insert(iterator.key(), -1);
            continue;
        }

        if (
            resolution.action
                == ScheduleImportTeacherAction::Create
            )
        {
            QString teacherName;
            for (const ScheduleImportClassCandidate& candidate : plan.candidates)
            {
                if (candidate.teacherKey == iterator.key())
                {
                    teacherName = candidate.teacherKr;
                    break;
                }
            }
            teacherName =
                teacherKey(teacherName);

            query.prepare(R"(
                INSERT INTO teachers (
                    teacher_kr,
                    room_number
                )
                VALUES (?, ?)
            )");
            query.addBindValue(teacherName);
            query.addBindValue(
                resolution.selectedRoom.trimmed()
                );

            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Creating a Korean teacher")
                        )
                    );
            }

            const int teacherId =
                query.lastInsertId().toInt();
            if (teacherId <= 0)
            {
                return std::unexpected(
                    QObject::tr(
                        "A Korean teacher could not be created."
                        )
                    );
            }
            resolvedTeacherIds.insert(iterator.key(), teacherId);
            ++summary.teachersCreated;
            continue;
        }

        resolvedTeacherIds.insert(
            iterator.key(),
            resolution.targetTeacherId
            );

        if (
            resolution.action
                == ScheduleImportTeacherAction::UpdateRoom
            )
        {
            query.prepare(R"(
                UPDATE teachers
                SET room_number=?
                WHERE id=?
            )");
            query.addBindValue(
                resolution.selectedRoom.trimmed()
                );
            query.addBindValue(resolution.targetTeacherId);

            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Updating a Korean teacher room")
                        )
                    );
            }
            ++summary.teachersUpdated;
        }
    }

    const QString timeTable =
        plan.kind == ScheduleImportKind::Intensive
            ? QStringLiteral("class_intensive_times")
            : QStringLiteral("class_times");
    const bool preservesAbsentIntensiveClasses =
        plan.kind == ScheduleImportKind::Intensive
        && plan.intensiveMode
            == ScheduleImportIntensiveMode::UpdateExisting;
    std::vector<std::optional<int>> insertedClassIdsByCandidate(
        static_cast<std::size_t>(plan.candidates.size())
        );
    QHash<int, QList<ClassTime>> finalTimes;
    for (int index = 0; index < plan.candidates.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate =
            plan.candidates[index];
        const ScheduleImportClassResolution resolution =
            classResolutions.value(index);

        if (
            resolution.action
                == ScheduleImportClassAction::Skip
            )
        {
            ++summary.classesSkipped;
            continue;
        }

        const int teacherId =
            resolvedTeacherIds.value(
                candidate.teacherKey,
                -1
                );
        if (teacherId <= 0)
        {
            return std::unexpected(
                QObject::tr(
                    "A class cannot be imported because its Korean teacher was skipped."
                    )
                );
        }

        int classId =
            resolution.targetClassId;

        if (
            resolution.action
                == ScheduleImportClassAction::CreateNew
            )
        {
            query.prepare(
                QStringLiteral(
                    "INSERT INTO classes (name) VALUES (?)"
                    )
                );
            query.addBindValue(
                QStringLiteral("%1 %2")
                    .arg(
                        candidate.classGrade,
                        candidate.classLevel
                        )
                    .simplified()
                );
            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Creating a class")
                        )
                    );
            }
            classId =
                query.lastInsertId().toInt();
            if (classId <= 0)
            {
                return std::unexpected(
                    QObject::tr("A class could not be created.")
                    );
            }
            insertedClassIdsByCandidate[
                static_cast<std::size_t>(index)
                ] = classId;
            ++summary.classesCreated;
        }
        else
        {
            ++summary.classesUpdated;
        }

        query.prepare(R"(
            INSERT INTO class_info (
                class_id,
                teacher_id,
                class_grade,
                class_level,
                class_color,
                font_color
            )
            VALUES (?, ?, ?, ?, ?, ?)
            ON CONFLICT(class_id)
            DO UPDATE SET
                teacher_id=excluded.teacher_id,
                class_grade=excluded.class_grade,
                class_level=excluded.class_level,
                class_color=excluded.class_color,
                font_color=excluded.font_color
        )");
        query.addBindValue(classId);
        query.addBindValue(teacherId);
        query.addBindValue(candidate.classGrade);
        query.addBindValue(candidate.classLevel);
        query.addBindValue(
            normalizedHexColor(
                resolution.classColor
                )
            );
        query.addBindValue(
            normalizedHexColor(
                resolution.fontColor
                )
            );

        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Updating imported class information")
                    )
                );
        }
    }

    // Resolve the typed Application projection only after CreateNew rows have
    // received their database IDs. This same projection was used above for
    // app-less overlap validation and remains the sole source of persisted
    // schedule-row selection.
    for (const ScheduleImportStateProjectedSchedule& projectedSchedule :
         projectedScheduleRows)
    {
        if (projectedSchedule.persistence
            == ScheduleImportStateProjectedSchedule::Persistence::
                KeepExistingRows)
        {
            continue;
        }

        int classId = -1;
        if (const auto* existingId = std::get_if<Domain::ClassId>(
                &projectedSchedule.classReference
                ))
        {
            classId = legacyId(*existingId).value_or(-1);
        }
        else
        {
            const auto candidateIndex = std::get<
                ScheduleImportStateCandidateIndex
                >(projectedSchedule.classReference).value;
            if (candidateIndex < insertedClassIdsByCandidate.size()
                && insertedClassIdsByCandidate[candidateIndex])
            {
                classId = *insertedClassIdsByCandidate[candidateIndex];
            }
        }

        if (classId <= 0)
        {
            return std::unexpected(
                QObject::tr(
                    "A projected schedule has no persisted class target."
                    )
                );
        }

        QList<ClassTime> times;
        for (const ScheduleImportStateTime& time : projectedSchedule.times)
        {
            times.push_back(
                {
                    qString(time.dayLabel),
                    qString(time.startLabel),
                    qString(time.endLabel)
                }
                );
        }
        finalTimes.insert(classId, std::move(times));
    }

    int finalScheduleRowCount = 0;
    for (const QList<ClassTime>& times : finalTimes)
    {
        finalScheduleRowCount += times.size();
    }
    StartupProfiler::recordScheduleImportApplyPrepared(
        finalTimes.size(),
        finalScheduleRowCount,
        teacherResolutions.size(),
        classResolutions.size()
        );

    const auto scheduleEntries = resolvedScheduleEntries(finalTimes);
    if (!scheduleEntries)
    {
        return std::unexpected(
            QObject::tr(
                "A resolved schedule entry has an invalid class or time."
                )
            );
    }

    if (!preservesAbsentIntensiveClasses)
    {
        for (const Classroom& classroom : *existingClasses)
        {
            const bool hadTimes =
                !selectedTimes(
                    existingInfo.value(classroom.id),
                    plan.kind
                    ).isEmpty();
            if (
                hadTimes
                && !finalTimes.contains(classroom.id)
                )
            {
                ++summary.schedulesCleared;
            }
        }
    }

    if (preservesAbsentIntensiveClasses)
    {
        query.prepare(
            QStringLiteral(
                "DELETE FROM %1 WHERE class_id=?"
                )
                .arg(timeTable)
            );
        for (
            auto iterator = finalTimes.cbegin();
            iterator != finalTimes.cend();
            ++iterator
            )
        {
            query.bindValue(0, iterator.key());
            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr(
                            "Clearing an existing intensive class schedule"
                            )
                        )
                    );
            }
        }
    }
    else if (
        !query.exec(
            QStringLiteral("DELETE FROM %1")
                .arg(timeTable)
            )
        )
    {
        return std::unexpected(
            queryFailure(
                query,
                QObject::tr("Clearing the previous schedule snapshot")
                )
            );
    }

    const Status written = writeScheduleEntries(
        m_database,
        timeTable,
        *scheduleEntries
        );
    if (!written)
    {
        return std::unexpected(written.error());
    }

    if (plan.kind == ScheduleImportKind::Intensive)
    {
        if (!query.exec(QStringLiteral("DELETE FROM intensive_slot_states")))
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Clearing the previous intensive slot states")
                    )
                );
        }

        const Status statesWritten =
            writeIntensiveSlotStates(
                m_database,
                plan.intensiveSlotStates
                );
        if (!statesWritten)
        {
            return std::unexpected(statesWritten.error());
        }
    }

    if (
        plan.saveProfileNameIfBlank
        || plan.updateProfileName
        )
    {
        query.prepare(
            QStringLiteral(
                "SELECT value FROM app_settings WHERE key='myInfo/name'"
                )
            );
        if (!query.exec())
        {
            return std::unexpected(
                queryFailure(
                    query,
                    QObject::tr("Reading My Information name")
                    )
                );
        }

        QString existingName;
        if (query.next())
        {
            existingName =
                query.value(0).toString().trimmed();
        }

        if (
            (
                existingName.isEmpty()
                || plan.updateProfileName
                )
            && !plan.selectedUserName.trimmed().isEmpty()
            )
        {
            query.prepare(R"(
                INSERT INTO app_settings (key, value)
                VALUES ('myInfo/name', ?)
                ON CONFLICT(key)
                DO UPDATE SET value=excluded.value
            )");
            query.addBindValue(
                plan.selectedUserName.trimmed()
                );
            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(
                        query,
                        QObject::tr("Saving My Information name")
                        )
                    );
            }
            summary.profileNameUpdated = true;
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr(
                "Unable to commit the schedule import transaction: %1"
                )
                .arg(m_database.lastError().text())
            );
    }

    return summary;
}
