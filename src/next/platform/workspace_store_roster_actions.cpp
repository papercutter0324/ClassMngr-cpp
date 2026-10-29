#include "next/platform/workspace_store_roster_actions.h"

#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/classes_list_read_snapshot.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/application/roster_custom_column_name_policy.h"
#include "next/application/roster_row_availability.h"
#include "next/application/roster_transfer_target_eligibility.h"
#include "next/application/speaking_evaluation_roster_score_import_use_case.h"
#include "next/platform/repository_error_code.h"

#include <QHash>
#include <QByteArray>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QStringView>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{
namespace
{

[[nodiscard]] std::optional<int> legacyClassId(const Domain::ClassId& id)
{
    int parsed = 0;
    const std::string& value = id.value();
    const auto [end, error] = std::from_chars(
        value.data(), value.data() + value.size(), parsed, 10);
    if (error != std::errc{} || end != value.data() + value.size()
        || parsed <= 0 || std::to_string(parsed) != value)
    {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

[[nodiscard]] QString legacyText(const std::u16string& value)
{
    QString result;
    result.reserve(static_cast<qsizetype>(value.size()));
    for (const char16_t unit : value)
    {
        result.append(QChar(unit));
    }
    return result;
}

[[nodiscard]] std::string legacyText(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

[[nodiscard]] Domain::OperationError repositoryError(
    const RepositoryError& source,
    const char* fallback
    )
{
    const QString diagnostic = toLegacyDiagnostic(source);
    const std::string message = legacyText(diagnostic);
    return {
        .code = repositoryDomainErrorCode(source.code),
        .message = message.empty() ? std::string(fallback) : message,
        .recoverable = source.recoverableOverride.value_or(
            source.code != RepositoryErrorCode::Technical)
    };
}

[[nodiscard]] Domain::OperationError genericError(
    const Domain::ErrorCode code,
    const char* message,
    const bool recoverable = true
    )
{
    return {.code = code, .message = message, .recoverable = recoverable};
}

[[nodiscard]] Application::RosterSnapshot applicationSnapshot(
    const Roster& source
    )
{
    Application::RosterSnapshot result;
    result.columns.reserve(static_cast<std::size_t>(source.columns.size()));
    for (const QString& column : source.columns)
    {
        result.columns.push_back(column.toStdU16String());
    }
    result.columnWidths.reserve(static_cast<std::size_t>(source.columnWidths.size()));
    for (const int width : source.columnWidths)
    {
        result.columnWidths.push_back(
            Application::RosterSnapshotColumnWidth{width});
    }
    result.rows.reserve(static_cast<std::size_t>(source.rows.size()));
    for (const QStringList& sourceRow : source.rows)
    {
        std::vector<std::u16string> row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& cell : sourceRow)
        {
            row.push_back(cell.toStdU16String());
        }
        result.rows.push_back(std::move(row));
    }
    return result;
}

[[nodiscard]] Roster legacyRoster(const Application::RosterSnapshot& source)
{
    Roster result;
    result.columns.reserve(static_cast<qsizetype>(source.columns.size()));
    for (const std::u16string& column : source.columns)
    {
        result.columns.append(legacyText(column));
    }
    result.columnWidths.reserve(static_cast<qsizetype>(source.columnWidths.size()));
    for (const Application::RosterSnapshotColumnWidth width : source.columnWidths)
    {
        result.columnWidths.append(width.rawValue());
    }
    result.rows.reserve(static_cast<qsizetype>(source.rows.size()));
    for (const auto& sourceRow : source.rows)
    {
        QStringList row;
        row.reserve(static_cast<qsizetype>(sourceRow.size()));
        for (const std::u16string& cell : sourceRow)
        {
            row.append(legacyText(cell));
        }
        result.rows.append(std::move(row));
    }
    return result;
}

[[nodiscard]] bool caseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return QStringView(left.data(), static_cast<qsizetype>(left.size())).compare(
        QStringView(right.data(), static_cast<qsizetype>(right.size())),
        Qt::CaseInsensitive) == 0;
}

class WorkspaceStoreRosterWindowReadPort final
    : public Application::RosterWindowReadPort
{
public:
    explicit WorkspaceStoreRosterWindowReadPort(RosterRepository* repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] Application::RosterWindowReadResult readRosterWindow(
        const Application::RosterWindowReadQuery& query
        ) const override
    {
        const auto classId = legacyClassId(query.classId);
        if (!classId)
        {
            return Application::RosterWindowReadResult::failure(genericError(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."));
        }
        if (!m_repository)
        {
            return Application::RosterWindowReadResult::failure(genericError(
                Domain::ErrorCode::Technical,
                "The roster repository is unavailable."));
        }

        try
        {
            const auto loaded = m_repository->loadRosterWindowDetailed(
                *classId,
                static_cast<int>(Application::RosterModeledRowCount));
            if (!loaded)
            {
                return Application::RosterWindowReadResult::failure(
                    repositoryError(loaded.error(), "The roster could not be loaded."));
            }
            if (loaded->parentMissing)
            {
                return Application::RosterWindowReadResult::failure(genericError(
                    Domain::ErrorCode::NotFound,
                    "The selected class was not found."));
            }
            return Application::RosterWindowReadResult::success({
                .roster = applicationSnapshot(loaded->roster),
                .hasRowsOutsideWindow = loaded->hasRowsOutsideWindow
            });
        }
        catch (const std::exception&)
        {
            return Application::RosterWindowReadResult::failure(genericError(
                Domain::ErrorCode::Technical, "The roster could not be loaded."));
        }
        catch (...)
        {
            return Application::RosterWindowReadResult::failure(genericError(
                Domain::ErrorCode::Technical, "The roster could not be loaded."));
        }
    }

private:
    RosterRepository* m_repository = nullptr;
};

class WorkspaceStoreRosterSavePort final : public Application::RosterSavePort
{
public:
    explicit WorkspaceStoreRosterSavePort(RosterRepository* repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] Domain::Result<void> saveRoster(
        const Application::PreparedRosterSaveRequest& request
        ) const override
    {
        const auto classId = legacyClassId(request.classId);
        if (!classId)
        {
            return Domain::Result<void>::failure(genericError(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."));
        }
        if (!m_repository)
        {
            return Domain::Result<void>::failure(genericError(
                Domain::ErrorCode::Technical,
                "The roster repository is unavailable."));
        }

        try
        {
            QStringList removedColumns;
            for (const std::u16string& column : request.removedCustomColumns)
            {
                removedColumns.append(legacyText(column));
            }
            const RepositoryStatus saved =
                m_repository->saveRosterWindowsWithParentCheck({{
                    .classId = *classId,
                    .roster = legacyRoster(request.roster),
                    .removedCustomColumns = std::move(removedColumns)
                }});
            if (!saved)
            {
                return Domain::Result<void>::failure(repositoryError(
                    saved.error(), "The roster could not be saved."));
            }
            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(genericError(
                Domain::ErrorCode::Technical, "The roster could not be saved."));
        }
        catch (...)
        {
            return Domain::Result<void>::failure(genericError(
                Domain::ErrorCode::Technical, "The roster could not be saved."));
        }
    }

private:
    RosterRepository* m_repository = nullptr;
};

class WorkspaceStoreSpeakingEvaluationReadPort final
    : public Application::SpeakingEvaluationReadPort
{
public:
    explicit WorkspaceStoreSpeakingEvaluationReadPort(
        SpeakingEvalRepository* repository
        ) noexcept
        : m_repository(repository)
    {
    }

    [[nodiscard]] Application::SpeakingEvaluationReadResult readEvaluation(
        const Application::SpeakingEvaluationReadQuery& query
        ) const override
    {
        const auto classId = legacyClassId(query.classId);
        if (!classId)
        {
            return Application::SpeakingEvaluationReadResult::failure(
                genericError(Domain::ErrorCode::InvalidInput,
                    "Class ID must be a canonical positive integer."));
        }
        if (!m_repository)
        {
            return Application::SpeakingEvaluationReadResult::failure(
                genericError(Domain::ErrorCode::NotFound,
                    "The speaking evaluation repository is unavailable."));
        }

        try
        {
            Application::SpeakingEvaluationReadSnapshot snapshot{
                .classId = query.classId,
                .evaluationName = query.evaluationName
            };
            const auto loaded = m_repository->forEachSpeakingEvalRowDetailed(
                *classId,
                legacyText(query.evaluationName),
                [&snapshot](const QStringList& row) {
                    std::vector<std::u16string> applicationRow;
                    applicationRow.reserve(static_cast<std::size_t>(row.size()));
                    for (const QString& cell : row)
                    {
                        applicationRow.push_back(cell.toStdU16String());
                    }
                    snapshot.rows.push_back(std::move(applicationRow));
                });
            if (!loaded)
            {
                return Application::SpeakingEvaluationReadResult::failure(
                    repositoryError(loaded.error(),
                        "The speaking evaluation could not be loaded."));
            }
            return Application::SpeakingEvaluationReadResult::success(
                std::move(snapshot));
        }
        catch (const std::exception&)
        {
            return Application::SpeakingEvaluationReadResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "The speaking evaluation could not be loaded."));
        }
        catch (...)
        {
            return Application::SpeakingEvaluationReadResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "The speaking evaluation could not be loaded."));
        }
    }

private:
    SpeakingEvalRepository* m_repository = nullptr;
};

[[nodiscard]] Domain::Result<void> saveTransferPair(
    RosterRepository* repository,
    const Application::RosterRowTransferSaveRequest& request
    )
{
    const auto sourceId = legacyClassId(request.sourceClassId);
    const auto targetId = legacyClassId(request.targetClassId);
    if (!sourceId || !targetId || *sourceId == *targetId)
    {
        return Domain::Result<void>::failure(genericError(
            Domain::ErrorCode::InvalidInput,
            "Transfer requires distinct canonical positive class IDs."));
    }
    if (!repository)
    {
        return Domain::Result<void>::failure(genericError(
            Domain::ErrorCode::NotFound,
            "The roster repository is unavailable."));
    }

    try
    {
        QStringList removedColumns;
        for (const std::u16string& column : request.sourceRemovedCustomColumns)
        {
            removedColumns.append(legacyText(column));
        }
        const RepositoryStatus saved = repository->saveRosterWindowsWithParentCheck({{
            {
                .classId = *sourceId,
                .roster = legacyRoster(request.sourceRoster),
                .removedCustomColumns = std::move(removedColumns)
            },
            {
                .classId = *targetId,
                .roster = legacyRoster(request.targetRoster),
                .removedCustomColumns = {}
            }
        }});
        if (!saved)
        {
            return Domain::Result<void>::failure(repositoryError(
                saved.error(), "The student transfer could not be saved."));
        }
        return Domain::Result<void>::success();
    }
    catch (const std::exception&)
    {
        return Domain::Result<void>::failure(genericError(
            Domain::ErrorCode::Technical,
            "The student transfer could not be saved."));
    }
    catch (...)
    {
        return Domain::Result<void>::failure(genericError(
            Domain::ErrorCode::Technical,
            "The student transfer could not be saved."));
    }
}

template <typename Result>
[[nodiscard]] Result storeOpenFailure(const RepositoryStatus& opened)
{
    return Result::failure(repositoryError(
        opened.error(), "The selected workspace could not be opened."));
}

} // namespace

Application::RosterWindowReadResult readRosterWindowAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId
    )
{
    const auto legacyId = legacyClassId(classId);
    if (!legacyId)
    {
        return Application::RosterWindowReadResult::failure(genericError(
            Domain::ErrorCode::InvalidInput,
            "Class ID must be a canonical positive integer."));
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return storeOpenFailure<Application::RosterWindowReadResult>(opened);
    }
    WorkspaceStoreRosterWindowReadPort port(store.rosterRepository());
    return Application::RosterWindowReadUseCase::execute(
        {.classId = classId}, port);
}

