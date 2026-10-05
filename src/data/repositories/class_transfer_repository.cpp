#include "class_transfer_repository.h"
#include "next/platform/class_transfer_apply_legacy_adapter.h"

#include "core/result.h"
#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/classroom.h"
#include "next/application/class_transfer_matching_policy.h"
#include "next/application/class_transfer_projection.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
struct ScheduledTime
{
    QString classLabel;
    ClassTime time;
    ClassMngr::Next::Application::ClassTransferScheduleCandidate candidate;
};

struct ValidatedPlan
{
    QHash<int, ClassImportResolution> classes;
    QHash<QString, TeacherImportResolution> teachers;
};

QString normalized(const QString& value)
{
    return value.simplified().toCaseFolded();
}

template <typename TypedId>
TypedId applicationId(const int legacyId)
{
    return *TypedId::fromString(std::to_string(legacyId));
}

QString reviewIssueMessage(
    const ClassMngr::Next::Application::ClassTransferReviewDecisionIssueCode code
    )
{
    using IssueCode =
        ClassMngr::Next::Application::ClassTransferReviewDecisionIssueCode;
    switch (code)
    {
    case IssueCode::InvalidClassAction:
    case IssueCode::InvalidClassIndex:
    case IssueCode::DuplicateClassResolution:
    case IssueCode::UnknownClassResolution:
        return QObject::tr(
            "The class import plan contains an invalid or duplicate class entry.");
    case IssueCode::MissingClassResolution:
        return QObject::tr("Every package class must have an import action.");
    case IssueCode::ReplaceClassMissingTarget:
    case IssueCode::ClassTargetNotInMatchSet:
        return QObject::tr(
            "A replacement class is not one of the inferred matches.");
    case IssueCode::NonReplaceClassHasTarget:
        return QObject::tr(
            "Only replacement actions may specify a destination class.");
    case IssueCode::DuplicateClassReplacementTarget:
        return QObject::tr(
            "Two package classes cannot replace the same destination class.");
    case IssueCode::InvalidTeacherAction:
    case IssueCode::EmptyTeacherKey:
    case IssueCode::DuplicateTeacherResolution:
    case IssueCode::UnknownTeacherResolution:
        return QObject::tr(
            "The teacher import plan contains an invalid or duplicate teacher entry.");
    case IssueCode::MissingTeacherResolution:
        return QObject::tr("Every package teacher must have an import action.");
    case IssueCode::UniqueTeacherCannotBeCreated:
    case IssueCode::CreateTeacherHasTarget:
        return QObject::tr(
            "An unambiguous teacher match must reuse the local teacher.");
    case IssueCode::TeacherActionMissingTarget:
    case IssueCode::TeacherTargetNotInMatchSet:
        return QObject::tr(
            "A selected teacher is not one of the inferred matches.");
    case IssueCode::DuplicateTeacherReplacementTarget:
        return QObject::tr(
            "Two different package teachers cannot replace the same local teacher.");
    }
    return QObject::tr("The class import choices are invalid.");
}

QList<ClassTransferEvaluation> loadEvaluations(
    QSqlDatabase& database,
    int classId,
    QString* errorMessage
    )
{
    QList<ClassTransferEvaluation> evaluations;
    QHash<int, int> evaluationIndexById;
    QSqlQuery evaluationQuery(database);

    evaluationQuery.prepare(R"(
        SELECT id, evaluation_name
        FROM speaking_evaluations
        WHERE class_id=?
        ORDER BY id
    )");
    evaluationQuery.addBindValue(classId);

    if (!evaluationQuery.exec())
    {
        *errorMessage = QObject::tr("Unable to read speaking evaluations: %1")
            .arg(evaluationQuery.lastError().text());
        return {};
    }

    while (evaluationQuery.next())
    {
        const int evaluationId = evaluationQuery.value("id").toInt();
        ClassTransferEvaluation evaluation;
        evaluation.name = evaluationQuery.value("evaluation_name").toString();
        evaluation.rows = SpeakingEval::emptyRows();
        evaluationIndexById.insert(evaluationId, evaluations.size());
        evaluations.append(evaluation);
    }

    if (evaluations.isEmpty())
    {
        return evaluations;
    }

    QSqlQuery rowQuery(database);
    rowQuery.prepare(R"(
        SELECT evaluation_id, row_index,
            col_0, col_1, col_2, col_3, col_4, col_5,
            col_6, col_7, col_8, col_9, col_10
        FROM speaking_eval_data
        WHERE evaluation_id IN (
            SELECT id
            FROM speaking_evaluations
            WHERE class_id=?
        )
        ORDER BY evaluation_id, row_index
    )");
    rowQuery.addBindValue(classId);

    if (!rowQuery.exec())
    {
        *errorMessage = QObject::tr("Unable to read speaking evaluation rows: %1")
            .arg(rowQuery.lastError().text());
        return {};
    }

    while (rowQuery.next())
    {
        const auto evaluationIndex = evaluationIndexById.constFind(
            rowQuery.value("evaluation_id").toInt());
        const int rowIndex = rowQuery.value("row_index").toInt();

        if (evaluationIndex == evaluationIndexById.cend()
            || rowIndex < 0 || rowIndex >= SpeakingEval::RowCount)
        {
            continue;
        }

        ClassTransferEvaluation& evaluation = evaluations[*evaluationIndex];
        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            evaluation.rows[rowIndex][column] =
                rowQuery.value(column + 2).toString();
        }
    }

    return evaluations;
}

QString transferClassLabel(
    const ClassTransferClass& transferClass
    )
{
    const QString course = QStringList{
        transferClass.info.classGrade.trimmed(),
        transferClass.info.classLevel.trimmed()
        }.join(QStringLiteral(" ")).trimmed();

    if (!course.isEmpty())
    {
        return course;
    }

    if (!transferClass.name.trimmed().isEmpty())
    {
        return transferClass.name.trimmed();
    }

    return transferClass.key;
}

