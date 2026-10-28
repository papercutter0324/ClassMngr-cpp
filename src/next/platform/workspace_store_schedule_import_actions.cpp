#include "next/platform/workspace_store_schedule_import_actions.h"

#include "data/database/workspace_store.h"
#include "next/application/schedule_import_apply_use_case.h"
#include "next/platform/workspace_store_schedule_import_apply_port.h"
#include "next/platform/workspace_store_schedule_import_state_snapshot_port.h"

#include <QByteArray>

namespace ClassMngr::Next::Platform
{
namespace
{

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

std::u16string utf16(const QString& value)
{
    return value.toStdU16String();
}

Application::ScheduleImportStateSnapshotOutcome stateOpenFailure(
    const QString& message
    )
{
    return Application::ScheduleImportStateSnapshotFailure{
        Application::ScheduleImportStateSnapshotFailureKind::
            ActiveSessionUnavailable,
        Application::ScheduleImportStateSnapshotFailureSource::Session,
        utf8(message)
    };
}

Application::ScheduleImportApplyResult applyOpenFailure(
    const QString& message
    )
{
    Application::ScheduleImportApplyFailure failure;
    failure.message = utf16(message);
    failure.code = Domain::ErrorCode::NotFound;
    return std::unexpected(std::move(failure));
}

} // namespace

Application::ScheduleImportStateSnapshotOutcome
readScheduleImportStateAtPath(const QString& databasePath)
{
    if (databasePath.trimmed().isEmpty())
    {
        return stateOpenFailure(QStringLiteral("No Teacher Profile is open."));
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return stateOpenFailure(opened.error().diagnostic);
    }

    const WorkspaceStoreScheduleImportStateSnapshotPort port(store);
    return Application::ScheduleImportStateSnapshotQueryHandler::execute(
        {}, port);
}

Application::ScheduleImportApplyResult applyScheduleImportAtPath(
    const QString& databasePath,
    const Application::ScheduleImportApplyRequest& request
    )
{
    if (databasePath.trimmed().isEmpty())
    {
        return applyOpenFailure(QStringLiteral("No Teacher Profile is open."));
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return applyOpenFailure(opened.error().diagnostic);
    }

    const WorkspaceStoreScheduleImportApplyPort port(store);
    return Application::ScheduleImportApplyUseCase::execute(request, port);
}

} // namespace ClassMngr::Next::Platform
