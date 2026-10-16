#pragma once

#include "core/result.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_transfer.h"
#include "next/application/class_transfer_apply_request.h"
#include "next/application/class_transfer_preview_query.h"

#include <QList>
#include <QHash>
#include <QSqlDatabase>

#include <expected>
#include <utility>

// Owning staged export reads. Errors remain attached to their original read
// position; the Application boundary replays them without further database reads.
struct ClassTransferExportSourceClass
{
    int classId = -1;
    int selectedIndex = -1;
    QString name;
    ClassTransferExportClassInfoReadRecord info;
    QString infoError;
    Roster roster;
    QString rosterError;
    QList<ClassTransferEvaluation> evaluations;
    QString evaluationError;
    qint64 teacherId = -1;
    bool evaluationAttempted = false;
};

struct ClassTransferExportSource
{
    QDateTime exportedAtUtc;
    QList<ClassTransferExportSourceClass> classes;
    QList<qint64> teacherIds;
    QHash<qint64, TeacherProfileReadRecord> teachersById;
    QHash<qint64, QString> teacherErrorsById;
    QString teacherBatchError;
    QString selectionOrReadError;
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

    // Ordinary repository failures keep their existing string-only behavior.
    ClassTransferImportError(QString failureMessage)
        : message(std::move(failureMessage))
    {
    }

    ClassTransferImportError(Kind failureKind, QString failureMessage)
        : kind(failureKind),
          message(std::move(failureMessage))
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
    [[nodiscard]] Result<ClassTransferExportSource> readExportSource(
        const QList<int>& classIds
        );

    [[nodiscard]] Result<ClassTransferPackage> buildPackage(
        const QList<int>& classIds
        );

    [[nodiscard]] Result<ClassImportPreview> previewImport(
        const ClassTransferPackage& package
        );

    // SQL reads and legacy text normalization stay at the repository edge.
    [[nodiscard]] ClassMngr::Next::Application::ClassTransferPreviewReadResult
    readSnapshot(
        const ClassMngr::Next::Application::ClassTransferPreviewRequest& request
        ) const override;

    [[nodiscard]] Result<ClassImportSummary> importClasses(
        const ClassTransferPackage& package,
        const ClassImportPlan& plan
        );

    [[nodiscard]] Result<ClassImportSummary> importClasses(
        const ClassTransferPackage& package,
        const ClassMngr::Next::Application::ClassTransferApplyRequest& request
        );

    // Retains the failure origin needed by typed callers while the established
    // importClasses overloads continue to expose their QString errors.
    [[nodiscard]] ClassTransferDetailedImportResult
    importClassesWithFailureDetails(
        const ClassTransferPackage& package,
        const ClassMngr::Next::Application::ClassTransferApplyRequest& request
        );

private:
    QSqlDatabase& m_database;
};