QString destinationClassLabel(
    const Classroom& classroom,
    const ClassInfo& info
    )
{
    const QString course = QStringList{
        info.classGrade.trimmed(),
        info.classLevel.trimmed()
        }.join(QStringLiteral(" ")).trimmed();

    if (!course.isEmpty())
    {
        return course;
    }

    if (!classroom.name.trimmed().isEmpty())
    {
        return classroom.name.trimmed();
    }

    return QObject::tr("Class %1").arg(classroom.id);
}

int dayIndex(
    const QString& day
    )
{
    static const QStringList Days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };

    return Days.indexOf(day);
}

int timeToMinutes(
    const QString& value
    )
{
    const QStringList parts = value.trimmed().split(
        QLatin1Char(' '), Qt::SkipEmptyParts);

    if (parts.size() != 2)
    {
        return -1;
    }

    const QStringList timeParts = parts[0].split(QLatin1Char(':'));

    if (timeParts.size() != 2)
    {
        return -1;
    }

    bool hourOk = false;
    bool minuteOk = false;
    int hour = timeParts[0].toInt(&hourOk);
    const int minute = timeParts[1].toInt(&minuteOk);
    const QString period = parts[1].toUpper();

    if (!hourOk || !minuteOk || hour < 1 || hour > 12
        || minute < 0 || minute > 59
        || (period != QStringLiteral("AM")
            && period != QStringLiteral("PM")))
    {
        return -1;
    }

    if (period == QStringLiteral("AM"))
    {
        if (hour == 12)
        {
            hour = 0;
        }
    }
    else if (hour != 12)
    {
        hour += 12;
    }

    return hour * 60 + minute;
}

std::optional<ClassMngr::Next::Application::ClassTransferScheduleCandidate>
candidateForTime(
    const ClassTime& time,
    const ClassMngr::Next::Application::TransferTimeCategory category
    )
{
    const int day = dayIndex(time.day);
    const int start = timeToMinutes(time.startTime);
    const int end = timeToMinutes(time.endTime);

    if (day < 0 || start < 0 || end < 0)
    {
        return std::nullopt;
    }

    const auto candidate =
        ClassMngr::Next::Application::ClassTransferScheduleCandidate::create(
            category,
            day,
            start,
            end
            );
    if (!candidate)
    {
        return std::nullopt;
    }

    return candidate.value();
}

QString timeDescription(
    const ScheduledTime& scheduled
    )
{
    return QObject::tr("%1 — %2 %3–%4")
        .arg(
            scheduled.classLabel,
            scheduled.time.day,
            scheduled.time.startTime,
            scheduled.time.endTime
            );
}

Status appendAndValidateTimes(
    QList<ScheduledTime>* destination,
    const QString& classLabel,
    const QList<ClassTime>& times,
    const QString& scheduleLabel,
    const ClassMngr::Next::Application::TransferTimeCategory category
    )
{
    for (const ClassTime& time : times)
    {
        const auto candidate = candidateForTime(time, category);
        if (!candidate)
        {
            return std::unexpected(
                QObject::tr("%1 contains an invalid %2 schedule entry: %3 %4–%5")
                    .arg(
                        classLabel,
                        scheduleLabel,
                        time.day,
                        time.startTime,
                        time.endTime
                        )
                );
        }

        destination->append({
            classLabel,
            time,
            *candidate
        });
    }

    return {};
}

QStringList findScheduleConflicts(
    const QList<ScheduledTime>& imported,
    const QList<ScheduledTime>& existing
    )
{
    using ClassTransferScheduleCandidate =
        ClassMngr::Next::Application::ClassTransferScheduleCandidate;
    using ClassTransferScheduleConflict =
        ClassMngr::Next::Application::ClassTransferScheduleConflict;
    using TransferTimeCategory =
        ClassMngr::Next::Application::TransferTimeCategory;

    std::vector<ClassTransferScheduleCandidate> incomingCandidates;
    incomingCandidates.reserve(static_cast<std::size_t>(imported.size()));
    for (const ScheduledTime& item : imported)
    {
        incomingCandidates.push_back(item.candidate);
    }

    std::vector<ClassTransferScheduleCandidate> existingCandidates;
    existingCandidates.reserve(static_cast<std::size_t>(existing.size()));
    for (const ScheduledTime& item : existing)
    {
        existingCandidates.push_back(item.candidate);
    }

    QStringList regularConflicts;
    QStringList intensiveConflicts;
    for (const ClassTransferScheduleConflict& conflict :
         ClassMngr::Next::Application::findClassTransferScheduleConflicts(
             incomingCandidates,
             existingCandidates))
    {
        const ScheduledTime& first = imported.at(
            static_cast<qsizetype>(conflict.incomingIndex));
        const ScheduledTime& second = conflict.otherIsIncoming
            ? imported.at(static_cast<qsizetype>(conflict.otherIndex))
            : existing.at(static_cast<qsizetype>(conflict.otherIndex));
        const QString scheduleLabel =
            first.candidate.category() == TransferTimeCategory::Regular
            ? QObject::tr("Regular schedule")
            : QObject::tr("Intensive schedule");
        QStringList& categoryConflicts =
            first.candidate.category() == TransferTimeCategory::Regular
            ? regularConflicts
            : intensiveConflicts;
        const QString message = QObject::tr("%1: %2 conflicts with %3")
            .arg(
                scheduleLabel,
                timeDescription(first),
                timeDescription(second)
                );

        if (!categoryConflicts.contains(message))
        {
            categoryConflicts.append(message);
        }
    }

    regularConflicts.append(intensiveConflicts);

    return regularConflicts;
}

