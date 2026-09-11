# My Information page handoff

## Deployment

`my_information_lifecycle_20260911` is paused after a successful targeted
build. The source fix is compile-verified, but its return-navigation behavior
remains UI Automation-unverified.

## Issue and root-cause evidence

The WinUI 3 My Information page can be blank after navigating from My
Workspace to Classes and back, although personal details are present at fresh
startup. This is a cached WinUI `Frame`/`Pivot` lifecycle problem, not a
personal-details data problem.

UI Automation reproduced the sequence against the staged Debug executable:

1. Startup: `Personal details form` and `Personal details description` are
   present.
2. Select `Classes navigation root`.
3. Select `My Workspace navigation root`.
4. My Information is selected, but both personal-details elements are absent.

The earlier form was nested in the first `PivotItem`; rebinding, dispatcher
callbacks, a timer, and reparenting that nested content did not survive cached
page reattachment. The current layout keeps the Pivot as the tab strip and
uses sibling panels for personal information, schedule, and calendar.

Do not touch the unrelated pre-existing edit in
`src/platform/windows/winui/MainWindow_classes_page_details.cpp`.

## Current source-only fix

- `MainWindow.xaml.h` retains the personal-details `ScrollViewer` and its
  `ContentControl` host state.
- `MainWindow_navigation.cpp` disables Home-page navigation caching and keeps
  the Home-route first-tab/sibling-visibility reset. The cached-Pivot
  `Loaded` and low-priority reset callbacks were removed.
- `MainWindow_personal_details.cpp` detaches the retained `ScrollViewer` from
  an old host and explicitly reattaches it to each fresh Home-page host,
  preserving the stateful controls and event wiring.

## Validation and next entry point

The targeted x64 Debug WinUI build passed after an elevated retry:

```powershell
cmake --build build\windows-x64-winui-debug --config Debug --target ClassMngrWindowsWinUI -- /m:2 /p:TrackFileAccess=false
```

The build completed with zero errors and two pre-existing Visual Studio
library-path warnings. No UI Automation or focused My Information smoke check
was run.

For behavioral validation, run this sequence:

1. Launch the staged Debug executable and run the focused UI
   Automation sequence: verify both personal-details elements at startup,
   select Classes, return to My Workspace, and verify that My Information is
   selected and both elements are present globally. Do not require them to be
   descendants of `My Information workspace tab`; the form is outside the
   Pivot.
2. Run the existing Phase 6 personal-details smoke check with
   `--phase6-personal-details-test`.
3. Run `git diff --check` and review the assigned source/document changes.

Earlier evidence: the build before the removed `tabs.Loaded` addition
succeeded with `0 Warning(s), 0 Error(s)` and staged
`dist/ClassMngr-windows-winui-x64/Debug/ClassMngrWinUI.exe`; the focused UIA
check still failed after the sibling-host and invalid-selection guard; and
the rebuild after adding `tabs.Loaded` was interrupted at the one-minute
cutoff, so its MSBuild error reflected interruption rather than compilation.
