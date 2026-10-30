#include "class_transfer_repository.h"
#include "next/platform/class_transfer_apply_legacy_adapter.h"
#include "next/platform/class_transfer_preview_legacy_adapter.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/classroom.h"
#include "domain/rules/schedule_value_parser.h"
#include "next/application/class_transfer_matching_policy.h"
#include "next/application/class_transfer_projection.h"
#include "next/application/class_transfer_preview_query.h"
#include "next/platform/repository_error_code.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
RepositoryErrorCode repositoryErrorCode(
    const ClassMngr::Next::Domain::ErrorCode code
    )
{
    switch (code)
    {
    case ClassMngr::Next::Domain::ErrorCode::InvalidInput:
        return RepositoryErrorCode::InvalidInput;
    case ClassMngr::Next::Domain::ErrorCode::NotFound:
        return RepositoryErrorCode::NotFound;
    case ClassMngr::Next::Domain::ErrorCode::Conflict:
        return RepositoryErrorCode::Conflict;
    case ClassMngr::Next::Domain::ErrorCode::Validation:
        return RepositoryErrorCode::Validation;
    case ClassMngr::Next::Domain::ErrorCode::Canceled:
        return RepositoryErrorCode::Technical;
    case ClassMngr::Next::Domain::ErrorCode::Technical:
        return RepositoryErrorCode::Technical;
    }
    return RepositoryErrorCode::Technical;
}

RepositoryError repositoryErrorForOperation(
    const ClassMngr::Next::Domain::OperationError& error
    )
{
    const QByteArray bytes = QByteArray::fromStdString(error.message);
    return RepositoryError{
        repositoryErrorCode(error.code),
        QString::fromUtf8(bytes.constData(), bytes.size())
    };
}

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

RepositoryErrorCode reviewIssueErrorCode(
    const ClassMngr::Next::Application::ClassTransferReviewDecisionIssueCode code
    )
{
    using IssueCode =
        ClassMngr::Next::Application::ClassTransferReviewDecisionIssueCode;
    switch (code)
    {
    case IssueCode::ClassTargetNotInMatchSet:
    case IssueCode::TeacherTargetNotInMatchSet:
        return RepositoryErrorCode::Conflict;
    case IssueCode::InvalidClassAction:
    case IssueCode::InvalidClassIndex:
    case IssueCode::DuplicateClassResolution:
    case IssueCode::UnknownClassResolution:
    case IssueCode::MissingClassResolution:
    case IssueCode::ReplaceClassMissingTarget:
    case IssueCode::NonReplaceClassHasTarget:
    case IssueCode::DuplicateClassReplacementTarget:
    case IssueCode::InvalidTeacherAction:
    case IssueCode::EmptyTeacherKey:
    case IssueCode::DuplicateTeacherResolution:
    case IssueCode::UnknownTeacherResolution:
    case IssueCode::MissingTeacherResolution:
    case IssueCode::UniqueTeacherCannotBeCreated:
    case IssueCode::CreateTeacherHasTarget:
    case IssueCode::TeacherActionMissingTarget:
    case IssueCode::DuplicateTeacherReplacementTarget:
        return RepositoryErrorCode::InvalidInput;
    }
    return RepositoryErrorCode::InvalidInput;
}

RepositoryError* firstExportError(ClassTransferExportSource& source)
{
    for (ClassTransferExportSourceClass& pending : source.classes)
    {
        if (pending.infoError)
        {
            return &*pending.infoError;
        }
        if (pending.rosterError)
        {
            return &*pending.rosterError;
        }
        if (pending.teacherId > 0)
        {
            if (source.teacherBatchError)
            {
                return &*source.teacherBatchError;
            }
            auto teacherError = source.teacherErrorsById.find(pending.teacherId);
            if (teacherError != source.teacherErrorsById.end())
            {
                return &teacherError.value();
            }
        }
        if (pending.evaluationError)
        {
            return &*pending.evaluationError;
        }
    }
    if (source.selectionOrReadError)
    {
        return &*source.selectionOrReadError;
    }
    return nullptr;
}