Result<ClassImportPreview> buildPreview(
    QSqlDatabase& database,
    const ClassTransferPackage& package
    )
{
    TeacherRepository teacherRepository(database);
    ClassRepository classRepository(database);
    ClassInfoRepository classInfoRepository(database);

    const Result<QList<Teacher>> destinationTeachers =
        teacherRepository.getAllTeachers();
    if (!destinationTeachers)
    {
        return std::unexpected(destinationTeachers.error());
    }

    const Result<QList<Classroom>> destinationClasses =
        classRepository.getClasses();
    if (!destinationClasses)
    {
        return std::unexpected(destinationClasses.error());
    }

    using namespace ClassMngr::Next::Application;

    ClassTransferMatchingRequest matchingRequest;
    matchingRequest.sourceTeachers.reserve(
        static_cast<std::size_t>(package.teachers.size())
        );
    for (const ClassTransferTeacher& source : package.teachers)
    {
        matchingRequest.sourceTeachers.push_back({
            source.key.toStdString(),
            {
                normalized(source.teacher.teacherEn).toStdString(),
                normalized(source.teacher.teacherKr).toStdString()
            }
        });
    }

    std::map<ClassMngr::Next::Domain::TeacherId, int> legacyTeacherIds;
    QHash<int, ClassTransferMatchingTeacherNames> destinationTeacherNamesById;
    matchingRequest.destinationTeachers.reserve(
        static_cast<std::size_t>(destinationTeachers->size())
        );
    for (const Teacher& destination : *destinationTeachers)
    {
        const auto typedId = applicationId<
            ClassMngr::Next::Domain::TeacherId>(destination.id);
        const ClassTransferMatchingTeacherNames names{
            normalized(destination.teacherEn).toStdString(),
            normalized(destination.teacherKr).toStdString()
        };
        legacyTeacherIds.emplace(typedId, destination.id);
        destinationTeacherNamesById.insert(destination.id, names);
        matchingRequest.destinationTeachers.push_back({
            typedId,
            names
        });
    }

    for (int index = 0; index < package.classes.size(); ++index)
    {
        const ClassTransferClass& source = package.classes[index];
        matchingRequest.sourceClasses.push_back({
            static_cast<std::size_t>(index),
            source.teacherKey.toStdString(),
            normalized(source.info.classGrade).toStdString(),
            normalized(source.info.classLevel).toStdString()
        });
    }

    std::unordered_set<std::string> sourceTeacherKeys;
    sourceTeacherKeys.reserve(matchingRequest.sourceTeachers.size());
    for (const auto& teacher : matchingRequest.sourceTeachers)
    {
        sourceTeacherKeys.insert(teacher.sourceKey);
    }

    const auto hasSourceTeacherForCourse = [&](
        const std::string& grade,
        const std::string& level)
    {
        return std::any_of(
            matchingRequest.sourceClasses.cbegin(),
            matchingRequest.sourceClasses.cend(),
            [&](const ClassTransferMatchingSourceClass& sourceClass)
            {
                if (sourceClass.grade.empty() || sourceClass.level.empty()
                    || sourceClass.grade != grade
                    || sourceClass.level != level)
                {
                    return false;
                }
                return sourceTeacherKeys.contains(
                    sourceClass.teacherSourceKey);
            }
            );
    };

    std::map<ClassMngr::Next::Domain::ClassId, int> legacyClassIds;
    const bool hasCompleteSourceClass = std::any_of(
        matchingRequest.sourceClasses.cbegin(),
        matchingRequest.sourceClasses.cend(),
        [](const ClassTransferMatchingSourceClass& sourceClass)
        {
            return !sourceClass.grade.empty() && !sourceClass.level.empty();
        }
        );

    if (hasCompleteSourceClass)
    {
        matchingRequest.destinationClasses.reserve(
            static_cast<std::size_t>(destinationClasses->size())
            );
        QList<int> destinationClassIds;
        destinationClassIds.reserve(destinationClasses->size());
        for (const Classroom& destination : *destinationClasses)
        {
            destinationClassIds.append(destination.id);
        }

        if (!destinationClassIds.isEmpty())
        {
            const Result<QList<ClassNavigationReadRecord>> destinationInfoRecords =
                classInfoRepository.loadClassesNavigationRecords(
                    destinationClassIds);
            if (!destinationInfoRecords)
            {
                return std::unexpected(destinationInfoRecords.error());
            }

            for (qsizetype index = 0;
                 index < destinationClasses->size();
                 ++index)
            {
                const Classroom& destination = destinationClasses->at(index);
                const ClassNavigationReadRecord& destinationInfo =
                    destinationInfoRecords->at(index);
                const QString grade = normalized(destinationInfo.grade);
                const QString level = normalized(destinationInfo.level);
                std::optional<ClassTransferMatchingDestinationTeacher>
                    destinationTeacher;
                if (destinationInfo.teacherId > 0)
                {
                    const auto typedTeacherId = applicationId<
                        ClassMngr::Next::Domain::TeacherId>(
                            destinationInfo.teacherId);
                    ClassTransferMatchingTeacherNames names;
                    if (hasSourceTeacherForCourse(
                            grade.toStdString(), level.toStdString()))
                    {
                        const auto teacherNames =
                            destinationTeacherNamesById.constFind(
                                destinationInfo.teacherId);
                        if (teacherNames != destinationTeacherNamesById.cend())
                        {
                            names = *teacherNames;
                        }
                        else
                        {
                            const Result<Teacher> teacher =
                                teacherRepository.getTeacher(
                                    destinationInfo.teacherId);
                            if (!teacher)
                            {
                                return std::unexpected(teacher.error());
                            }

                            names = {
                                normalized(teacher->teacherEn).toStdString(),
                                normalized(teacher->teacherKr).toStdString()
                            };
                        }
                    }

                    destinationTeacher = ClassTransferMatchingDestinationTeacher{
                        typedTeacherId,
                        std::move(names)
                    };
                }

                const auto typedId = applicationId<
                    ClassMngr::Next::Domain::ClassId>(destination.id);
                legacyClassIds.emplace(typedId, destination.id);
                matchingRequest.destinationClasses.push_back({
                    typedId,
                    grade.toStdString(),
                    level.toStdString(),
                    std::move(destinationTeacher)
                });
            }
        }
    }

    const ClassTransferMatchingResult matches =
        matchClassTransferCandidates(matchingRequest);
    ClassImportPreview preview;
    preview.teachers.reserve(static_cast<qsizetype>(matches.teachers.size()));
    for (std::size_t index = 0; index < matches.teachers.size(); ++index)
    {
        ClassImportTeacherPreview teacherPreview;
        teacherPreview.teacherKey = package.teachers[
            static_cast<qsizetype>(index)].key;
        for (const auto& id : matches.teachers[index].matchingTeacherIds)
        {
            teacherPreview.matchingTeacherIds.append(legacyTeacherIds.at(id));
        }
        preview.teachers.append(std::move(teacherPreview));
    }

    preview.classes.reserve(static_cast<qsizetype>(matches.classes.size()));
    for (const auto& match : matches.classes)
    {
        ClassImportClassPreview classPreview;
        classPreview.packageClassIndex = static_cast<int>(
            match.packageClassIndex);
        for (const auto& id : match.matchingClassIds)
        {
            classPreview.matchingClassIds.append(legacyClassIds.at(id));
        }
        preview.classes.append(std::move(classPreview));
    }

    return preview;
}

