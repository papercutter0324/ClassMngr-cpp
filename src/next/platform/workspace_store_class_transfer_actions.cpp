#include "next/platform/workspace_store_class_transfer_actions.h"

#include "data/database/workspace_store.h"
#include "data/repositories/class_transfer_legacy_package_adapter.h"
#include "data/repositories/class_transfer_repository.h"
#include "domain/validation/class_transfer_package_validator.h"
#include "next/application/class_transfer_apply_use_case.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/platform/repository_error_code.h"
#include "next/platform/workspace_store_classes_navigator_read_actions.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{
namespace
{

[[nodiscard]] std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

[[nodiscard]] Domain::OperationError operationError(
    const Domain::ErrorCode code,
    std::string message,
    const bool recoverable = true
    )
{
    return {.code = code, .message = std::move(message), .recoverable = recoverable};
}

[[nodiscard]] Domain::OperationError repositoryError(
    const RepositoryError& source
    )
{
    const std::string message = utf8(source.diagnostic);
    return operationError(
        repositoryDomainErrorCode(source.code),
        message.empty() ? "Class transfer data is unavailable." : message,
        source.recoverableOverride.value_or(
            source.code != RepositoryErrorCode::Technical));
}

[[nodiscard]] Application::ClassTransferExportSourceResult openFailure(
    const RepositoryError& source
    )
{
    return Application::ClassTransferExportSourceResult::failure(
        repositoryError(source));
}

[[nodiscard]] Application::ClassTransferPreviewQueryResult openPreviewFailure(
    const RepositoryError& source
    )
{
    return Application::ClassTransferPreviewQueryResult::failure(
        repositoryError(source));
}

[[nodiscard]] Application::ClassTransferApplyResult openApplyFailure(
    const RepositoryError& source
    )
{
    return Application::ClassTransferApplyResult::failure(
        repositoryError(source));
}

[[nodiscard]] Application::ClassTransferExportSourceResult exportSourceFromStore(
    ClassTransferRepository& repository,
    const Application::ClassTransferExportRequest& request
    )
{
    QList<int> legacyIds;
    legacyIds.reserve(static_cast<qsizetype>(request.classIds.size()));
    for (const Domain::ClassId& classId : request.classIds)
    {
        int value = 0;
        const std::string& text = classId.value();
        const auto [end, error] = std::from_chars(
            text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size()
            || value <= 0 || std::to_string(value) != text)
        {
            value = 0; // Preserve repository-owned deferred selection errors.
        }
        legacyIds.append(value);
    }

    const auto loaded = repository.readExportSource(legacyIds);
    if (!loaded)
    {
        return Application::ClassTransferExportSourceResult::failure(
            repositoryError(loaded.error()));
    }

    Application::ClassTransferExportSource output;
    output.exportedAtUtcMilliseconds =
        Application::ClassTransferExportUtcMilliseconds{
            loaded->exportedAtUtc.toMSecsSinceEpoch()};
    if (loaded->selectionOrReadError)
    {
        output.deferredSelectionOrLookupError =
            repositoryError(*loaded->selectionOrReadError);
    }
    if (loaded->teacherBatchError)
    {
        output.teacherBatchError = repositoryError(*loaded->teacherBatchError);
    }

    for (const qint64 teacherId : loaded->teacherIds)
    {
        const auto typedTeacherId = Domain::TeacherId::fromString(
            std::to_string(teacherId));
        if (!typedTeacherId)
        {
            return Application::ClassTransferExportSourceResult::failure(
                operationError(Domain::ErrorCode::Validation,
                    "A class transfer teacher ID is invalid.", false));
        }

        Application::ClassExportSourceTeacher teacher{*typedTeacherId, {}};
        const auto failedProfile = loaded->teacherErrorsById.constFind(teacherId);
        if (failedProfile != loaded->teacherErrorsById.cend())
        {
            teacher.profile = Application::ClassExportStage<
                Application::ClassExportTeacherProfile>::error(
                    repositoryError(*failedProfile));
        }
        else if (loaded->teachersById.contains(teacherId))
        {
            const TeacherProfileReadRecord& source = loaded->teachersById[teacherId];
            Application::ClassExportTeacherProfile profile;
            profile.teacherKr = source.teacherKr.toStdU16String();
            profile.teacherEn = source.teacherEn.toStdU16String();
            profile.preferredRomanization = source.preferredRomanization.toStdU16String();
            profile.preferredName = source.preferredName.toStdU16String();
            profile.roomNumber = source.roomNumber.toStdU16String();
            profile.birthday = source.birthday.toStdU16String();
            profile.phoneNumber = source.phoneNumber.toStdU16String();
            profile.wifiName = source.wifiName.toStdU16String();
            profile.wifiPassword = source.wifiPassword.toStdU16String();
            profile.internetType = source.internetType.toStdU16String();
            profile.zoomId = source.zoomId.toStdU16String();
            profile.zoomPassword = source.zoomPassword.toStdU16String();
            profile.projectionType = source.projectionType.toStdU16String();
            profile.notes = source.notes.toStdU16String();
            teacher.profile = Application::ClassExportStage<
                Application::ClassExportTeacherProfile>::value(std::move(profile));
        }
        output.teachers.push_back(std::move(teacher));
    }

    for (const ClassTransferExportSourceClass& source : loaded->classes)
    {
        const auto typedClassId = Domain::ClassId::fromString(
            std::to_string(source.classId));
        if (!typedClassId)
        {
            return Application::ClassTransferExportSourceResult::failure(
                operationError(Domain::ErrorCode::Validation,
                    "A class transfer class ID is invalid.", false));
        }

        Application::ClassExportSourceClass classroom{
            *typedClassId,
            Application::ClassTransferExportSelectionOrdinal{
                static_cast<std::size_t>(source.selectedIndex)},
            source.name.toStdU16String(), {}, {}, {}, {}};
        if (source.infoError)
        {
            classroom.info = Application::ClassExportStage<
                Application::ClassExportSourceInfo>::error(
                    repositoryError(*source.infoError));
        }
        else
        {
            Application::ClassExportSourceInfo info;
            info.sourceClassId = source.info.classId;
            info.sourceTeacherId = source.info.teacherId;
            info.classGrade = source.info.classGrade.toStdU16String();
            info.classLevel = source.info.classLevel.toStdU16String();
            info.readingBook = source.info.readingBook.toStdU16String();
            info.essayBook = source.info.essayBook.toStdU16String();
            info.classColor = source.info.classColor.toStdU16String();
            info.fontColor = source.info.fontColor.toStdU16String();
            info.notes = source.info.notes.toStdU16String();
            info.timeFillerActivities = source.info.timeFillerActivities.toStdU16String();
            info.teacherKr = source.info.teacherKr.toStdU16String();
            info.teacherEn = source.info.teacherEn.toStdU16String();
            info.teacherPreferredName = source.info.teacherPreferredName.toStdU16String();
            info.roomNumber = source.info.roomNumber.toStdU16String();
            info.wifiName = source.info.wifiName.toStdU16String();
            info.wifiPassword = source.info.wifiPassword.toStdU16String();
            info.internetType = source.info.internetType.toStdU16String();
            info.zoomId = source.info.zoomId.toStdU16String();
            info.zoomPassword = source.info.zoomPassword.toStdU16String();
            info.projectionType = source.info.projectionType.toStdU16String();
            for (const ClassTime& row : source.info.classTimes)
            {
                info.classTimes.push_back({row.day.toStdU16String(),
                    row.startTime.toStdU16String(), row.endTime.toStdU16String()});
            }
            for (const ClassTime& row : source.info.intensiveTimes)
            {
                info.intensiveTimes.push_back({row.day.toStdU16String(),
                    row.startTime.toStdU16String(), row.endTime.toStdU16String()});
            }
            classroom.info = Application::ClassExportStage<
                Application::ClassExportSourceInfo>::value(std::move(info));
        }

        if (source.rosterError)
        {
            classroom.roster = Application::ClassExportStage<
                Application::ClassExportRoster>::error(
                    repositoryError(*source.rosterError));
        }
        else
        {
            Application::ClassExportRoster roster;
            for (const QString& column : source.roster.columns)
            {
                roster.columns.push_back(column.toStdU16String());
            }
            for (const int width : source.roster.columnWidths)
            {
                roster.columnWidths.push_back(
                    Application::ClassExportRosterColumnWidth{width});
            }
            for (const QStringList& row : source.roster.rows)
            {
                std::vector<Application::ClassExportText> cells;
                cells.reserve(static_cast<std::size_t>(row.size()));
                for (const QString& cell : row)
                {
                    cells.push_back(cell.toStdU16String());
                }
                roster.rows.push_back(std::move(cells));
            }
            classroom.roster = Application::ClassExportStage<
                Application::ClassExportRoster>::value(std::move(roster));
        }

        if (source.evaluationAttempted)
        {
            classroom.teacher = Application::ClassExportStage<
                std::optional<Domain::TeacherId>>::value(source.teacherId > 0
                    ? Domain::TeacherId::fromString(std::to_string(source.teacherId))
                    : std::nullopt);
            if (source.evaluationError)
            {
                classroom.evaluations = Application::ClassExportStage<
                    std::vector<Application::ClassExportEvaluation>>::error(
                        repositoryError(*source.evaluationError));
            }
            else
            {
                std::vector<Application::ClassExportEvaluation> evaluations;
                evaluations.reserve(static_cast<std::size_t>(source.evaluations.size()));
                for (const ClassTransferEvaluation& evaluation : source.evaluations)
                {
                    Application::ClassExportEvaluation mapped;
                    mapped.name = evaluation.name.toStdU16String();
                    for (const QStringList& row : evaluation.rows)
                    {
                        std::vector<Application::ClassExportText> cells;
                        cells.reserve(static_cast<std::size_t>(row.size()));
                        for (const QString& cell : row)
                        {
                            cells.push_back(cell.toStdU16String());
                        }
                        mapped.rows.push_back(std::move(cells));
                    }
                    evaluations.push_back(std::move(mapped));
                }
                classroom.evaluations = Application::ClassExportStage<
                    std::vector<Application::ClassExportEvaluation>>::value(
                        std::move(evaluations));
            }
        }
        output.classes.push_back(std::move(classroom));
    }

    return Application::ClassTransferExportSourceResult::success(std::move(output));
}

class WorkspaceStoreClassTransferApplyPort final
    : public Application::ClassTransferApplyPort
{
public:
    explicit WorkspaceStoreClassTransferApplyPort(WorkspaceStore& store) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::ClassTransferApplyResult apply(
        Application::ClassTransferApplyCommand command
        ) const override
    {
        try
        {
            ClassTransferRepository* const repository =
                m_store ? m_store->classTransferRepository() : nullptr;
            if (!m_store || !m_store->isOpen() || !repository)
            {
                return Application::ClassTransferApplyResult::failure(
                    operationError(Domain::ErrorCode::NotFound,
                        "The destination workspace is unavailable."));
            }

            const auto normalized = ClassTransferPackageValidator::normalizedAndValidated(
                Data::Legacy::toLegacyClassTransferPackage(
                    std::move(command.package)));
            if (!normalized)
            {
                return Application::ClassTransferApplyResult::failure(
                    operationError(Domain::ErrorCode::Validation,
                        utf8(normalized.error())));
            }

            const auto imported = repository->importClassesWithFailureDetails(
                *normalized, command.choices);
            if (!imported)
            {
                return Application::ClassTransferApplyResult::failure(
                    repositoryError(imported.error().repositoryError));
            }

            Application::ClassTransferApplySummary summary;
            summary.skippedClassCount = Application::ClassTransferSkippedClassCount{
                imported->skippedClassCount};
            for (const int id : imported->createdClassIds)
            {
                const auto typed = Domain::ClassId::fromString(std::to_string(id));
                if (id <= 0 || !typed)
                {
                    return Application::ClassTransferApplyResult::failure(
                        operationError(Domain::ErrorCode::Technical,
                            "The import repository returned an invalid class ID.", false));
                }
                summary.createdClassIds.push_back(*typed);
            }
            for (const int id : imported->replacedClassIds)
            {
                const auto typed = Domain::ClassId::fromString(std::to_string(id));
                if (id <= 0 || !typed)
                {
                    return Application::ClassTransferApplyResult::failure(
                        operationError(Domain::ErrorCode::Technical,
                            "The import repository returned an invalid class ID.", false));
                }
                summary.replacedClassIds.push_back(*typed);
            }
            return Application::ClassTransferApplyResult::success(std::move(summary));
        }
        catch (const std::exception&)
        {
            return Application::ClassTransferApplyResult::failure(
                operationError(Domain::ErrorCode::Technical,
                    "Classes could not be imported."));
        }
        catch (...)
        {
            return Application::ClassTransferApplyResult::failure(
                operationError(Domain::ErrorCode::Technical,
                    "Classes could not be imported."));
        }
    }

private:
    WorkspaceStore* m_store = nullptr;
};

} // namespace

