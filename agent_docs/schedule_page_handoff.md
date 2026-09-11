# Schedule page handoff

## Status

The Schedule page issue is fixed. Database open, create, and close lifecycle
handlers now refresh the cached Schedule workspace, so the Regular schedule
and board are populated on the first view after startup or database changes.

The latest committed implementation is `a891d46a` (`Fix Sub Prep navigation
and shared tab styling`).

## Root-cause evidence

The Schedule workspace is hosted inside the cached My Workspace/Home page.
`populateHomePage()` can construct it before a database is available, and
`populateScheduleWorkspace()` then calls `refreshScheduleWorkspace()` while
the workspace is in its no-database state.

Before the fix, database lifecycle handlers refreshed Campus, My Details, Sub
Prep, Classes, and Calendar, but omitted the Schedule workspace:

- `MainWindow::openDatabasePath()` and `createDatabasePath()` in
  `src/platform/windows/winui/MainWindow_database_commands.cpp`.
- `MainWindow::CloseDatabaseMenuItem_Click()` in
  `src/platform/windows/winui/MainWindow_file_commands.cpp`.

That omission allowed the cached Schedule controls to retain an empty
`m_scheduleInfos` collection and a collapsed board after the database becomes
available. Returning to Home called `refreshScheduleWorkspace()`, which could
make the schedule appear again. The Regular/Intensive/Testing buttons call
`setScheduleDisplayMode()` and `refreshScheduleBoard()` only; they repaint the
current model but do not reload schedule data.

`MainWindow::refreshScheduleWorkspace()` in
`src/platform/windows/winui/MainWindow_schedule_editor.cpp` confirms the
state transition: it clears `m_scheduleInfos`, loads class and schedule data
only when `m_openDatabase` exists, and then calls `refreshScheduleBoard()`.

## Validation

The rebuilt schedule diagnostic passed:

```powershell
.\dist\ClassMngr-windows-winui-x64\Debug\ClassMngrWinUI.exe --phase6-schedule-test
# exit code 0
```

The diagnostic constructs Home before a database is available, checks that
Regular data and the board are ready after a workspace refresh without
switching display modes, and remains in-memory so it does not leave temporary
database files or alter recent-database state. It also continues to cover the
schedule engine, editor controls, import flow, testing flow, and board
rendering.

## Completed changes

1. Added `refreshScheduleWorkspace()` to the database open, database create,
   and database close refresh sequences.
2. Strengthened the phase-6 schedule diagnostic with a Regular-board
   readiness assertion.
3. Rebuilt the WinUI target and reran `--phase6-schedule-test`.

## Working-tree note

Only the lifecycle refresh fix, its diagnostic coverage, and this handoff
update were changed for this issue.