Result<ValidatedPlan> validatePlan(
    QSqlDatabase& database,
    const ClassTransferPackage& package,
    const ClassMngr::Next::Application::ClassTransferApplyRequest& choices
    )
{
    const auto previewResult = buildPreview(database, package);

    if (!previewResult)
    {
        return std::unexpected(previewResult.error());
    }

    const auto candidates = ClassMngr::Next::Platform::classTransferApplyCandidates(
        package, *previewResult, true);
    if (!candidates)
        return std::unexpected(candidates.error());
    auto request = choices;
    for (auto& resolution : request.teachers)
        if (QString::fromStdString(resolution.teacherKey).trimmed().isEmpty())
            resolution.teacherKey.clear();
    const auto decision = ClassMngr::Next::Application::validateClassTransferApplyRequest(
        request, *candidates);
    if (!decision.accepted())
    {
        return std::unexpected(reviewIssueMessage(decision.issues.front().code));
    }

    const auto plan = ClassMngr::Next::Platform::validatedClassTransferLegacyPlan(request);
    ValidatedPlan validated;
    for (const ClassImportResolution& resolution : plan.classes)
    {
        validated.classes.insert(resolution.packageClassIndex, resolution);
    }
    for (const TeacherImportResolution& resolution : plan.teachers)
    {
        validated.teachers.insert(resolution.teacherKey, resolution);
    }
    return validated;
}

Status preflightSchedules(
    QSqlDatabase& database,
    const ClassTransferPackage& package,
    const ValidatedPlan& plan
    )
{
    using TransferTimeCategory =
        ClassMngr::Next::Application::TransferTimeCategory;

    QList<ScheduledTime> importedSchedules;
    QSet<int> replacedClassIds;

    for (auto iterator = plan.classes.cbegin();
         iterator != plan.classes.cend(); ++iterator)
    {
        if (iterator->action == ClassImportAction::Replace)
        {
            replacedClassIds.insert(iterator->targetClassId);
        }
    }

    for (int index = 0; index < package.classes.size(); ++index)
    {
        const ClassImportResolution resolution = plan.classes.value(index);

        if (resolution.action == ClassImportAction::Skip)
        {
            continue;
        }

        const ClassTransferClass& transferClass = package.classes[index];
        const QString label = transferClassLabel(transferClass);
        Status status = appendAndValidateTimes(
            &importedSchedules,
            label,
            transferClass.info.classTimes,
            QObject::tr("regular"),
            TransferTimeCategory::Regular
            );

        if (!status)
        {
            return status;
        }

        status = appendAndValidateTimes(
            &importedSchedules,
            label,
            transferClass.info.intensiveTimes,
            QObject::tr("intensive"),
            TransferTimeCategory::Intensive
            );

        if (!status)
        {
            return status;
        }
    }

    ClassRepository classRepository(database);
    ClassInfoRepository classInfoRepository(database);
    QList<ScheduledTime> existingSchedules;

    const Result<QList<Classroom>> destinationClasses =
        classRepository.getClasses();
    if (!destinationClasses)
    {
        return std::unexpected(destinationClasses.error());
    }

    QList<Classroom> eligibleDestinationClasses;
    QList<int> eligibleDestinationClassIds;
    for (const Classroom& classroom : *destinationClasses)
    {
        if (replacedClassIds.contains(classroom.id))
        {
            continue;
        }

        eligibleDestinationClasses.append(classroom);
        eligibleDestinationClassIds.append(classroom.id);
    }

    if (!eligibleDestinationClassIds.isEmpty())
    {
        const Result<QList<ClassNavigationReadRecord>> destinationRecords =
            classInfoRepository.loadClassesNavigationRecords(
                eligibleDestinationClassIds);
        if (!destinationRecords)
        {
            return std::unexpected(destinationRecords.error());
        }

        for (qsizetype index = 0; index < destinationRecords->size(); ++index)
        {
            const Classroom& classroom = eligibleDestinationClasses.at(index);
            const ClassNavigationReadRecord& record =
                destinationRecords->at(index);
            ClassInfo info;
            info.classGrade = record.grade;
            info.classLevel = record.level;

            const QString label = destinationClassLabel(classroom, info);
            Status status = appendAndValidateTimes(
                &existingSchedules,
                label,
                record.regularTimes,
                QObject::tr("regular"),
                TransferTimeCategory::Regular
                );

            if (!status)
            {
                return status;
            }

            status = appendAndValidateTimes(
                &existingSchedules,
                label,
                record.intensiveTimes,
                QObject::tr("intensive"),
                TransferTimeCategory::Intensive
                );

            if (!status)
            {
                return status;
            }
        }
    }

    const QStringList conflicts = findScheduleConflicts(
        importedSchedules,
        existingSchedules
        );

    if (!conflicts.isEmpty())
    {
        return std::unexpected(
            QObject::tr("Schedule conflicts prevent this import:\n\n%1")
                .arg(conflicts.join(QLatin1Char('\n')))
            );
    }

    return {};
}