Application::ClassesListReadResult readClassTransferClassesAtPath(
    const QString& databasePath
    )
{
    return readClassesNavigatorListAtPath(databasePath);
}

Application::ClassTransferExportResult exportClassTransferAtPath(
    const QString& databasePath,
    const Application::ClassTransferExportRequest& request
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Application::ClassTransferExportResult::failure(
            repositoryError(opened.error()));
    }
    ClassTransferRepository* const repository = store.classTransferRepository();
    if (!repository)
    {
        return Application::ClassTransferExportResult::failure(
            operationError(Domain::ErrorCode::Technical,
                "The class transfer repository is unavailable."));
    }

    auto source = exportSourceFromStore(*repository, request);
    if (!source)
    {
        return Application::ClassTransferExportResult::failure(source.error());
    }
    class LocalExportSourcePort final : public Application::ClassTransferExportSourceReadPort
    {
    public:
        explicit LocalExportSourcePort(Application::ClassTransferExportSource value)
            : m_value(std::move(value))
        {
        }
        Application::ClassTransferExportSourceResult readSource(
            const Application::ClassTransferExportRequest&) override
        {
            return Application::ClassTransferExportSourceResult::success(
                std::move(m_value));
        }
    private:
        Application::ClassTransferExportSource m_value;
    } sourcePort(std::move(source.value()));
    return Application::ClassTransferExportQuery(sourcePort).execute(request);
}

