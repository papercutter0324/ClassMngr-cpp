# Schedule page handoff

## Current WinUI schedule-import flow

The WinUI schedule-import interaction uses a resettable XLSX source dialog
followed by a separate `Review & Reconcile` dialog. The implementation surface
is `MainWindow_schedule_page.cpp`, `MainWindow_schedule_editor.cpp`,
`MainWindow_diagnostics_schedule.cpp`, `MainWindow.xaml.h`, and the
hand-authored WinUI project file where required; unrelated existing changes
were preserved.

The source flow picks XLSX files and validates Regular/Intensives through the
native workbook reader. One visible sheet proceeds to teacher selection;
multiple visible sheets require worksheet then teacher selection. The flow has
an explicit `Select a name...` state, cancellation/reset behavior, titlebar
dragging, and horizontal resizing.

The review flow uses a WinUI `Pivot` for Classes and Korean Teachers, with
conditional `Unrecognized` diagnostics and vertical scrolling. It reuses the
schedule-board renderer and color picker, provides per-item resolution
controls, gates Import until resolutions are complete, and confirms proposals
in six distinct readable rows.

## Verification and boundary

The x64 Debug WinUI build completed with zero errors; staged
`ClassMngrWindowsWinUI.exe --phase6-schedule-test` passed; focused schedule
reader/import tests passed 4/4; final presentation assertions passed 14/14;
and `git diff --check` passed. Existing dependency/OpenXLSX/NuGet warnings
were non-blocking.

No launchable native UI was available in the verification host. Live visual
spacing, picker behavior, drag/resize, dialog replacement, and focus
restoration remain unverified follow-up.
