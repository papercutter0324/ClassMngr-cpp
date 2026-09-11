# Schedule page handoff

## Status

The Schedule page issue is diagnosed but not fixed. The symptom is that the
Regular schedule can be empty on its first view after startup or database
open, while switching display modes and returning to Regular causes the board
to repaint.

The latest committed implementation is `a891d46a` (`Fix Sub Prep navigation
and shared tab styling`).

## Root-cause evidence

The Schedule workspace is hosted inside the cached My Workspace/Home page.
`populateHomePage()` can construct it before a database is available, and
`populateScheduleWorkspace()` then calls `refreshScheduleWorkspace()` while
the workspace is in its no-database state.

Database lifecycle handlers refresh Campus, My Details, Sub Prep, Classes, and
Calendar, but omit the Schedule workspace:

- `MainWindow::openDatabasePath()` and `createDatabasePath()` in
  `src/platform/windows/winui/MainWindow_database_commands.cpp`.
- `MainWindow::CloseDatabaseMenuItem_Click()` in
  `src/platform/windows/winui/MainWindow_file_commands.cpp`.

As a result, the cached Schedule controls can retain an empty
`m_scheduleInfos` collection and a collapsed board after the database becomes
available. Returning to Home calls `refreshScheduleWorkspace()`, which can
make the schedule appear again. The Regular/Intensive/Testing buttons call
`setScheduleDisplayMode()` and `refreshScheduleBoard()` only; they repaint the
current model but do not reload schedule data.

`MainWindow::refreshScheduleWorkspace()` in
`src/platform/windows/winui/MainWindow_schedule_editor.cpp` confirms the
state transition: it clears `m_scheduleInfos`, loads class and schedule data
only when `m_openDatabase` exists, and then calls `refreshScheduleBoard()`.

## Validation

The existing schedule diagnostic passed:

```powershell
.\dist\ClassMngr-windows-winui-x64\Debug\ClassMngrWinUI.exe --phase6-schedule-test
# exit code 0
```

This confirms the schedule engine, editor controls, import flow, testing flow,
and board rendering work in isolation. The missing database-lifecycle refresh
is not covered by that diagnostic.

## Recommended next steps

1. Call `refreshScheduleWorkspace()` in the database open, database create,
   and database close refresh sequences.
2. Extend the schedule diagnostic to construct Home before attaching an
   in-memory database, then verify that Regular data and the board are ready
   without changing display modes.
3. Rebuild the WinUI target and rerun `--phase6-schedule-test` plus the new
   lifecycle assertion.

## Working-tree note

The unrelated modification in
`src/platform/windows/winui/MainWindow.xaml` was present before this handoff
and was not changed or included in the handoff commit.