Status queryFailure(
    const QSqlQuery& query,
    const QString& operation
    )
{
    return std::unexpected(
        SqlQueryUtils::errorFor(query, operation).userMessage()
        );
}

Result<int> insertTeacher(
    QSqlDatabase& database,
    const Teacher& teacher
    )
{
    QSqlQuery query(database);
    query.prepare(R"(
        INSERT INTO teachers (
            teacher_kr, teacher_en, preferred_romanization, preferred_name,
            room_number, birthday, phone_number,
            wifi_name, wifi_password, internet_type,
            zoom_id, zoom_password, projection_type, notes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.preferredRomanization);
    query.addBindValue(teacher.preferredName);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.birthday);
    query.addBindValue(teacher.phoneNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(teacher.internetType);
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
    query.addBindValue(teacher.projectionType);
    query.addBindValue(teacher.notes);

    if (!query.exec())
    {
        return std::unexpected(queryFailure(
            query, QObject::tr("Creating an imported teacher")).error());
    }

    const int id = query.lastInsertId().toInt();

    if (id <= 0)
    {
        return std::unexpected(
            QObject::tr("The imported teacher did not receive a Teacher Profile ID.")
            );
    }

    return id;
}

Status updateTeacher(
    QSqlDatabase& database,
    int teacherId,
    const Teacher& teacher
    )
{
    QSqlQuery query(database);
    query.prepare(R"(
        UPDATE teachers SET
            teacher_kr=?, teacher_en=?, preferred_romanization=?, preferred_name=?,
            room_number=?, birthday=?, phone_number=?,
            wifi_name=?, wifi_password=?, internet_type=?,
            zoom_id=?, zoom_password=?, projection_type=?, notes=?
        WHERE id=?
    )");
    query.addBindValue(teacher.teacherKr);
    query.addBindValue(teacher.teacherEn);
    query.addBindValue(teacher.preferredRomanization);
    query.addBindValue(teacher.preferredName);
    query.addBindValue(teacher.roomNumber);
    query.addBindValue(teacher.birthday);
    query.addBindValue(teacher.phoneNumber);
    query.addBindValue(teacher.wifiName);
    query.addBindValue(teacher.wifiPassword);
    query.addBindValue(teacher.internetType);
    query.addBindValue(teacher.zoomId);
    query.addBindValue(teacher.zoomPassword);
    query.addBindValue(teacher.projectionType);
    query.addBindValue(teacher.notes);
    query.addBindValue(teacherId);

    if (!query.exec())
    {
        return queryFailure(query, QObject::tr("Updating a matched teacher"));
    }

    if (query.numRowsAffected() != 1)
    {
        return std::unexpected(
            QObject::tr("The matched teacher no longer exists.")
            );
    }

    return {};
}

Status clearClassData(
    QSqlDatabase& database,
    int classId
    )
{
    QSqlQuery query(database);
    query.prepare(R"(
        DELETE FROM speaking_eval_data
        WHERE evaluation_id IN (
            SELECT id FROM speaking_evaluations WHERE class_id=?
        )
    )");
    query.addBindValue(classId);

    if (!query.exec())
    {
        return queryFailure(query, QObject::tr("Clearing speaking evaluation rows"));
    }

    for (const QString& table : {
             QStringLiteral("speaking_evaluations"),
             QStringLiteral("roster_columns"),
             QStringLiteral("roster_data"),
             QStringLiteral("class_info"),
             QStringLiteral("class_times"),
             QStringLiteral("class_intensive_times")
         })
    {
        query.prepare(
            QStringLiteral("DELETE FROM %1 WHERE class_id=?").arg(table));
        query.addBindValue(classId);

        if (!query.exec())
        {
            return queryFailure(
                query,
                QObject::tr("Clearing imported class data from %1").arg(table)
                );
        }
    }

    return {};
}

Status writeClassData(
    QSqlDatabase& database,
    int classId,
    int teacherId,
    const ClassTransferClass& transferClass
    )
{
    QSqlQuery query(database);
    const ClassInfo& info = transferClass.info;
    query.prepare(R"(
        INSERT INTO class_info (
            class_id, teacher_id, class_grade, class_level,
            reading_book, essay_book, class_color, font_color,
            notes, time_filler_activities
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(classId);
    query.addBindValue(teacherId > 0 ? QVariant(teacherId) : QVariant());
    query.addBindValue(info.classGrade);
    query.addBindValue(info.classLevel);
    query.addBindValue(info.readingBook);
    query.addBindValue(info.essayBook);
    query.addBindValue(info.classColor);
    query.addBindValue(info.fontColor);
    query.addBindValue(info.notes);
    query.addBindValue(info.timeFillerActivities);

    if (!query.exec())
    {
        return queryFailure(query, QObject::tr("Writing imported class information"));
    }

    const auto writeTimes = [&database, classId](
        const QString& table,
        const QList<ClassTime>& times) -> Status
    {
        for (const ClassTime& time : times)
        {
            QSqlQuery timeQuery(database);
            timeQuery.prepare(
                QStringLiteral(
                    "INSERT INTO %1 (class_id, day, start_time, end_time) "
                    "VALUES (?, ?, ?, ?)").arg(table));
            timeQuery.addBindValue(classId);
            timeQuery.addBindValue(time.day);
            timeQuery.addBindValue(time.startTime);
            timeQuery.addBindValue(time.endTime);

            if (!timeQuery.exec())
            {
                return queryFailure(
                    timeQuery,
                    QObject::tr("Writing imported schedule data")
                    );
            }
        }

        return {};
    };

    Status status = writeTimes(
        QStringLiteral("class_times"), info.classTimes);

    if (!status)
    {
        return status;
    }

    status = writeTimes(
        QStringLiteral("class_intensive_times"), info.intensiveTimes);

    if (!status)
    {
        return status;
    }

    for (int column = 0; column < transferClass.roster.columns.size(); ++column)
    {
        query.prepare(R"(
            INSERT INTO roster_columns (class_id, name, position, width)
            VALUES (?, ?, ?, ?)
        )");
        query.addBindValue(classId);
        query.addBindValue(transferClass.roster.columns[column]);
        query.addBindValue(column);
        query.addBindValue(
            column < transferClass.roster.columnWidths.size()
                ? transferClass.roster.columnWidths[column]
                : 0
            );

        if (!query.exec())
        {
            return queryFailure(query, QObject::tr("Writing imported roster columns"));
        }
    }

    for (int row = 0; row < transferClass.roster.rows.size(); ++row)
    {
        for (int column = 0;
             column < transferClass.roster.columns.size(); ++column)
        {
            const QString value =
                column < transferClass.roster.rows[row].size()
                    ? transferClass.roster.rows[row][column]
                    : QString();

            if (value.isEmpty())
            {
                continue;
            }

            query.prepare(R"(
                INSERT INTO roster_data (class_id, row_index, col_index, value)
                VALUES (?, ?, ?, ?)
            )");
            query.addBindValue(classId);
            query.addBindValue(row);
            query.addBindValue(column);
            query.addBindValue(value);

            if (!query.exec())
            {
                return queryFailure(query, QObject::tr("Writing imported roster data"));
            }
        }
    }

    for (const ClassTransferEvaluation& evaluation : transferClass.evaluations)
    {
        query.prepare(R"(
            INSERT INTO speaking_evaluations (class_id, evaluation_name)
            VALUES (?, ?)
        )");
        query.addBindValue(classId);
        query.addBindValue(evaluation.name);

        if (!query.exec())
        {
            return queryFailure(query, QObject::tr("Creating an imported speaking evaluation"));
        }

        const int evaluationId = query.lastInsertId().toInt();

        for (int row = 0; row < SpeakingEval::RowCount; ++row)
        {
            query.prepare(R"(
                INSERT INTO speaking_eval_data (
                    evaluation_id, row_index,
                    col_0, col_1, col_2, col_3, col_4, col_5,
                    col_6, col_7, col_8, col_9, col_10
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )");
            query.addBindValue(evaluationId);
            query.addBindValue(row);

            for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
            {
                query.addBindValue(
                    row < evaluation.rows.size()
                    && column < evaluation.rows[row].size()
                        ? evaluation.rows[row][column]
                        : QString()
                    );
            }

            if (!query.exec())
            {
                return queryFailure(query, QObject::tr("Writing imported speaking evaluation rows"));
            }
        }
    }

    return {};
}
}

ClassTransferRepository::ClassTransferRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<ClassTransferExportSource> ClassTransferRepository::readExportSource(
    const QList<int>& classIds
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(QObject::tr("No Teacher Profile is open."));
    }

    if (classIds.isEmpty())
    {
        return std::unexpected(QObject::tr("No classes were selected."));
    }

    DatabaseTransaction transaction(m_database);

    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Unable to start the class export transaction: %1")
                .arg(m_database.lastError().text())
            );
    }

    ClassRepository classRepository(m_database);
    ClassInfoRepository classInfoRepository(m_database);
    RosterRepository rosterRepository(m_database);
    TeacherRepository teacherRepository(m_database);

    ClassTransferExportSource source;
    source.exportedAtUtc = QDateTime::currentDateTimeUtc();
    QSet<int> seenClasses;
    QSet<int> seenTeachers;
    source.classes.reserve(classIds.size());

    // Resolve selections in order and stop at the first known selection or
    // class lookup failure. Only this validated prefix is passed to the data
    // batches, so a later invalid selection cannot mask an earlier failure.
    for (int index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds[index];

        if (classId <= 0 || seenClasses.contains(classId))
        {
            source.selectionOrReadError = QObject::tr(
                "The class selection contains an invalid or duplicate class.");
            break;
        }

        const Result<Classroom> classroom =
            classRepository.getClassById(classId);
        if (!classroom)
        {
            source.selectionOrReadError = classroom.error();
            break;
        }

        seenClasses.insert(classId);
        ClassTransferExportSourceClass pending;
        pending.classId = classId;
        pending.selectedIndex = index;
        pending.name = classroom->name;
        source.classes.append(std::move(pending));
    }

    QList<int> exportClassIds;
    exportClassIds.reserve(source.classes.size());
    for (const ClassTransferExportSourceClass& pending : source.classes)
    {
        exportClassIds.append(pending.classId);
    }

    auto classInfoRecords =
        classInfoRepository.loadClassInfoRecords(exportClassIds);
    bool classInfoBatchMatchesRequest =
        classInfoRecords.has_value()
        && classInfoRecords->size() == source.classes.size();
    if (classInfoBatchMatchesRequest)
    {
        for (qsizetype index = 0; index < source.classes.size(); ++index)
        {
            if (classInfoRecords->at(index).classId
                != source.classes.at(index).classId)
            {
                classInfoBatchMatchesRequest = false;
                break;
            }
        }
    }

    if (classInfoBatchMatchesRequest)
    {
        for (qsizetype index = 0; index < source.classes.size(); ++index)
        {
            source.classes[index].info =
                std::move((*classInfoRecords)[index]);
        }
    }
    else
    {
        for (ClassTransferExportSourceClass& pending : source.classes)
        {
            Result<ClassInfo> info =
                classInfoRepository.loadClassInfo(pending.classId);
            if (info)
            {
                pending.info = std::move(*info);
            }
            else
            {
                pending.infoError = info.error();
            }
        }
    }

    auto rosterRecords = rosterRepository.loadFullRosters(exportClassIds);
    bool rosterBatchMatchesRequest =
        rosterRecords.has_value()
        && rosterRecords->size() == source.classes.size();
    if (rosterBatchMatchesRequest)
    {
        for (qsizetype index = 0; index < source.classes.size(); ++index)
        {
            if (rosterRecords->at(index).classId
                != source.classes.at(index).classId)
            {
                rosterBatchMatchesRequest = false;
                break;
            }
        }
    }

    if (rosterBatchMatchesRequest)
    {
        for (qsizetype index = 0; index < source.classes.size(); ++index)
        {
            source.classes[index].roster =
                std::move((*rosterRecords)[index].roster);
        }
    }
    else
    {
        for (ClassTransferExportSourceClass& pending : source.classes)
        {
            Result<Roster> roster =
                rosterRepository.loadRoster(pending.classId);
            if (roster)
            {
                pending.roster = std::move(*roster);
            }
            else
            {
                pending.rosterError = roster.error();
            }
        }
    }

    // Discover the first known info/roster/evaluation failure in the original
    // per-class sequence. Teacher IDs are collected only after info and roster
    // succeed, and before evaluations, to retain their former read order.
    for (ClassTransferExportSourceClass& pending : source.classes)
    {
        if (!pending.infoError.isEmpty() || !pending.rosterError.isEmpty())
        {
            break;
        }

        pending.teacherId = pending.info.teacherId;
        if (pending.teacherId > 0 && !seenTeachers.contains(pending.teacherId))
        {
            seenTeachers.insert(pending.teacherId);
            source.teacherIds.append(pending.teacherId);
        }

        pending.evaluationAttempted = true;
        pending.evaluations = loadEvaluations(
            m_database, pending.classId, &pending.evaluationError);
        if (!pending.evaluationError.isEmpty())
        {
            break;
        }
    }

    if (!source.teacherIds.isEmpty())
    {
        const Result<QList<TeacherProfileBatchReadRecord>> teacherProfiles =
            teacherRepository.loadTeacherProfileRecords(source.teacherIds);
        if (!teacherProfiles)
        {
            // A batch-level failure occupies the first assigned-teacher
            // profile's position in the original sequence.
            source.teacherBatchError = teacherProfiles.error();
        }
        else if (teacherProfiles->size() != source.teacherIds.size())
        {
            source.teacherBatchError = QObject::tr(
                "Loading teacher profiles failed: the database returned an unexpected number of records.");
        }
        else
        {
            for (qsizetype index = 0; index < source.teacherIds.size(); ++index)
            {
                const int teacherId = source.teacherIds[index];
                const TeacherProfileBatchReadRecord& record =
                    teacherProfiles->at(index);
                if (record.teacherId != teacherId)
                {
                    source.teacherBatchError = QObject::tr(
                        "Loading teacher profiles failed: the database returned records in a different identifier order.");
                    break;
                }

                if (!record.profile)
                {
                    source.teacherErrorsById.insert(teacherId, record.profile.error());
                    continue;
                }

                if (record.profile->id != teacherId)
                {
                    source.teacherBatchError = QObject::tr(
                        "Loading teacher profiles failed: a database record returned a different teacher identifier.");
                    break;
                }

                source.teachersById.insert(teacherId, *record.profile);
            }
        }
    }

    bool readFailed = !source.selectionOrReadError.isEmpty()
        || !source.teacherBatchError.isEmpty() || !source.teacherErrorsById.isEmpty();
    for (const auto& pending : source.classes)
        readFailed = readFailed || !pending.infoError.isEmpty()
            || !pending.rosterError.isEmpty() || !pending.evaluationError.isEmpty();
    if (readFailed)
        return source; // Transaction destructor rolls back, exactly as error replay did.

    if (!transaction.commit())
        return std::unexpected(QObject::tr("Unable to finish the class export transaction: %1")
            .arg(m_database.lastError().text()));
    return source;
}