QList<ClassTransferEvaluation> loadEvaluations(
    QSqlDatabase& database,
    int classId,
    std::optional<RepositoryError>* error
    )
{
    QList<ClassTransferEvaluation> evaluations;
    QHash<qint64, int> evaluationIndexById;
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
        *error = RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to read speaking evaluations: %1")
                .arg(evaluationQuery.lastError().text())
        };
        return {};
    }

    while (evaluationQuery.next())
    {
        bool evaluationIdOk = false;
        const qint64 evaluationId = evaluationQuery.value("id").toLongLong(
            &evaluationIdOk
            );
        if (!evaluationIdOk)
        {
            *error = RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Unable to read speaking evaluations: invalid evaluation identifier for class %1."
                    ).arg(classId)
            };
            return {};
        }

        ClassTransferEvaluation evaluation;
        evaluation.name = evaluationQuery.value("evaluation_name").toString();
        evaluation.rows = SpeakingEval::emptyRows();
        evaluationIndexById.insert(evaluationId, evaluations.size());
        evaluations.append(evaluation);
    }

    if (evaluationQuery.lastError().isValid())
    {
        *error = RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to read speaking evaluations: %1")
                .arg(evaluationQuery.lastError().text())
        };
        return {};
    }

    if (evaluations.isEmpty())
    {
        return evaluations;
    }

    QSqlQuery rowQuery(database);
    rowQuery.prepare(R"(
        SELECT evaluation_id, row_index,
            typeof(row_index) AS row_index_storage_type,
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
        *error = RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to read speaking evaluation rows: %1")
                .arg(rowQuery.lastError().text())
        };
        return {};
    }

    while (rowQuery.next())
    {
        bool evaluationIdOk = false;
        const qint64 evaluationId = rowQuery.value(
            "evaluation_id"
            ).toLongLong(&evaluationIdOk);
        if (!evaluationIdOk)
        {
            *error = RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Unable to read speaking evaluation rows: invalid evaluation identifier for class %1."
                    ).arg(classId)
            };
            return {};
        }

        const auto evaluationIndex = evaluationIndexById.constFind(
            evaluationId);
        if (rowQuery.value("row_index_storage_type").toString()
            != QStringLiteral("integer"))
        {
            continue;
        }

        bool rowIndexOk = false;
        const int rowIndex = rowQuery.value("row_index").toInt(&rowIndexOk);

        if (!rowIndexOk || evaluationIndex == evaluationIndexById.cend()
            || rowIndex < 0 || rowIndex >= SpeakingEval::RowCount)
        {
            continue;
        }

        ClassTransferEvaluation& evaluation = evaluations[*evaluationIndex];
        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            evaluation.rows[rowIndex][column] =
                rowQuery.value(column + 3).toString();
        }
    }

    if (rowQuery.lastError().isValid())
    {
        *error = RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to read speaking evaluation rows: %1")
                .arg(rowQuery.lastError().text())
        };
        return {};
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
    const auto parsed = ScheduleValueParser::parseWeekday(day);
    if (!parsed || parsed->text != day)
    {
        return -1;
    }

    static const QStringList Days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday"),
        QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };

    return Days.indexOf(parsed->text);
}

std::optional<ClassMngr::Next::Domain::Weekday> weekdayForIndex(
    const int weekdayIndex
    )
{
    using Weekday = ClassMngr::Next::Domain::Weekday;
    switch (weekdayIndex)
    {
    case 0:
        return Weekday::Monday;
    case 1:
        return Weekday::Tuesday;
    case 2:
        return Weekday::Wednesday;
    case 3:
        return Weekday::Thursday;
    case 4:
        return Weekday::Friday;
    case 5:
        return Weekday::Saturday;
    case 6:
        return Weekday::Sunday;
    }

    return std::nullopt;
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

    const QString normalizedTime = QStringLiteral("%1:%2 %3")
        .arg(hour)
        .arg(minute, 2, 10, QLatin1Char('0'))
        .arg(period);
    constexpr std::array allowedFormats{
        ScheduleValueParser::DisplayTimeFormat::HourMinuteAmPmSpaced
    };
    const auto parsed = ScheduleValueParser::parseDisplayTime(
        normalizedTime,
        allowedFormats
        );
    if (!parsed)
    {
        return -1;
    }

    return parsed->hour() * 60 + parsed->minute();
}