Domain::Result<void> saveRosterAtPath(
    const QString& databasePath,
    const Application::RosterSaveRequest& request
    )
{
    const auto legacyId = legacyClassId(request.classId);
    if (!legacyId)
    {
        return Domain::Result<void>::failure(genericError(
            Domain::ErrorCode::InvalidInput,
            "Class ID must be a canonical positive integer."));
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return storeOpenFailure<Domain::Result<void>>(opened);
    }
    WorkspaceStoreRosterSavePort port(store.rosterRepository());
    return Application::RosterSaveUseCase::execute(
        request, port, caseInsensitiveEquals);
}

Domain::Result<bool> rosterColumnHasTailDataAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId,
    const std::u16string& columnName
    )
{
    const auto legacyId = legacyClassId(classId);
    if (!legacyId)
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::InvalidInput,
            "Class ID must be a canonical positive integer."));
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return storeOpenFailure<Domain::Result<bool>>(opened);
    }

    QSqlQuery columns(store.database());
    columns.prepare(QStringLiteral(
        "SELECT name FROM roster_columns WHERE class_id=? ORDER BY position, id"));
    columns.addBindValue(*legacyId);
    if (!columns.exec())
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Technical,
            "The roster columns could not be checked."));
    }

    const std::u16string normalizedRequestedName =
        Application::normalizeRosterCustomColumnName(
            columnName, caseInsensitiveEquals);
    if (normalizedRequestedName.empty())
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Validation,
            "The roster column name cannot be mapped unambiguously."));
    }

    int sourceColumnIndex = -1;
    int matchingColumnCount = 0;
    int orderedColumnIndex = 0;
    while (columns.next())
    {
        const std::u16string normalizedStoredName =
            Application::normalizeRosterCustomColumnName(
                columns.value(0).toString().toStdU16String(),
                caseInsensitiveEquals);
        if (!normalizedStoredName.empty()
            && caseInsensitiveEquals(normalizedStoredName, normalizedRequestedName))
        {
            sourceColumnIndex = orderedColumnIndex;
            ++matchingColumnCount;
        }
        ++orderedColumnIndex;
    }
    if (columns.lastError().isValid())
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Technical,
            "The roster columns could not be checked."));
    }
    columns.finish();
    if (matchingColumnCount != 1 || sourceColumnIndex < 0)
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Validation,
            "The roster column name cannot be mapped unambiguously."));
    }

    QSqlQuery tail(store.database());
    tail.prepare(QStringLiteral(
        "SELECT EXISTS(SELECT 1 FROM roster_data "
        "WHERE class_id=? AND row_index>=? AND col_index=?)"));
    tail.addBindValue(*legacyId);
    tail.addBindValue(static_cast<int>(Application::RosterModeledRowCount));
    tail.addBindValue(sourceColumnIndex);
    if (!tail.exec() || !tail.next())
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Technical,
            "The stored roster rows could not be checked."));
    }
    const bool hasTailData = tail.value(0).toBool();
    if (tail.lastError().isValid())
    {
        return Domain::Result<bool>::failure(genericError(
            Domain::ErrorCode::Technical,
            "The stored roster rows could not be checked."));
    }
    return Domain::Result<bool>::success(hasTailData);
}

