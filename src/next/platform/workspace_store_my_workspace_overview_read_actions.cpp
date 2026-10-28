#include "next/platform/workspace_store_my_workspace_overview_read_actions.h"

#include "data/database/workspace_store.h"
#include "next/application/my_workspace_upcoming_events_read.h"
#include "next/platform/workspace_store_my_workspace_schedule_read_port.h"
#include "next/platform/workspace_store_my_workspace_upcoming_events_read_port.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>

namespace ClassMngr::Next::Platform
{
namespace
{

template <typename Result>
Result openFailure(const RepositoryStatus& opened)
{
    const QByteArray diagnostic = opened.error().diagnostic.toUtf8();
    return Result::failure({
        .code = repositoryDomainErrorCode(opened.error().code),
        .message = diagnostic.isEmpty()
            ? "The active workspace could not be opened for its overview read."
            : diagnostic.toStdString(),
        .recoverable = true
    });
}

} // namespace

Application::MyWorkspaceScheduleReadResult readMyWorkspaceScheduleAtPath(
    const QString& databasePath
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openFailure<Application::MyWorkspaceScheduleReadResult>(opened);
    }
    const WorkspaceStoreMyWorkspaceScheduleReadPort port(store);
    return Application::MyWorkspaceScheduleReadUseCase::execute(port);
}

Application::MyWorkspaceUpcomingEventsReadResult
readMyWorkspaceUpcomingEventsAtPath(const QString& databasePath)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openFailure<Application::MyWorkspaceUpcomingEventsReadResult>(opened);
    }
    const WorkspaceStoreMyWorkspaceUpcomingEventsReadPort port(store);
    return Application::MyWorkspaceUpcomingEventsReadUseCase::execute(port);
}

} // namespace ClassMngr::Next::Platform
