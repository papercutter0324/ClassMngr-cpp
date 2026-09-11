# My Information page handoff

## User issue

The WinUI 3 My Information page is blank after navigating away from My Workspace to Classes and then returning to My Workspace. At fresh startup, the personal-details content is present.

## Findings

- The problem is a cached WinUI `Frame`/`Pivot` lifecycle issue, not missing personal-details data.
- UI Automation reproduced this sequence against the staged Debug executable:
  1. Startup: `Personal details form` and `Personal details description` are present.
  2. Select `Classes navigation root`.
  3. Select `My Workspace navigation root`.
  4. The My Information tab is selected, but both personal-details elements are absent.
- The previous implementation placed the personal-details form inside the first `PivotItem`. Rebinding, dispatcher callbacks, a timer, and reparenting that nested content did not survive cached-page reattachment.
- A sibling-host layout was added so the `Pivot` supplies only the tab strip while the personal-information, schedule, and calendar panels live in a sibling `Grid` and are switched with `Visibility`.

## Current source state

File changed by this task: `src/platform/windows/winui/MainWindow_navigation.cpp`.

Current changes include:

- `selectHomeInformationTab()` resets the Home page Pivot to index 0 and shows the first sibling content panel.
- `NavigationView_SelectionChanged()` and `ContentFrame_Navigated()` call that reset for the Home route.
- `populateHomePage()` builds an external `My Workspace content` Grid with three sibling panels; PivotItems contain empty placeholder Grids.
- The Pivot `SelectionChanged` handler ignores transient invalid (`-1`) selections during cached-page reattachment.
- The latest unverified addition registers `tabs.Loaded(...)` to reset index 0 and restore the first sibling panel after the Pivot is loaded.

Do not touch the user’s pre-existing unstaged edit in `src/platform/windows/winui/MainWindow_classes_page_details.cpp`.

## Validation status

- Build before the latest `tabs.Loaded` addition: succeeded with `0 Warning(s), 0 Error(s)` and staged the executable at:
  `dist/ClassMngr-windows-winui-x64/Debug/ClassMngrWinUI.exe`.
- Focused UIA check against that build still failed after the sibling-host plus invalid-selection guard:
  `initial form=True description=True`; `after Classes return form=False description=False`.
- The rebuild after adding `tabs.Loaded` was stopped at the user’s one-minute cutoff. Its final MSBuild error was caused by interruption; the new source has not been compiled or validated.

## Continue here

1. Build only `ClassMngrWindowsWinUI` in Debug and inspect the actual compiler result:

   ```powershell
   cmake --build build\windows-x64-winui-debug --config Debug --target ClassMngrWindowsWinUI -- /m:2 /p:TrackFileAccess=false
   ```

2. Rerun the UIA Classes → My Workspace sequence. Check globally for `Personal details form` and `Personal details description`, not descendants under `My Information workspace tab` because the form is intentionally outside the Pivot.

3. If the blank state remains, capture the following at startup and at 100/300/700/1500/2500 ms after returning: presence/descendant count of `My Workspace content`, selected state of `My Information workspace tab`, `Schedule workspace tab`, and `Calendar workspace tab`. The likely remaining issue is a late valid Pivot selection event after `Loaded`; factor the visibility reset into a helper and schedule a low-priority dispatcher reset after reattachment, or disable caching for the Home page if that is the smallest reliable fix.

4. Run the existing Phase 6 smoke test with `--phase6-personal-details-test`, then `git diff --check` and review the diff.

## Artifacts and processes

Diagnostic captures created during this task are under:

- `artifacts/diagnostic-personal-details/`
- `artifacts/diagnostic-personal-details-postfix/`

They are untracked and can be removed only after confirming they are the task’s generated directories. The interrupted build process was stopped; check for any remaining `ClassMngrWinUI` or MSBuild processes before launching another UI test.