Application::SpeakingEvaluationRosterScoreImportBatchResult
readSpeakingEvaluationRosterScoresAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId,
    const std::vector<std::u16string>& evaluationNames
    )
{
    Application::SpeakingEvaluationRosterScoreImportBatchItems imports;
    imports.reserve(evaluationNames.size());
    if (evaluationNames.empty())
    {
        return Application::SpeakingEvaluationRosterScoreImportBatchResult::success(
            std::move(imports));
    }

    if (!Application::isCanonicalPositiveDecimalId(classId.value()))
    {
        return Application::SpeakingEvaluationRosterScoreImportBatchResult::failure(
            genericError(Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."));
    }
    for (const std::u16string& evaluationName : evaluationNames)
    {
        if (Application::trimQtWhitespace(evaluationName).empty())
        {
            return Application::SpeakingEvaluationRosterScoreImportBatchResult::failure(
                genericError(Domain::ErrorCode::InvalidInput,
                    "Evaluation names must not be empty."));
        }
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return storeOpenFailure<
            Application::SpeakingEvaluationRosterScoreImportBatchResult>(opened);
    }
    SpeakingEvalRepository* const repository = store.speakingEvalRepository();
    if (!repository)
    {
        return Application::SpeakingEvaluationRosterScoreImportBatchResult::failure(
            genericError(Domain::ErrorCode::NotFound,
                "The speaking evaluation repository is unavailable."));
    }

    const WorkspaceStoreSpeakingEvaluationReadPort readPort(repository);
    for (const std::u16string& evaluationName : evaluationNames)
    {
        auto imported = Application::SpeakingEvaluationRosterScoreImportUseCase::execute(
            {.classId = classId, .evaluationName = evaluationName}, readPort);
        if (!imported)
        {
            // A caller must not apply a partial group of evaluations.
            return Application::SpeakingEvaluationRosterScoreImportBatchResult::failure(
                imported.error());
        }
        imports.push_back(std::move(imported));
    }
    return Application::SpeakingEvaluationRosterScoreImportBatchResult::success(
        std::move(imports));
}

Application::RosterTransferTargetChoicesResult
readRosterTransferTargetChoicesAtPath(
    const QString& databasePath,
    const Domain::ClassId& sourceClassId
    )
{
    const auto sourceLegacyId = legacyClassId(sourceClassId);
    if (!sourceLegacyId)
    {
        return Application::RosterTransferTargetChoicesResult::failure(
            genericError(Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."));
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return storeOpenFailure<
            Application::RosterTransferTargetChoicesResult>(opened);
    }
    ClassRepository* const classRepository = store.classRepository();
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    RosterRepository* const rosterRepository = store.rosterRepository();
    TeacherRepository* const teacherRepository = store.teacherRepository();
    if (!classRepository || !infoRepository || !rosterRepository
        || !teacherRepository)
    {
        return Application::RosterTransferTargetChoicesResult::failure(
            genericError(Domain::ErrorCode::NotFound,
                "Roster transfer class data is unavailable."));
    }

    try
    {
        const auto classes = classRepository->getBoundedClassesForNavigation(
            static_cast<int>(Application::kClassesNavigationMaximumClasses),
            static_cast<int>(Application::kClassesNavigationMaximumTextLength));
        if (!classes)
        {
            return Application::RosterTransferTargetChoicesResult::failure(
                repositoryError(classes.error(),
                    "Transfer classes could not be loaded."));
        }

        QList<ClassNavigationBoundedReadRequestEntry> requestedClasses;
        requestedClasses.reserve(classes->size());
        for (const Classroom& classroom : *classes)
        {
            requestedClasses.append({
                .classId = classroom.id,
                .className = classroom.name
            });
        }
        const ClassNavigationBoundedReadLimits limits{
            .maximumClassCount = static_cast<int>(
                Application::kClassesNavigationMaximumClasses),
            .maximumTextLength = static_cast<int>(
                Application::kClassesNavigationMaximumTextLength),
            .maximumScheduleRowsPerSourceAndClass = static_cast<int>(
                Application::kClassesNavigationMaximumScheduleRows),
            .maximumScheduleTokenLength = static_cast<int>(
                Application::kClassesNavigationMaximumScheduleTokenLength)
        };
        const auto metadata = infoRepository->loadBoundedClassesNavigationRecords(
            requestedClasses, limits);
        if (!metadata)
        {
            return Application::RosterTransferTargetChoicesResult::failure(
                repositoryError(metadata.error(),
                    "Transfer class details could not be loaded."));
        }
        if (metadata->size() != requestedClasses.size())
        {
            return Application::RosterTransferTargetChoicesResult::failure(
                genericError(Domain::ErrorCode::Validation,
                    "Transfer class details returned an unexpected record count."));
        }

        const ClassNavigationReadRecord* sourceDetails = nullptr;
        for (const ClassNavigationBoundedReadRecord& record : *metadata)
        {
            if (record.details.classId == *sourceLegacyId
                && record.details.hasClassInfo)
            {
                sourceDetails = &record.details;
                break;
            }
        }
        if (!sourceDetails)
        {
            return Application::RosterTransferTargetChoicesResult::failure(
                genericError(Domain::ErrorCode::NotFound,
                    "The source class grade could not be loaded."));
        }

        const std::u16string sourceGrade =
            sourceDetails->grade.toStdU16String();
        Application::RosterTransferTargetChoicesSnapshot snapshot{
            .sourceGrade = sourceGrade
        };
        if (!Application::isRosterTransferSourceEligible(
                sourceClassId, sourceGrade))
        {
            return Application::RosterTransferTargetChoicesResult::success(
                std::move(snapshot));
        }

        struct EligibleTarget final
        {
            const ClassNavigationBoundedReadRecord* record = nullptr;
            Domain::ClassId classId;
        };
        std::vector<EligibleTarget> eligibleTargets;
        QList<qint64> requestedTeacherIds;
        QSet<qint64> seenTeacherIds;
        for (std::size_t index = 0; index < metadata->size(); ++index)
        {
            const ClassNavigationBoundedReadRecord& record = (*metadata)[
                static_cast<qsizetype>(index)];
            const ClassNavigationReadRecord& details = record.details;
            if (details.classId <= 0 || !details.hasClassInfo)
            {
                continue;
            }
            const auto targetClassId = Domain::ClassId::fromString(
                std::to_string(details.classId));
            if (!targetClassId
                || !Application::isRosterTransferTargetEligible(
                    sourceClassId, sourceGrade, *targetClassId,
                    details.grade.toStdU16String()))
            {
                continue;
            }

            eligibleTargets.push_back({.record = &record, .classId = *targetClassId});
            if (details.teacherId > 0 && !seenTeacherIds.contains(details.teacherId))
            {
                seenTeacherIds.insert(details.teacherId);
                requestedTeacherIds.append(details.teacherId);
            }
        }

        QHash<qint64, MyClassesTeacherDisplayNameBatchReadRecord>
            teacherDisplayNames;
        if (!requestedTeacherIds.isEmpty())
        {
            const auto loadedTeachers =
                teacherRepository->loadMyClassesTeacherDisplayNameRecords(
                    requestedTeacherIds);
            if (!loadedTeachers)
            {
                return Application::RosterTransferTargetChoicesResult::failure(
                    repositoryError(loadedTeachers.error(),
                        "Transfer teacher details could not be loaded."));
            }
            for (const MyClassesTeacherDisplayNameBatchReadRecord& teacher :
                 *loadedTeachers)
            {
                teacherDisplayNames.insert(teacher.teacherId, teacher);
            }
        }

        for (const EligibleTarget& eligibleTarget : eligibleTargets)
        {
            const ClassNavigationBoundedReadRecord& record =
                *eligibleTarget.record;
            const ClassNavigationReadRecord& details = record.details;

            const auto targetRoster = rosterRepository->loadRosterWindowDetailed(
                details.classId,
                static_cast<int>(Application::RosterModeledRowCount));
            if (!targetRoster)
            {
                return Application::RosterTransferTargetChoicesResult::failure(
                    repositoryError(targetRoster.error(),
                        "A transfer target roster could not be checked."));
            }
            if (targetRoster->parentMissing)
            {
                continue;
            }
            const Application::RosterSnapshot roster =
                applicationSnapshot(targetRoster->roster);
            const bool full = Application::firstEmptyRosterRow(roster.rows)
                    .rawValue()
                >= Application::RosterModeledRowCount;

            Application::RosterTransferTargetDisplayMetadata displayMetadata{
                .grade = details.grade.toStdU16String(),
                .level = details.level.toStdU16String(),
                .teacherEnglishName = details.teacherEnglishName.toStdU16String(),
                .teacherKoreanName = details.teacherKoreanName.toStdU16String()
            };
            if (details.teacherId > 0)
            {
                const auto teacher = teacherDisplayNames.constFind(
                    details.teacherId);
                if (teacher != teacherDisplayNames.cend())
                {
                    const auto maximumTextLength = static_cast<qsizetype>(
                        Application::kClassesNavigationMaximumTextLength);
                    if (teacher->preferredName.size() <= maximumTextLength)
                    {
                        displayMetadata.teacherPreferredName =
                            teacher->preferredName.toStdU16String();
                    }
                    if (teacher->preferredRomanization.size() <= maximumTextLength)
                    {
                        displayMetadata.teacherPreferredRomanization =
                            teacher->preferredRomanization.toStdU16String();
                    }
                }
            }
            displayMetadata.regularSchedule.reserve(
                static_cast<std::size_t>(details.regularTimes.size()));
            for (const ClassTime& time : details.regularTimes)
            {
                displayMetadata.regularSchedule.push_back({
                    .day = time.day.toStdU16String(),
                    .startTime = time.startTime.toStdU16String()
                });
            }

            snapshot.candidates.push_back({
                .classId = eligibleTarget.classId,
                .className = record.className.trimmed().toStdU16String(),
                .displayMetadata = std::move(displayMetadata),
                .full = full
            });
        }

        return Application::RosterTransferTargetChoicesResult::success(
            std::move(snapshot));
    }
    catch (const std::exception&)
    {
        return Application::RosterTransferTargetChoicesResult::failure(
            genericError(Domain::ErrorCode::Technical,
                "Transfer classes could not be loaded."));
    }
    catch (...)
    {
        return Application::RosterTransferTargetChoicesResult::failure(
            genericError(Domain::ErrorCode::Technical,
                "Transfer classes could not be loaded."));
    }
}

Domain::Result<void> saveRosterTransferAtPath(
    const QString& databasePath,
    const Application::RosterRowTransferSaveRequest& request
    )
{
    const auto sourceId = legacyClassId(request.sourceClassId);
    const auto targetId = legacyClassId(request.targetClassId);
    if (!sourceId || !targetId || *sourceId == *targetId)
    {
        return Domain::Result<void>::failure(genericError(
            Domain::ErrorCode::InvalidInput,
            "Transfer requires distinct canonical positive class IDs."));
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return storeOpenFailure<Domain::Result<void>>(opened);
    }
    return saveTransferPair(store.rosterRepository(), request);
}

} // namespace ClassMngr::Next::Platform