Application::ClassTransferPreviewQueryResult previewClassTransferAtPath(
    const QString& databasePath,
    const Application::ClassTransferPreviewRequest& request
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openPreviewFailure(opened.error());
    }
    ClassTransferRepository* const repository = store.classTransferRepository();
    if (!repository)
    {
        return Application::ClassTransferPreviewQueryResult::failure(
            operationError(Domain::ErrorCode::Technical,
                "Class transfer preview candidates could not be loaded."));
    }
    try
    {
        return Application::ClassTransferPreviewQuery(*repository).execute(request);
    }
    catch (const std::exception&)
    {
        return Application::ClassTransferPreviewQueryResult::failure(
            operationError(Domain::ErrorCode::Technical,
                "Class transfer preview candidates could not be loaded."));
    }
    catch (...)
    {
        return Application::ClassTransferPreviewQueryResult::failure(
            operationError(Domain::ErrorCode::Technical,
                "Class transfer preview candidates could not be loaded."));
    }
}

Application::ClassTransferApplyResult applyClassTransferAtPath(
    const QString& databasePath,
    Application::ClassTransferApplyCommand command
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openApplyFailure(opened.error());
    }
    const WorkspaceStoreClassTransferApplyPort port(store);
    return Application::ClassTransferApplyUseCase::execute(
        std::move(command), port);
}

} // namespace ClassMngr::Next::Platform
