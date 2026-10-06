#include "teacher_import_repository.h"

#include "next/application/teacher_import_use_case.h"
#include "next/application/teacher_import_plan_mapper.h"
#include "teacher_import_sql_persistence_adapter.h"

#include <QObject>
#include <QDebug>
#include <QSqlDatabase>

#include <cstdint>
#include <string>
#include <utility>

namespace
{
using namespace ClassMngr::Next::Application;

QString validationMessage(
    const TeacherImportPlanValidationIssue issue
    )
{
    switch (issue)
    {
    case TeacherImportPlanValidationIssue::ReviewSelectionMismatch:
        return QObject::tr(
            "The reviewed Korean teachers do not match the import selection.");
    case TeacherImportPlanValidationIssue::InvalidSourceDate:
        return QObject::tr("The teacher import date is invalid.");
    case TeacherImportPlanValidationIssue::MissingKoreanTeacherName:
        return QObject::tr("Every imported Korean teacher must have a name.");
    case TeacherImportPlanValidationIssue::DuplicateKoreanTeacherName:
        return QObject::tr("The import contains a duplicate Korean teacher name.");
    case TeacherImportPlanValidationIssue::MissingNativeEnglishTeacherName:
        return QObject::tr("Every imported Native English Teacher must have a name.");
    case TeacherImportPlanValidationIssue::DuplicateNativeEnglishTeacherName:
        return QObject::tr(
            "The import contains a duplicate Native English Teacher name.");
    case TeacherImportPlanValidationIssue::MissingGsTeamMemberName:
        return QObject::tr("Every imported GS Team member must have a name.");
    case TeacherImportPlanValidationIssue::DuplicateGsTeamMemberName:
        return QObject::tr("The import contains a duplicate GS Team name.");
    case TeacherImportPlanValidationIssue::None:
        break;
    }
    return {};
}

QString persistenceMessage(const TeacherImportUseCaseError& error)
{
    if (!error.persistenceFailure.message.empty())
    {
        return QString::fromStdU16String(error.persistenceFailure.message);
    }
    switch (error.persistenceFailure.operation)
    {
    case TeacherImportPersistenceOperation::BeginTransaction:
        return QObject::tr("Unable to start the teacher import transaction.");
    case TeacherImportPersistenceOperation::CommitTransaction:
        return QObject::tr("Unable to commit the teacher import transaction.");
    case TeacherImportPersistenceOperation::RollbackTransaction:
        return QObject::tr("Unable to roll back the teacher import transaction.");
    case TeacherImportPersistenceOperation::LoadSnapshot:
    case TeacherImportPersistenceOperation::CreateKoreanTeacher:
    case TeacherImportPersistenceOperation::UpdateKoreanTeacher:
    case TeacherImportPersistenceOperation::CreateNativeEnglishTeacher:
    case TeacherImportPersistenceOperation::UpdateNativeEnglishTeacher:
    case TeacherImportPersistenceOperation::CreateGsTeamMember:
    case TeacherImportPersistenceOperation::UpdateGsTeamMember:
    case TeacherImportPersistenceOperation::SaveLatestSourceDate:
        return QObject::tr("The teacher import could not be completed.");
    }
    return QObject::tr("The teacher import could not be completed.");
}

QString useCaseErrorMessage(const TeacherImportUseCaseError& error)
{
    switch (error.kind)
    {
    case TeacherImportUseCaseErrorKind::InvalidRequest:
    case TeacherImportUseCaseErrorKind::InvalidReview:
        return QObject::tr("The teacher import review choices are invalid.");
    case TeacherImportUseCaseErrorKind::InvalidPlan:
        return validationMessage(error.validationIssue);
    case TeacherImportUseCaseErrorKind::AmbiguousStoredMatch:
        switch (error.recordNamespace)
        {
        case TeacherImportRecordNamespace::KoreanTeacher:
            return QObject::tr("More than one stored Korean teacher matches %1.")
                .arg(QString::fromStdU16String(error.matchLabel));
        case TeacherImportRecordNamespace::NativeEnglishTeacher:
            return QObject::tr(
                "More than one stored Native English Teacher matches %1.")
                .arg(QString::fromStdU16String(error.matchLabel));
        case TeacherImportRecordNamespace::GsTeamMember:
            return QObject::tr("More than one stored GS Team member matches %1.")
                .arg(QString::fromStdU16String(error.matchLabel));
        }
        break;
    case TeacherImportUseCaseErrorKind::KoreanMatchPolicyRejected:
        return QObject::tr(
            "The matched Korean teacher no longer matches the import key.");
    case TeacherImportUseCaseErrorKind::PersistenceFailure:
        return persistenceMessage(error);
    }
    return QObject::tr("The teacher import could not be completed.");
}
}

TeacherImportRepository::TeacherImportRepository(QSqlDatabase& database)
    : m_database(database)
{
}

Result<TeacherImportSummary> TeacherImportRepository::importTeachers(
    const TeacherImportPlan& plan
    )
{
    const TeacherImportUseCaseRequest request =
        ClassMngr::Next::Application::teacherImportPlanToUseCaseRequest(plan);
    const auto persistence = makeTeacherImportSqlPersistenceAdapter(m_database);
    const auto applied = TeacherImportUseCase::execute(request, *persistence);
    if (!applied)
    {
        logRollbackFailure(applied.error());
        return std::unexpected(legacyErrorMessage(applied.error()));
    }

    return legacySummary(*applied);
}

QString TeacherImportRepository::legacyErrorMessage(
    const ClassMngr::Next::Application::TeacherImportUseCaseError& error
    )
{
    return useCaseErrorMessage(error);
}

TeacherImportSummary TeacherImportRepository::legacySummary(
    const ClassMngr::Next::Application::TeacherImportUseCaseResult& result
    )
{
    return {
        .koreanTeachers = {
            .created = result.koreanTeachers.created,
            .updated = result.koreanTeachers.updated,
            .unchanged = result.koreanTeachers.unchanged
        },
        .nativeEnglishTeachers = {
            .created = result.nativeEnglishTeachers.created,
            .updated = result.nativeEnglishTeachers.updated,
            .unchanged = result.nativeEnglishTeachers.unchanged
        },
        .gsTeamMembers = {
            .created = result.gsTeamMembers.created,
            .updated = result.gsTeamMembers.updated,
            .unchanged = result.gsTeamMembers.unchanged
        }
    };
}

void TeacherImportRepository::logRollbackFailure(
    const ClassMngr::Next::Application::TeacherImportUseCaseError& error
    )
{
    if (!error.rollbackFailure)
    {
        return;
    }
    QString rollbackMessage = QString::fromStdU16String(
        error.rollbackFailure->message);
    if (rollbackMessage.isEmpty())
    {
        rollbackMessage = QObject::tr(
            "Unable to roll back the teacher import transaction.");
    }
    qWarning().noquote()
        << QObject::tr("Teacher import rollback also failed: %1")
               .arg(rollbackMessage);
}