std::optional<ClassMngr::Next::Application::ClassTransferScheduleCandidate>
candidateForTime(
    const ClassTime& time,
    const ClassMngr::Next::Application::TransferTimeCategory category
    )
{
    const std::optional<ClassMngr::Next::Domain::Weekday> weekday =
        weekdayForIndex(dayIndex(time.day));
    const int start = timeToMinutes(time.startTime);
    const int end = timeToMinutes(time.endTime);

    if (!weekday || start < 0 || end < 0)
    {
        return std::nullopt;
    }

    const auto candidate =
        ClassMngr::Next::Application::ClassTransferScheduleCandidate::create(
            category,
            *weekday,
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

RepositoryStatus appendAndValidateTimes(
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
            return std::unexpected(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr("%1 contains an invalid %2 schedule entry: %3 %4–%5")
                    .arg(
                        classLabel,
                        scheduleLabel,
                        time.day,
                        time.startTime,
                        time.endTime
                        )
            });
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
            static_cast<qsizetype>(conflict.incomingIndex.rawValue()));
        const ScheduledTime& second = conflict.otherIsIncoming
            ? imported.at(
                static_cast<qsizetype>(conflict.otherIndex.rawValue()))
            : existing.at(
                static_cast<qsizetype>(conflict.otherIndex.rawValue()));
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

RepositoryResult<ClassImportPreview> buildPreview(
    ClassMngr::Next::Application::ClassTransferPreviewReadPort& readPort,
    const ClassTransferPackage& package
    )
{
    const auto query = ClassMngr::Next::Application::ClassTransferPreviewQuery(
        readPort);
    const auto result = query.execute(
        ClassMngr::Next::Platform::classTransferPreviewRequest(package));
    if (!result)
    {
        return std::unexpected(repositoryErrorForOperation(result.error()));
    }

    return ClassMngr::Next::Platform::legacyClassTransferPreview(
        result.value());
}

RepositoryResult<ValidatedPlan> validatePlan(
    QSqlDatabase& database,
    const ClassTransferPackage& package,
    const ClassMngr::Next::Application::ClassTransferApplyRequest& choices
    )
{
    ClassTransferRepository readRepository(database);
    const auto previewResult = buildPreview(readRepository, package);

    if (!previewResult)
    {
        return std::unexpected(previewResult.error());
    }

    const auto candidates = ClassMngr::Next::Platform::classTransferApplyCandidates(
        package, *previewResult, true);
    if (!candidates)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            candidates.error()
        });
    }
    auto request = choices;
    for (auto& resolution : request.teachers)
        if (QString::fromStdString(resolution.teacherKey.value()).trimmed().isEmpty())
            resolution.teacherKey =
                ClassMngr::Next::Application::ClassTransferTeacherKey{};
    const auto decision = ClassMngr::Next::Application::validateClassTransferApplyRequest(
        request, *candidates);
    if (!decision.accepted())
    {
        const auto issueCode = decision.issues.front().code;
        return std::unexpected(RepositoryError{
            reviewIssueErrorCode(issueCode),
            reviewIssueMessage(issueCode)
        });
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

std::expected<void, ClassTransferImportError> preflightSchedules(
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
        RepositoryStatus status = appendAndValidateTimes(
            &importedSchedules,
            label,
            transferClass.info.classTimes,
            QObject::tr("regular"),
            TransferTimeCategory::Regular
            );

        if (!status)
        {
            return std::unexpected(ClassTransferImportError{
                ClassTransferImportError::Kind::RepositoryFailure,
                status.error()
            });
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
            return std::unexpected(ClassTransferImportError{
                ClassTransferImportError::Kind::RepositoryFailure,
                status.error()
            });
        }
    }

    ClassRepository classRepository(database);
    ClassInfoRepository classInfoRepository(database);
    QList<ScheduledTime> existingSchedules;

    const RepositoryResult<QList<Classroom>> destinationClasses =
        classRepository.getClasses();
    if (!destinationClasses)
    {
        return std::unexpected(ClassTransferImportError{
            ClassTransferImportError::Kind::RepositoryFailure,
            destinationClasses.error()
        });
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
        const RepositoryResult<QList<ClassNavigationReadRecord>> destinationRecords =
            classInfoRepository.loadClassesNavigationRecords(
                eligibleDestinationClassIds);
        if (!destinationRecords)
        {
            return std::unexpected(
                ClassTransferImportError{
                    ClassTransferImportError::Kind::RepositoryFailure,
                    destinationRecords.error()
                });
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
            RepositoryStatus status = appendAndValidateTimes(
                &existingSchedules,
                label,
                record.regularTimes,
                QObject::tr("regular"),
                TransferTimeCategory::Regular
                );

            if (!status)
            {
                return std::unexpected(ClassTransferImportError{
                    ClassTransferImportError::Kind::RepositoryFailure,
                    status.error()
                });
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
                return std::unexpected(ClassTransferImportError{
                    ClassTransferImportError::Kind::RepositoryFailure,
                    status.error()
                });
            }
        }
    }

    const QStringList conflicts = findScheduleConflicts(
        importedSchedules,
        existingSchedules
        );

    if (!conflicts.isEmpty())
    {
        return std::unexpected(ClassTransferImportError{
            ClassTransferImportError::Kind::ScheduleConflict,
            RepositoryError{
                RepositoryErrorCode::Conflict,
                QObject::tr("Schedule conflicts prevent this import:\n\n%1")
                    .arg(conflicts.join(QLatin1Char('\n')))
            }
        });
    }

    return {};
}

RepositoryStatus queryFailure(
    const QSqlQuery& query,
    const QString& operation
    )
{
    return std::unexpected(SqlQueryUtils::repositoryErrorFor(
        SqlQueryUtils::errorFor(query, operation)
        ));
}

RepositoryResult<qint64> insertTeacher(
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

    bool idValid = false;
    const qint64 id = query.lastInsertId().toLongLong(&idValid);

    if (!idValid || id <= 0)
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            QObject::tr("The imported teacher did not receive a Teacher Profile ID.")
        });
    }

    return id;
}

RepositoryStatus updateTeacher(
    QSqlDatabase& database,
    qint64 teacherId,
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
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Conflict,
            QObject::tr("The matched teacher no longer exists.")
        });
    }

    return {};
}