Result<ClassTransferPackage> ClassTransferRepository::buildPackage(const QList<int>& classIds)
{
    auto loaded = readExportSource(classIds);
    if (!loaded)
        return std::unexpected(loaded.error());
    auto& pendingClasses = loaded->classes;
    const auto& teacherIds = loaded->teacherIds;
    const auto& teachersById = loaded->teachersById;
    const auto& teacherErrorsById = loaded->teacherErrorsById;
    const auto& teacherBatchError = loaded->teacherBatchError;
    const auto& selectionOrReadError = loaded->selectionOrReadError;
    ClassTransferPackage package;
    package.exportedAtUtc = loaded->exportedAtUtc;
    // Replay staged outcomes in their original order so an earlier teacher
    // or evaluation failure keeps precedence over a later info, roster,
    // selection, or class lookup failure.
    for (const ClassTransferExportSourceClass& pending : pendingClasses)
    {
        if (!pending.infoError.isEmpty())
        {
            return std::unexpected(pending.infoError);
        }
        if (!pending.rosterError.isEmpty())
        {
            return std::unexpected(pending.rosterError);
        }

        if (pending.teacherId > 0)
        {
            if (!teacherBatchError.isEmpty())
            {
                return std::unexpected(teacherBatchError);
            }
            const auto teacherError =
                teacherErrorsById.constFind(pending.teacherId);
            if (teacherError != teacherErrorsById.cend())
            {
                return std::unexpected(*teacherError);
            }
        }

        if (!pending.evaluationError.isEmpty())
        {
            return std::unexpected(pending.evaluationError);
        }
    }

    if (!selectionOrReadError.isEmpty())
    {
        return std::unexpected(selectionOrReadError);
    }

    QHash<int, QString> teacherKeys;
    for (const int teacherId : teacherIds)
    {
        const QString key =
            QStringLiteral("teacher-%1").arg(package.teachers.size() + 1);
        teacherKeys.insert(teacherId, key);
        package.teachers.append({key, teachersById.value(teacherId)});
    }

    for (ClassTransferExportSourceClass& pending : pendingClasses)
    {
        ClassTransferClass transferClass;
        transferClass.key = QStringLiteral("class-%1")
            .arg(pending.selectedIndex + 1);
        transferClass.name = pending.name;
        transferClass.info = std::move(pending.info);
        transferClass.roster = std::move(pending.roster);
        transferClass.evaluations = std::move(pending.evaluations);

        if (pending.teacherId > 0)
        {
            transferClass.teacherKey = teacherKeys.value(pending.teacherId);
        }

        transferClass.info.classId = -1;
        transferClass.info.teacherId = -1;
        transferClass.info.teacherKr.clear();
        transferClass.info.teacherEn.clear();
        transferClass.info.roomNumber.clear();
        transferClass.info.wifiName.clear();
        transferClass.info.wifiPassword.clear();
        transferClass.info.internetType.clear();
        transferClass.info.zoomId.clear();
        transferClass.info.zoomPassword.clear();
        transferClass.info.projectionType.clear();
        package.classes.append(transferClass);
    }

    return package;
}

