#pragma once

#include "data/repositories/class_info_repository.h"
#include "data/repositories/repository_error.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_transfer.h"
#include "next/application/class_transfer_apply_request.h"
#include "next/application/class_transfer_preview_query.h"

#include <QList>
#include <QHash>
#include <QSqlDatabase>

#include <expected>
#include <optional>
#include <utility>

// Owning staged export reads. Errors remain attached to their original read
// position; the Application boundary replays them without further database reads.
struct ClassTransferExportSourceClass
{
    int classId = -1;
    int selectedIndex = -1;
    QString name;
    ClassTransferExportClassInfoReadRecord info;
    std::optional<RepositoryError> infoError;
    Roster roster;
    std::optional<RepositoryError> rosterError;
    QList<ClassTransferEvaluation> evaluations;
    std::optional<RepositoryError> evaluationError;
    qint64 teacherId = -1;
    bool evaluationAttempted = false;
};

struct ClassTransferExportSource
{
    QDateTime exportedAtUtc;
    QList<ClassTransferExportSourceClass> classes;
    QList<qint64> teacherIds;
    QHash<qint64, TeacherProfileReadRecord> teachersById;
    QHash<qint64, RepositoryError> teacherErrorsById;
    std::optional<RepositoryError> teacherBatchError;
    std::optional<RepositoryError> selectionOrReadError;
};

struct ClassTransferImportError
{
    enum class Kind
    {
        RepositoryFailure,
        ScheduleConflict
    };

    Kind kind = Kind::RepositoryFailure;
    QString message;
    RepositoryError repositoryError;

    // Keep the established diagnostic text available to legacy callers.
    ClassTransferImportError(QString failureMessage)
        : ClassTransferImportError(
              Kind::RepositoryFailure,
              RepositoryError{std::move(failureMessage)}
              )
    {
    }

    ClassTransferImportError(RepositoryError failure)
        : ClassTransferImportError(
              Kind::RepositoryFailure,
              std::move(failure)
              )
    {
    }

    ClassTransferImportError(Kind failureKind, QString failureMessage)
        : ClassTransferImportError(
              failureKind,
              RepositoryError{std::move(failureMessage)}
              )
    {
    }

    ClassTransferImportError(
        Kind failureKind,
        RepositoryError failure
        )
        : kind(failureKind),
          message(failure.diagnostic),
          repositoryError(std::move(failure))
    {
    }
};

using ClassTransferDetailedImportResult =
    std::expected<ClassImportSummary, ClassTransferImportError>;

class ClassTransferRepository final
    : public ClassMngr::Next::Application::ClassTransferPreviewReadPort
{
public:
    explicit ClassTransferRepository(
        QSqlDatabase& database
        );

    // Commits only a successful read snapshot; staged failures roll back and
    // remain available to callers for the existing per-class error replay.
    [[nodiscard]] RepositoryResult<ClassTransferExportSource> readExportSource(
        const QList<int>& classIds
        );

    [[nodiscard]] RepositoryResult<ClassTransferPackage> buildPackage(
        const QList<int>& classIds
        );

    [[nodiscard]] RepositoryResult<ClassImportPreview> previewImport(
        const ClassTransferPackage& package
        );

    // SQL reads and legacy text normalization stay at the repository edge.
    [[nodiscard]] ClassMngr::Next::Application::ClassTransferPreviewReadResult
    readSnapshot(
        const ClassMngr::Next::Application::ClassTransferPreviewRequest& request
        ) const override;

    [[nodiscard]] RepositoryResult<ClassImportSummary> importClasses(
        const ClassTransferPackage& package,
        const ClassImportPlan& plan
        );

    [[nodiscard]] RepositoryResult<ClassImportSummary> importClasses(
        const ClassTransferPackage& package,
        const ClassMngr::Next::Application::ClassTransferApplyRequest& request
        );

    // Retains the failure origin while importClasses exposes the same
    // diagnostic through its structured repository error.
    [[nodiscard]] ClassTransferDetailedImportResult
    importClassesWithFailureDetails(
        const ClassTransferPackage& package,
        const ClassMngr::Next::Application::ClassTransferApplyRequest& request
        );

private:
    QSqlDatabase& m_database;
};
