# LLM-to-LLM handoff: WinUI sidebar and My Information

## Request

Fix the Windows WinUI navigation so that:

- `My Workspace` has no sidebar sub-nodes; its navigation is through page tabs.
- `Classes` has no sidebar sub-nodes; its navigation is through page tabs.
- Clicking `My Workspace` and then the `My Information` tab displays the correct personal-information page.

The user then asked to stop work and hand this task to another computer. This document records the current uncommitted state; implementation and final validation are not complete.

## Repository and current state

- Repository: `C:\Git\ClassMngr-cpp`
- Branch/worktree changes are uncommitted.
- Current modified files:
  - `src/platform/windows/winui/App.xaml.cpp`
  - `src/platform/windows/winui/MainWindow.xaml`
  - `src/platform/windows/winui/MainWindow.xaml.cpp`
  - `src/platform/windows/winui/MainWindow.xaml.h`
- No commit was created.
- Do not discard the existing changes with `git reset --hard` or checkout commands.

## Changes already made

### `MainWindow.xaml`

The following child `NavigationViewItem`s were removed:

- Workspace Information
- Workspace Schedule
- Workspace Calendar
- Class Details
- Class Roster
- Class Speaking Evaluations
- Class Analytics
- Class Notes

`My Workspace` and `Classes` are now leaf sidebar items. The existing Home and Classes page tab surfaces are intended to remain the navigation mechanism.

### `MainWindow.xaml.h`

- Removed members for the deleted child navigation items.
- Added an overload of `populatePersonalDetailsPage` that can populate a `ContentControl` host and accept the Home-page context.

### `MainWindow.xaml.cpp`

The current implementation attempts to:

- Normalize the legacy `personal_details` route to `home`.
- Make clicking the Workspace root navigate to `home` and reset the Home pivot to the My Information tab.
- Make clicking the Classes root navigate to `classes`.
- Keep Schedule and Calendar as Home page tabs, and class workflows as Classes page tabs.
- Build the engine-backed personal-details form as the first Home tab (`My Information`), with Schedule and Calendar as the other tabs.
- Refresh personal details, schedule, testing, and calendar content when the cached Home page is reused.
- Use `try_as<ContentControl>()` in the compatibility wrapper so a cached Home `Page` does not cause an `E_NOINTERFACE` exception.
- Map legacy route checks in the phase-6 personal-details smoke test back to Home.

### `App.xaml.cpp`

The phase-test helper was changed so `scheduleTestExit` and `completeViewModelTest` retain the `Window` by value. `scheduleTestExit` currently calls `ExitProcess` directly instead of queueing a close on the window dispatcher. This was added while investigating an intermittent test-harness access violation after the Home page became a larger asynchronous page.

Review whether this lifecycle change is acceptable before finalizing. It may be needed for the command-line smoke tests, but it is broader than the user-facing navigation change.

## Important unfinished diagnostic code

`runPhase3SemanticChecks()` currently contains temporary diagnostics that must be removed or replaced before final acceptance:

- A `trace` lambda attempts to append to `phase3-semantic-debug.txt`.
- `focusReady = true;` is forced after the focus block.
- A temporary `failureMask` calls `ExitProcess(100 + failureMask)` before the async view-model check.
- A temporary `ExitProcess(200)` is used when `runPhase3ViewModelChecks()` returns false.

These are present in the working tree intentionally for the next agent’s diagnosis. Do not leave them in the final product or normal test path.

The failure-mask bits are:

| Bit | Check |
| ---: | --- |
| 1 | phase-3 navigation |
| 2 | phase-1 input |
| 4 | focus |
| 8 | localization/resources |
| 16 | dialogs |
| 32 | threading |
| 64 | phase-4 semantic checks |

An exit code of `100 + mask` identifies the synchronous failure mask. Exit code `200` identifies a failed phase-3 view-model check.

## Known validation status

Already observed before the latest diagnostic edit:

- WinUI x64 Debug CMake build completed successfully with zero warnings/errors several times.
- XAML parsing succeeded with PowerShell XML parsing.
- `git diff --check` passed; Git only reported line-ending conversion warnings.
- The obsolete navigation-item-name scan found no removed child item names.
- These isolated tests returned success at least once: phase-3 navigation, phase-1 input, phase-3 localization, phase-3 dialog, phase-3 threading, phase-4 semantic, phase-3 view-model, and phase-6 personal-details.
- Phase-6 personal-details returned success repeatedly after changing the cached-page cast to `try_as`.

Not complete:

- The combined `--phase3-semantic-test` was alternating between exit code `13` and `0` in repeated runs before the latest diagnostic patch. The `13` result was not explained yet. A temporary `focusReady = true` did not eliminate the alternating result, so the remaining suspect is likely another combined-stage interaction, possibly phase-4 state or the async view-model check.
- The binary was not rebuilt and the full verifier was not rerun after the latest diagnostic code was added.
- No final UI acceptance pass has been completed on the new Home tabs.

## Recommended continuation

1. Inspect `runPhase3SemanticChecks()` and rebuild so the temporary exit-code diagnostics are in the binary.
2. Run one combined semantic test and decode the result:

   ```powershell
   .\build\windows-x64-winui-debug\Debug\ClassMngrWinUI.exe --phase3-semantic-test
   $LASTEXITCODE
   ```

   The executable path may differ in the local build output; use the existing build layout if needed.
3. If the result is `100 + mask`, use the table above. If it is `200`, investigate `runPhase3ViewModelChecks()` after the phase-4 check. Isolated checks passing does not prove the combined sequence is stable.
4. Implement the smallest real fix for the alternating combined test. Do not keep the forced focus result or temporary file tracing. Consider whether the async view-model check should run before phase 4, whether state needs to be reset between checks, or whether an extra dispatcher turn is required—but preserve the existing phase-test contract.
5. Rebuild with:

   ```powershell
   cmake --build build\windows-x64-winui-debug --config Debug --target ClassMngrWindowsWinUI -- /m:2 /p:TrackFileAccess=false
   ```

   If MSBuild/FileTracker requires it, run the command with the elevated build permission available in this environment.
6. Run the combined semantic test repeatedly (10 runs is preferred), then run the full staged verifier:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\verify_windows_winui_stage.ps1 -StageDirectory dist\ClassMngr-windows-winui-x64\Debug -Platform x64
   ```

7. Perform final static checks:

   ```powershell
   $null = [xml](Get-Content -Raw 'src\platform\windows\winui\MainWindow.xaml')
   git diff --check
   rg -n "WorkspaceInformationNavigationItem|WorkspaceScheduleNavigationItem|WorkspaceCalendarNavigationItem|ClassDetailsNavigationItem|ClassRosterNavigationItem|ClassSpeakingEvaluationsNavigationItem|ClassAnalyticsNavigationItem|ClassNotesNavigationItem" src\platform\windows\winui
   git status --short
   git diff --stat
   ```

   The obsolete-item search should produce no matches. Also verify that no `phase3-semantic-debug.txt` was created in the repository or staging output.
8. Review the complete diff, especially the `App.xaml.cpp` lifecycle change, and only then report completion.

## Files to inspect first on the next computer

- `src/platform/windows/winui/MainWindow.xaml.cpp`, around `runPhase3SemanticChecks()` and the Home/personal-details population methods.
- `src/platform/windows/winui/MainWindow.xaml`, the `NavigationView` declaration and Home/Classes page tabs.
- `src/platform/windows/winui/App.xaml.cpp`, the phase-test activation and `scheduleTestExit` helper.
- `scripts/verify_windows_winui_stage.ps1`, for the authoritative smoke-test sequence.

## Acceptance criteria

- Sidebar contains no child nodes under My Workspace or Classes.
- Clicking My Workspace displays the Home page with My Information selected by default.
- Clicking My Information displays the engine-backed personal-details controls, not the old generic/incorrect page.
- Schedule and Calendar remain reachable through Home tabs.
- Class tabs remain reachable through the Classes page.
- Existing phase smoke tests pass consistently, including repeated combined phase-3 semantic runs.
- No temporary diagnostic tracing, forced pass conditions, or debug exit paths remain in production code.