Result<ClassImportPreview> ClassTransferRepository::previewImport(
    const ClassTransferPackage& package
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(QObject::tr("No Teacher Profile is open."));
    }

    return buildPreview(m_database, package);
}

Result<ClassImportSummary> ClassTransferRepository::importClasses(
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    )
{
    return importClasses(package,
        ClassMngr::Next::Platform::classTransferApplyRequest(plan));
}

Result<ClassImportSummary> ClassTransferRepository::importClasses(
    const ClassTransferPackage& package,
    const ClassMngr::Next::Application::ClassTransferApplyRequest& request
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(QObject::tr("No Teacher Profile is open."));
    }

    const auto validatedResult = validatePlan(m_database, package, request);

    if (!validatedResult)
    {
        return std::unexpected(validatedResult.error());
    }

    const Status schedulesReady = preflightSchedules(
        m_database, package, *validatedResult);

    if (!schedulesReady)
    {
        return std::unexpected(schedulesReady.error());
    }

    QSet<QString> usedTeacherKeys;

    for (int index = 0; index < package.classes.size(); ++index)
    {
        if (validatedResult->classes.value(index).action
                != ClassImportAction::Skip
            && !package.classes[index].teacherKey.isEmpty())
        {
            usedTeacherKeys.insert(package.classes[index].teacherKey);
        }
    }

    DatabaseTransaction transaction(m_database);

    if (!transaction.started())
    {
        return std::unexpected(
            QObject::tr("Unable to start the class import transaction: %1")
                .arg(m_database.lastError().text())
            );
    }

    QHash<QString, int> teacherIds;

    for (const ClassTransferTeacher& transferTeacher : package.teachers)
    {
        if (!usedTeacherKeys.contains(transferTeacher.key))
        {
            continue;
        }

        const TeacherImportResolution resolution =
            validatedResult->teachers.value(transferTeacher.key);

        if (resolution.action == TeacherImportAction::Create)
        {
            const auto teacherId = insertTeacher(
                m_database, transferTeacher.teacher);

            if (!teacherId)
            {
                return std::unexpected(teacherId.error());
            }

            teacherIds.insert(transferTeacher.key, *teacherId);
        }
        else
        {
            if (resolution.action == TeacherImportAction::ReplaceExisting)
            {
                const Status updated = updateTeacher(
                    m_database,
                    resolution.targetTeacherId,
                    transferTeacher.teacher
                    );

                if (!updated)
                {
                    return std::unexpected(updated.error());
                }
            }

            teacherIds.insert(
                transferTeacher.key, resolution.targetTeacherId);
        }
    }

    ClassImportSummary summary;

    for (int index = 0; index < package.classes.size(); ++index)
    {
        const ClassImportResolution resolution =
            validatedResult->classes.value(index);
        const ClassTransferClass& transferClass = package.classes[index];

        if (resolution.action == ClassImportAction::Skip)
        {
            ++summary.skippedClassCount;
            continue;
        }

        int classId = resolution.targetClassId;
        QSqlQuery query(m_database);

        if (resolution.action == ClassImportAction::Create)
        {
            query.prepare(QStringLiteral("INSERT INTO classes (name) VALUES (?)"));
            query.addBindValue(transferClass.name);

            if (!query.exec())
            {
                return std::unexpected(
                    queryFailure(query, QObject::tr("Creating an imported class")).error()
                    );
            }

            classId = query.lastInsertId().toInt();

            if (classId <= 0)
            {
                return std::unexpected(
                    QObject::tr("The imported class did not receive a Teacher Profile ID.")
                    );
            }

            summary.createdClassIds.append(classId);
        }
        else
        {
            query.prepare(QStringLiteral("UPDATE classes SET name=? WHERE id=?"));
            query.addBindValue(transferClass.name);
            query.addBindValue(classId);

            if (!query.exec() || query.numRowsAffected() != 1)
            {
                return std::unexpected(
                    queryFailure(query, QObject::tr("Updating a replaced class")).error()
                    );
            }

            const Status cleared = clearClassData(m_database, classId);

            if (!cleared)
            {
                return std::unexpected(cleared.error());
            }

            summary.replacedClassIds.append(classId);
        }

        const int teacherId = transferClass.teacherKey.isEmpty()
            ? -1
            : teacherIds.value(transferClass.teacherKey, -1);
        const Status written = writeClassData(
            m_database, classId, teacherId, transferClass);

        if (!written)
        {
            return std::unexpected(written.error());
        }
    }

    if (!transaction.commit())
    {
        return std::unexpected(
            QObject::tr("Unable to commit the class import transaction: %1")
                .arg(m_database.lastError().text())
            );
    }

    return summary;
}