RepositoryStatus clearClassData(
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

RepositoryStatus writeClassData(
    QSqlDatabase& database,
    int classId,
    qint64 teacherId,
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
        const QList<ClassTime>& times) -> RepositoryStatus
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

    RepositoryStatus status = writeTimes(
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

        const qint64 evaluationId = query.lastInsertId().toLongLong();

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

ClassMngr::Next::Application::ClassTransferPreviewReadResult
ClassTransferRepository::readSnapshot(
    const ClassMngr::Next::Application::ClassTransferPreviewRequest& request
    ) const
{
    using namespace ClassMngr::Next;
    using Application::ClassTransferMatchingDestinationTeacher;
    using Application::ClassTransferMatchingTeacherNames;
    using Application::ClassTransferPreviewReadResult;
    using Application::ClassTransferPreviewReadSnapshot;

    const auto failure = [](
        const Domain::ErrorCode code,
        const QString& message,
        const bool recoverable = true)
    {
        const QByteArray bytes = message.toUtf8();
        return ClassTransferPreviewReadResult::failure({
            .code = code,
            .message = std::string(bytes.constData(),
                static_cast<std::size_t>(bytes.size())),
            .recoverable = recoverable
        });
    };
    const auto repositoryFailure = [&failure](const RepositoryError& error)
    {
        return failure(
            ClassMngr::Next::Platform::repositoryDomainErrorCode(error.code),
            toLegacyDiagnostic(error),
            error.recoverableOverride.value_or(true)
            );
    };

    if (!m_database.isOpen())
    {
        return failure(
            Domain::ErrorCode::NotFound,
            QObject::tr("No Teacher Profile is open."));
    }

    const auto hasCompleteSourceClass = std::any_of(
        request.classes.cbegin(), request.classes.cend(),
        [](const Application::ClassTransferMatchingSourceClass& source)
        {
            return !source.grade.empty() && !source.level.empty();
        });
    const auto sourceHasAssignedTeacherForAnyCourse = [&]
    {
        return std::any_of(
            request.classes.cbegin(), request.classes.cend(),
            [&request](const Application::ClassTransferMatchingSourceClass& source)
            {
                if (source.grade.empty() || source.level.empty())
                {
                    return false;
                }
                return std::any_of(
                    request.teachers.cbegin(), request.teachers.cend(),
                    [&source](const Application::ClassTransferMatchingSourceTeacher& teacher)
                    {
                        return teacher.sourceKey == source.teacherSourceKey;
                    });
            });
    };
    const bool needTeacherRows = sourceHasAssignedTeacherForAnyCourse()
        || std::any_of(
            request.teachers.cbegin(), request.teachers.cend(),
            [](const Application::ClassTransferMatchingSourceTeacher& teacher)
            {
                return !teacher.names.english.empty()
                    || !teacher.names.korean.empty();
            });

    ClassTransferPreviewReadSnapshot snapshot;
    QHash<qint64, ClassTransferMatchingTeacherNames> teacherNamesById;
    if (needTeacherRows)
    {
        QSqlQuery destinationTeacherQuery(m_database);
        const auto loaded = SqlQueryUtils::execute(
            destinationTeacherQuery,
            QStringLiteral(R"(
                SELECT id, teacher_en, teacher_kr
                FROM teachers
                ORDER BY teacher_en
            )"),
            QObject::tr("Loading teachers for class import")
            );
        if (!loaded)
        {
            return failure(
                Domain::ErrorCode::Technical,
                loaded.error().userMessage());
        }

        while (destinationTeacherQuery.next())
        {
            bool valid = false;
            const qint64 legacyId = destinationTeacherQuery
                .value(QStringLiteral("id")).toLongLong(&valid);
            if (!valid || legacyId <= 0)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    QObject::tr("A teacher identifier could not be read for class import."),
                    false);
            }

            const auto typedId = Domain::TeacherId::fromString(
                std::to_string(legacyId));
            if (!typedId)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    QObject::tr("A teacher identifier could not be read for class import."),
                    false);
            }

            const ClassTransferMatchingTeacherNames names{
                normalized(destinationTeacherQuery
                    .value(QStringLiteral("teacher_en")).toString())
                    .toStdString(),
                normalized(destinationTeacherQuery
                    .value(QStringLiteral("teacher_kr")).toString())
                    .toStdString()
            };
            teacherNamesById.insert(legacyId, names);
            snapshot.teachers.push_back({*typedId, names});
        }

        if (destinationTeacherQuery.lastError().isValid())
        {
            return failure(
                Domain::ErrorCode::Technical,
                destinationTeacherQuery.lastError().text());
        }
    }

    if (!hasCompleteSourceClass)
    {
        return ClassTransferPreviewReadResult::success(std::move(snapshot));
    }

    ClassRepository classRepository(m_database);
    ClassInfoRepository classInfoRepository(m_database);
    TeacherRepository teacherRepository(m_database);
    const RepositoryResult<QList<Classroom>> destinationClasses =
        classRepository.getClasses();
    if (!destinationClasses)
    {
        return repositoryFailure(destinationClasses.error());
    }

    if (destinationClasses->isEmpty())
    {
        return ClassTransferPreviewReadResult::success(std::move(snapshot));
    }

    QList<int> destinationClassIds;
    destinationClassIds.reserve(destinationClasses->size());
    for (const Classroom& destination : *destinationClasses)
    {
        if (destination.id <= 0)
        {
            return failure(
                Domain::ErrorCode::Validation,
                QObject::tr("A class identifier could not be read for class import."),
                false);
        }
        destinationClassIds.append(destination.id);
    }

    const RepositoryResult<QList<ClassNavigationReadRecord>> destinationInfoRecords =
        classInfoRepository.loadClassesNavigationRecords(destinationClassIds);
    if (!destinationInfoRecords)
    {
        return repositoryFailure(destinationInfoRecords.error());
    }
    if (destinationInfoRecords->size() != destinationClasses->size())
    {
        return failure(
            Domain::ErrorCode::Validation,
            QObject::tr("Class information returned a different number of rows for class import."),
            false);
    }

    const auto hasSourceTeacherForCourse = [&request](
        const QString& grade,
        const QString& level)
    {
        const std::string normalizedGrade = normalized(grade).toStdString();
        const std::string normalizedLevel = normalized(level).toStdString();
        return std::any_of(
            request.classes.cbegin(), request.classes.cend(),
            [&request, &normalizedGrade, &normalizedLevel](
                const Application::ClassTransferMatchingSourceClass& source)
            {
                if (source.grade.empty() || source.level.empty()
                    || source.grade != normalizedGrade
                    || source.level != normalizedLevel)
                {
                    return false;
                }
                return std::any_of(
                    request.teachers.cbegin(), request.teachers.cend(),
                    [&source](const Application::ClassTransferMatchingSourceTeacher& teacher)
                    {
                        return teacher.sourceKey == source.teacherSourceKey;
                    });
            });
    };

    snapshot.classes.reserve(static_cast<std::size_t>(destinationClasses->size()));
    for (qsizetype index = 0; index < destinationClasses->size(); ++index)
    {
        const Classroom& destination = destinationClasses->at(index);
        const ClassNavigationReadRecord& info = destinationInfoRecords->at(index);
        const QString grade = normalized(info.grade);
        const QString level = normalized(info.level);

        std::optional<ClassTransferMatchingDestinationTeacher> teacher;
        if (info.teacherId > 0)
        {
            const auto typedTeacherId = Domain::TeacherId::fromString(
                std::to_string(info.teacherId));
            if (!typedTeacherId)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    QObject::tr("A teacher identifier could not be read for class import."),
                    false);
            }

            ClassTransferMatchingTeacherNames names;
            if (hasSourceTeacherForCourse(grade, level))
            {
                const auto existingNames = teacherNamesById.constFind(
                    info.teacherId);
                if (existingNames != teacherNamesById.cend())
                {
                    names = *existingNames;
                }
                else
                {
                    const RepositoryResult<TeacherProfileReadRecord> profile =
                        teacherRepository.lookupTeacherProfileById(
                            info.teacherId);
                    if (!profile)
                    {
                        return failure(
                            Domain::ErrorCode::Technical,
                            toLegacyDiagnostic(profile.error())
                            );
                    }
                    names = {
                        normalized(profile->teacherEn).toStdString(),
                        normalized(profile->teacherKr).toStdString()
                    };
                }
            }

            teacher = ClassTransferMatchingDestinationTeacher{
                *typedTeacherId,
                std::move(names)
            };
        }

        const auto typedClassId = Domain::ClassId::fromString(
            std::to_string(destination.id));
        if (!typedClassId)
        {
            return failure(
                Domain::ErrorCode::Validation,
                QObject::tr("A class identifier could not be read for class import."),
                false);
        }

        snapshot.classes.push_back({
            *typedClassId,
            grade.toStdString(),
            level.toStdString(),
            std::move(teacher)
        });
    }

    return ClassTransferPreviewReadResult::success(std::move(snapshot));
}

RepositoryResult<ClassTransferExportSource> ClassTransferRepository::readExportSource(
    const QList<int>& classIds
    )
{
    if (!m_database.isOpen())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr("No Teacher Profile is open.")
        });
    }

    if (classIds.isEmpty())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::InvalidInput,
            QObject::tr("No classes were selected.")
        });
    }

    DatabaseTransaction transaction(m_database);

    if (!transaction.started())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to start the class export transaction: %1")
                .arg(m_database.lastError().text())
        });
    }

    ClassRepository classRepository(m_database);
    ClassInfoRepository classInfoRepository(m_database);
    RosterRepository rosterRepository(m_database);
    TeacherRepository teacherRepository(m_database);

    ClassTransferExportSource source;
    source.exportedAtUtc = QDateTime::currentDateTimeUtc();
    QSet<int> seenClasses;
    QSet<qint64> seenTeachers;
    source.classes.reserve(classIds.size());

    // Resolve selections in order and stop at the first known selection or
    // class lookup failure. Only this validated prefix is passed to the data
    // batches, so a later invalid selection cannot mask an earlier failure.
    for (int index = 0; index < classIds.size(); ++index)
    {
        const int classId = classIds[index];

        if (classId <= 0 || seenClasses.contains(classId))
        {
            source.selectionOrReadError = RepositoryError{
                RepositoryErrorCode::InvalidInput,
                QObject::tr(
                    "The class selection contains an invalid or duplicate class.")
            };
            break;
        }

        const RepositoryResult<Classroom> classroom =
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
        classInfoRepository.loadClassTransferExportClassInfoRecords(exportClassIds);
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
            RepositoryResult<ClassTransferExportClassInfoReadRecord> info =
                classInfoRepository.loadClassTransferExportClassInfoRecord(pending.classId);
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
            RepositoryResult<Roster> roster =
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
        if (pending.infoError || pending.rosterError)
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
        if (pending.evaluationError)
        {
            break;
        }
    }

    if (!source.teacherIds.isEmpty())
    {
        const RepositoryResult<QList<ClassTransferExportTeacherProfileBatchReadRecord>> teacherProfiles =
            teacherRepository.loadClassTransferExportTeacherProfileRecords(source.teacherIds);
        if (!teacherProfiles)
        {
            // A batch-level failure occupies the first assigned-teacher
            // profile's position in the original sequence.
            source.teacherBatchError = teacherProfiles.error();
        }
        else if (teacherProfiles->size() != source.teacherIds.size())
        {
            source.teacherBatchError = RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "Loading teacher profiles failed: the database returned an unexpected number of records.")
            };
        }
        else
        {
            for (qsizetype index = 0; index < source.teacherIds.size(); ++index)
            {
                const qint64 teacherId = source.teacherIds[index];
                const ClassTransferExportTeacherProfileBatchReadRecord& record =
                    teacherProfiles->at(index);
                if (record.teacherId != teacherId)
                {
                    source.teacherBatchError = RepositoryError{
                        RepositoryErrorCode::Validation,
                        QObject::tr(
                            "Loading teacher profiles failed: the database returned records in a different identifier order.")
                    };
                    break;
                }

                if (!record.profile)
                {
                    source.teacherErrorsById.insert(
                        teacherId,
                        record.profile.error()
                        );
                    continue;
                }

                if (record.profile->teacherId != teacherId)
                {
                    source.teacherBatchError = RepositoryError{
                        RepositoryErrorCode::Validation,
                        QObject::tr(
                            "Loading teacher profiles failed: a database record returned a different teacher identifier.")
                    };
                    break;
                }

                source.teachersById.insert(teacherId, *record.profile);
            }
        }
    }

    bool readFailed = source.selectionOrReadError.has_value()
        || source.teacherBatchError.has_value()
        || !source.teacherErrorsById.isEmpty();
    for (const auto& pending : source.classes)
        readFailed = readFailed || pending.infoError.has_value()
            || pending.rosterError.has_value()
            || pending.evaluationError.has_value();
    if (readFailed)
    {
        if (!transaction.rollbackChecked())
        {
            if (RepositoryError* primary = firstExportError(source))
            {
                primary->code = RepositoryErrorCode::Technical;
            }
        }
        return source;
    }

    if (!transaction.commit())
    {
        const QString commitDiagnostic = m_database.lastError().text();
        RepositoryError error{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to finish the class export transaction: %1")
                .arg(commitDiagnostic)
        };
        if (!transaction.rollbackChecked())
        {
            error.code = RepositoryErrorCode::Technical;
        }
        return std::unexpected(std::move(error));
    }
    return source;
}

RepositoryResult<ClassTransferPackage> ClassTransferRepository::buildPackage(const QList<int>& classIds)
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
        if (pending.infoError)
        {
            return std::unexpected(*pending.infoError);
        }
        if (pending.rosterError)
        {
            return std::unexpected(*pending.rosterError);
        }

        if (pending.teacherId > 0)
        {
            if (teacherBatchError)
            {
                return std::unexpected(*teacherBatchError);
            }
            const auto teacherError =
                teacherErrorsById.constFind(pending.teacherId);
            if (teacherError != teacherErrorsById.cend())
            {
                return std::unexpected(*teacherError);
            }
        }

        if (pending.evaluationError)
        {
            return std::unexpected(*pending.evaluationError);
        }
    }

    if (selectionOrReadError)
    {
        return std::unexpected(*selectionOrReadError);
    }

    QHash<qint64, QString> teacherKeys;
    for (const qint64 teacherId : teacherIds)
    {
        const QString key =
            QStringLiteral("teacher-%1").arg(package.teachers.size() + 1);
        teacherKeys.insert(teacherId, key);
        const auto& profile = teachersById[teacherId];
        Teacher teacher;
        // Portable legacy payloads do not serialize local identity. Preserve
        // representable IDs for legacy callers without truncating wide IDs.
        teacher.id = teacherId <= std::numeric_limits<int>::max()
            ? static_cast<int>(teacherId) : -1;
        teacher.teacherKr = profile.teacherKr;
        teacher.teacherEn = profile.teacherEn;
        teacher.preferredRomanization = profile.preferredRomanization;
        teacher.preferredName = profile.preferredName;
        teacher.roomNumber = profile.roomNumber;
        teacher.birthday = profile.birthday;
        teacher.phoneNumber = profile.phoneNumber;
        teacher.wifiName = profile.wifiName;
        teacher.wifiPassword = profile.wifiPassword;
        teacher.internetType = profile.internetType;
        teacher.zoomId = profile.zoomId;
        teacher.zoomPassword = profile.zoomPassword;
        teacher.projectionType = profile.projectionType;
        teacher.notes = profile.notes;
        package.teachers.append({key, std::move(teacher)});
    }

    for (ClassTransferExportSourceClass& pending : pendingClasses)
    {
        ClassTransferClass transferClass;
        transferClass.key = QStringLiteral("class-%1")
            .arg(pending.selectedIndex + 1);
        transferClass.name = pending.name;
        // Project the portable class fields; local IDs remain unassigned.
        transferClass.info.classGrade = std::move(pending.info.classGrade);
        transferClass.info.classLevel = std::move(pending.info.classLevel);
        transferClass.info.readingBook = std::move(pending.info.readingBook);
        transferClass.info.essayBook = std::move(pending.info.essayBook);
        transferClass.info.classColor = std::move(pending.info.classColor);
        transferClass.info.fontColor = std::move(pending.info.fontColor);
        transferClass.info.classTimes = std::move(pending.info.classTimes);
        transferClass.info.intensiveTimes = std::move(pending.info.intensiveTimes);
        transferClass.info.notes = std::move(pending.info.notes);
        transferClass.info.timeFillerActivities = std::move(pending.info.timeFillerActivities);
        transferClass.info.teacherPreferredName = std::move(pending.info.teacherPreferredName);
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

RepositoryResult<ClassImportPreview> ClassTransferRepository::previewImport(
    const ClassTransferPackage& package
    )
{
    return buildPreview(*this, package);
}

RepositoryResult<ClassImportSummary> ClassTransferRepository::importClasses(
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    )
{
    return importClasses(package,
        ClassMngr::Next::Platform::classTransferApplyRequest(plan));
}

RepositoryResult<ClassImportSummary> ClassTransferRepository::importClasses(
    const ClassTransferPackage& package,
    const ClassMngr::Next::Application::ClassTransferApplyRequest& request
    )
{
    const ClassTransferDetailedImportResult result =
        importClassesWithFailureDetails(package, request);
    if (!result)
    {
        return std::unexpected(result.error().repositoryError);
    }
    return *result;
}

ClassTransferDetailedImportResult
ClassTransferRepository::importClassesWithFailureDetails(
    const ClassTransferPackage& package,
    const ClassMngr::Next::Application::ClassTransferApplyRequest& request
    )
{
    return importClassesWithFailureDetailsImpl(package, request, std::nullopt);
}

ClassTransferDetailedImportResult
ClassTransferRepository::importClassesWithBoundedVisibleClassCount(
    const ClassTransferPackage& package,
    const ClassMngr::Next::Application::ClassTransferApplyRequest& request,
    const ClassMngr::Next::Application::ClassTransferVisibleClassLimit limit
    )
{
    return importClassesWithFailureDetailsImpl(package, request, limit);
}

ClassTransferDetailedImportResult
ClassTransferRepository::importClassesWithFailureDetailsImpl(
    const ClassTransferPackage& package,
    const ClassMngr::Next::Application::ClassTransferApplyRequest& request,
    const std::optional<
        ClassMngr::Next::Application::ClassTransferVisibleClassLimit> limit
    )
{
    if (limit && (limit->rawValue() == 0
        || limit->rawValue() > static_cast<std::size_t>(
            std::numeric_limits<int>::max())))
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::InvalidInput,
            QObject::tr("The visible Classes import limit is invalid.")
        });
    }

    if (!m_database.isOpen())
    {
        return std::unexpected(RepositoryError{
            RepositoryErrorCode::NotFound,
            QObject::tr("No Teacher Profile is open.")
        });
    }

    const auto validatedResult = validatePlan(m_database, package, request);

    if (!validatedResult)
    {
        return std::unexpected(validatedResult.error());
    }

    const auto schedulesReady = preflightSchedules(
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
        return std::unexpected(ClassTransferImportError{
            RepositoryError{
                RepositoryErrorCode::Technical,
                QObject::tr("Unable to start the class import transaction: %1")
                    .arg(m_database.lastError().text())
            }
        });
    }

    const auto failTransaction = [&transaction](
        RepositoryError error,
        const ClassTransferImportError::Kind kind =
            ClassTransferImportError::Kind::RepositoryFailure
        ) -> ClassTransferDetailedImportResult
    {
        const QString commitContext =
            QObject::tr("Unable to commit the class import transaction:");
        if (error.code == RepositoryErrorCode::Technical
            && error.diagnostic.contains(
                QStringLiteral("FOREIGN KEY constraint failed"),
                Qt::CaseInsensitive
                )
            && !error.diagnostic.contains(commitContext))
        {
            error.diagnostic = QObject::tr(
                "Unable to commit the class import transaction: %1"
                ).arg(error.diagnostic);
        }
        if (!transaction.rollbackChecked())
        {
            error.code = RepositoryErrorCode::Technical;
        }
        return std::unexpected(ClassTransferImportError{
            kind,
            std::move(error)
        });
    };

    if (limit)
    {
        std::size_t createCount = 0;
        for (auto iterator = validatedResult->classes.cbegin();
             iterator != validatedResult->classes.cend(); ++iterator)
        {
            if (iterator.value().action == ClassImportAction::Create)
            {
                ++createCount;
            }
        }

        QSqlQuery visibleClassCountQuery(m_database);
        // Match ClassRepository::getBoundedClassesForNavigation: testing
        // classes are not visible in the Next Classes list.
        if (!visibleClassCountQuery.exec(QStringLiteral(R"(
            SELECT COUNT(*)
            FROM classes c
            LEFT JOIN testing_classes tc
            ON tc.class_id = c.id
            WHERE tc.class_id IS NULL
        )")))
        {
            return failTransaction(queryFailure(
                visibleClassCountQuery,
                QObject::tr("Counting visible classes before import")
                ).error());
        }
        if (!visibleClassCountQuery.next())
        {
            return failTransaction(RepositoryError{
                RepositoryErrorCode::Technical,
                QObject::tr(
                    "Unable to count visible classes before import: %1")
                    .arg(visibleClassCountQuery.lastError().text())
            });
        }
        bool existingCountValid = false;
        const qint64 existingVisibleClassCount =
            visibleClassCountQuery.value(0).toLongLong(&existingCountValid);
        if (!existingCountValid || existingVisibleClassCount < 0)
        {
            return failTransaction(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "The visible Classes count is outside the supported range.")
            });
        }

        const std::uintmax_t existingCount = static_cast<std::uintmax_t>(
            existingVisibleClassCount);
        const std::uintmax_t createsCount =
            static_cast<std::uintmax_t>(createCount);
        if (createsCount > std::numeric_limits<std::uintmax_t>::max()
                - existingCount)
        {
            return failTransaction(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "The projected visible Classes count is outside the supported range.")
            });
        }
        const std::uintmax_t projectedVisibleClassCount =
            existingCount + createsCount;
        if (projectedVisibleClassCount > static_cast<std::uintmax_t>(
                limit->rawValue()))
        {
            return failTransaction(RepositoryError{
                RepositoryErrorCode::Validation,
                QObject::tr(
                    "The class import would exceed the visible Classes limit of %1.")
                    .arg(static_cast<qulonglong>(limit->rawValue()))
            });
        }
    }

    QHash<QString, qint64> teacherIds;

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
                return failTransaction(teacherId.error());
            }

            teacherIds.insert(transferTeacher.key, *teacherId);
        }
        else
        {
            if (resolution.action == TeacherImportAction::ReplaceExisting)
            {
                const RepositoryStatus updated = updateTeacher(
                    m_database,
                    resolution.targetTeacherId,
                    transferTeacher.teacher
                    );

                if (!updated)
                {
                    return failTransaction(updated.error());
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
                return failTransaction(queryFailure(
                    query,
                    QObject::tr("Creating an imported class")
                    ).error());
            }

            bool classIdValid = false;
            const qint64 generatedClassId = query.lastInsertId().toLongLong(
                &classIdValid);

            if (!classIdValid || generatedClassId <= 0
                || generatedClassId > std::numeric_limits<int>::max())
            {
                return failTransaction(RepositoryError{
                    RepositoryErrorCode::Validation,
                    QObject::tr("The imported class did not receive a Teacher Profile ID.")
                });
            }

            classId = static_cast<int>(generatedClassId);
            summary.createdClassIds.append(classId);
        }
        else
        {
            query.prepare(QStringLiteral("UPDATE classes SET name=? WHERE id=?"));
            query.addBindValue(transferClass.name);
            query.addBindValue(classId);

            if (!query.exec())
            {
                return failTransaction(queryFailure(
                    query,
                    QObject::tr("Updating a replaced class")
                    ).error());
            }
            if (query.numRowsAffected() != 1)
            {
                const RepositoryError diagnostic = queryFailure(
                    query,
                    QObject::tr("Updating a replaced class")
                    ).error();
                return failTransaction(RepositoryError{
                    RepositoryErrorCode::Conflict,
                    diagnostic.diagnostic
                });
            }

            const RepositoryStatus cleared = clearClassData(m_database, classId);

            if (!cleared)
            {
                return failTransaction(cleared.error());
            }

            summary.replacedClassIds.append(classId);
        }

        const qint64 teacherId = transferClass.teacherKey.isEmpty()
            ? -1
            : teacherIds.value(transferClass.teacherKey, -1);
        const RepositoryStatus written = writeClassData(
            m_database, classId, teacherId, transferClass);

        if (!written)
        {
            return failTransaction(written.error());
        }
    }

    if (!transaction.commit())
    {
        const QString commitDiagnostic = m_database.lastError().text();
        return failTransaction(RepositoryError{
            RepositoryErrorCode::Technical,
            QObject::tr("Unable to commit the class import transaction: %1")
                .arg(commitDiagnostic)
        });
    }

    return summary;
}
