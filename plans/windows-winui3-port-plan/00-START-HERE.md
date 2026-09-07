# ClassMngr Windows WinUI 3 Port — Start Here

## Purpose

This directory is the plan of record for replacing the Windows Qt presentation
with WinUI 3 while keeping one portable C++23 engine and retaining the Qt
application on macOS and Linux.

Progress history and the all-phase dashboard live in
[progress-log.md](progress-log.md). Keep this file focused on stable plan
rules and the active phase handoff.

## Target Outcome

```text
ClassMngrEngine (portable C++23; no Qt, WinUI, WinRT, or Win32 UI types)
    |-- domain models, validation, rules, and use cases
    |-- SQLite repositories and schema migrations
    |-- import/export and renderer-neutral report models
    |-- platform-service interfaces
    |
    +-- ClassMngrQtDesktop
    |       |-- macOS Qt application and adapters
    |       +-- Linux Qt application and adapters
    |
    +-- ClassMngrWindowsWinUI (x64 release; x86 build/test support)
            |-- WinUI 3 XAML presentation
            |-- C++/WinRT view models and adapters
            |-- Windows App SDK lifecycle and resources
            |-- Windows platform-service implementations
            +-- optional Direct2D surfaces for specialized rendering only
```

WinUI 3 owns the Windows shell, layout, standard controls, focus, input,
theming, and accessibility semantics. ClassMngr will not build a
general-purpose Win32/Direct2D control toolkit. Direct2D or DirectWrite interop is
permitted only for a measured feature need such as a chart, report preview, or
other specialized drawing surface.

## Fixed Decisions

- The Windows presentation uses WinUI 3 from a pinned stable Windows App SDK.
- The Windows UI is C++/WinRT with XAML and links the shared C++23 engine.
- The supported Windows floor becomes Windows 10 version 1809 (build 17763),
  or a newer floor if required by the pinned Windows App SDK at implementation
  time. The build and installer must enforce the same value.
- Development and initial release packaging are unpackaged and self-contained,
  preserving the existing Inno Setup and signed-installer update workflow.
  Package identity may be reconsidered only through a separate ADR.
- Windows x64 is the release gate. Windows x86 is a required build-and-test
  target with Debug and Release configurations, engine tests, application
  smoke tests, and self-contained staging. An x86 public installer or updater
  artifact requires a separate demand and release-support decision.
- ARM64 remains build-only and non-blocking until a separate support decision
  supplies runtime, performance, and package evidence.
- The Qt Windows product remains the shipping fallback until WinUI parity and
  cutover gates pass. Development builds use isolated identity, settings, and
  copied databases.
- macOS and Linux retain their Qt UI, deployment, and test workflows.
- No product rule, SQL statement, migration, or validation rule is duplicated
  in the WinUI presentation.

These decisions supersede the presentation-stack and Windows-version choices
in ADR 0001. The replacement decision is recorded in
[ADR 0002](../../docs/porting/adr/0002-winui3-windows-presentation.md).

## How to Use This Plan

For implementation work, load this file and only the active phase file. Finish
the active phase gate before beginning broad work from a later phase. Consult
[progress-log.md](progress-log.md) only when historical status or cross-phase
evidence is needed.

## Qt Build-Time Note

Because the project includes substantial features and supports multiple operating
systems, Qt builds often take longer than initially expected. Use the initial
estimate as a baseline and add 50% when building on a computer named
`Felt-Desktop`. Add 100% on `Felt-Work`, as it is a six-year-old ultra-thin
laptop.

## Completion Commit Note

When a job is complete and its required validation has passed, automatically
create a commit for the completed work without waiting for another prompt.

## Commit Message Note

Use the format `Phase 2 - <short description>` with normal spaces between
words. For example: `Phase 2 - Document catalog service`. Do not use
concatenated or slug-style subjects such as `Phase2-document-catalog-service`.

## Current Focus

Phase 4 is complete. Its shared UX layer, first-party virtualization decision,
three editor prototypes, input and Korean IME protocols, large-data gate,
chart strategy, and staged semantic control check passed the accepted exit
review on 2026-09-07. Phase 5 is now in progress; its shell geometry slice,
Qt-ordered menu/sidebar shell, engine-backed existing-database Open/recent
startup flow, native file-dialog policy, and native New/create flow are
committed. The engine-backed Campus Information read-only baseline is also
committed: the page uses `CampusRecordService`, retains list/detail state,
and covers no-database, empty, populated, engine-error, and Korean-text
semantic states. Native save/export/folder commands use `FileSavePicker`,
`FolderPicker`, and engine/file-system boundaries. Campus resources are staged
through `WindowsResourceProvider`, optional map images are loaded asynchronously,
the feature labels/content are localized, and required WinUI-only Campus
resource keys are guarded during `.resw` generation. Deterministic
paired-scenario hooks cover startup, no-database, empty, populated, and error
states. The
paired-scenario manifest, WinUI captures, x64 runtime measurements, and
x64/x86 Debug/Release host builds are recorded. Qt evidence is complete only
for startup/no-database; empty/populated/error need matching Qt fixtures or an
approved exception. Owner review and interactive picker/unsaved-change,
keyboard/Korean IME, focus, DPI, and accessibility review remain. Touch,
high-contrast, and
accessibility automation remain out of
scope for Phase 4.

## Phase 5 Resume Handoff

Resume from commit `8d11eec` (`Phase 5 - Guard required WinUI resources`).
Host-level x64/x86 Debug/Release builds and staged verification are complete;
the remaining work is owner acceptance of the recorded evidence and the
unresolved paired-Qt and interaction-review gates.

Finished Phase 5 work:

- shell geometry, the Qt-ordered menu/sidebar shell, and retained navigation
  state;
- engine-backed existing-database Open, recent-database startup, native
  `FileOpenPicker` policy, and native New/create through `FileSavePicker`;
- Campus Directory > Information as a read-only `CampusRecordService` page
  with list/detail state, scrolling, all record fields, and explicit
  no-database, empty, populated, and engine-error states;
- the deterministic `--phase5-campus-test`, including an in-memory Korean
  campus fixture; and
- the staged verifier invocation for the new Phase 5 activation check;
- native Save, Save As, Close, current-page JSON export, and campus-resource
  folder export through the operating-system picker boundary;
- packaged campus resource staging, `WindowsResourceProvider` reads, optional
  Campus map image loading, and localized Campus Information labels/content;
- the shared-catalog fallback for WinUI resources, including restoration of the
  required `CampusInformationPage` context and a generator guard for its
  `Campus Information` and `Name` keys; and
- deterministic `--phase5-campus-no-database`, `--phase5-campus-empty`,
  `--phase5-campus-populated`, and `--phase5-campus-error` launch hooks; and
- the paired-scenario runner that records real WinUI captures and honestly
  marks missing Qt evidence instead of fabricating a pair.

Validation already passed:

```text
ctest --test-dir build\windows-x64-winui-debug -C Debug -R "ClassMngrEngine(CampusRecordService|ResourcePackPolicy)Tests" --output-on-failure
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\scripts\verify_windows_winui_stage.ps1 -StageDirectory .\dist\ClassMngr-windows-winui-x64\Debug -Platform x64
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\scripts\verify_windows_winui_stage.ps1 -StageDirectory .\dist\ClassMngr-windows-winui-x64\Release -Platform x64
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\scripts\verify_windows_winui_stage.ps1 -StageDirectory .\dist\ClassMngr-windows-winui-x86\Debug -Platform Win32
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\scripts\verify_windows_winui_stage.ps1 -StageDirectory .\dist\ClassMngr-windows-winui-x86\Release -Platform Win32
ctest --test-dir build\windows-x86-winui-debug -C Debug -R ClassMngrEngineCampusRecordServiceTests --output-on-failure
ctest --test-dir build\windows-x86-winui-release -C Release -R ClassMngrEngineCampusRecordServiceTests --output-on-failure
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\scripts\porting\windows\validate_winui_scenario_artifacts.ps1 -ArtifactRoot .\artifacts\phase5\paired-20260908-x64-debug-clean2 -RequirePassed
git diff --check
```

All four host-level WinUI targets build successfully, and all four staged
verifiers pass the complete Phase 1-5 sequence. The focused Campus/ResourcePack
engine tests pass in x64 Debug and x86 Debug/Release; strict `/WX` direct MSVC
translation-unit checks, translation-catalog XML validation, generated `.resw`
validation, and PowerShell AST parsing also pass. The paired manifest validator
accepts five WinUI sidecars, and the x64 Debug/Release measurement reports each
contain three successful clean iterations. The restricted local CMake WinUI
target still reproduces the environment's `Microsoft.Build.Utilities.FileTracker`
`UnauthorizedAccessException`; the host-level workaround is documented below.

### Avoiding the WinUI FileTracker error

If a WinUI build reports `MSB4018` from the MIDL task with
`Microsoft.Build.Utilities.FileTracker` and `E_ACCESSDENIED`, run the build
from a full Windows developer or CI host rather than a restricted sandbox.
That host must grant the invoking account write access to the repository,
architecture/configuration build tree, the Windows SDK/MSBuild intermediate
directories, and the normal user temporary/AppData locations; NuGet restore
also needs its configured package source to be reachable. The reproducible
host-level command is:

```powershell
cmake --build build\windows-x64-winui-debug --config Debug --target ClassMngrWindowsWinUI --parallel 2
```

The WinUI project already sets `TrackFileAccess=false`, and the build wrapper
passes the same property, but that property does not prevent MIDL's static
`FileTracker` initialization. Do not keep changing application source or
package inputs when this exact stack trace occurs: all four required targets
build successfully once the command has host-level filesystem access. Apply
the same host rule to the x86 Debug/Release and x64 Release lanes before
collecting runtime evidence.

Next work, in order:

1. Obtain owner review of the five recorded WinUI scenarios and their paired
   manifest. Add matching Qt empty/populated/error fixtures, or record an
   explicitly approved exception for the missing Qt evidence.
2. Complete interactive review of native picker and unsaved-change behavior,
   keyboard/Korean IME, focus, DPI, and accessibility; also decide the
   first-navigation cap that Phase 0 left open.
3. Record the [table layout and style parity plan](../../docs/porting/windows-winui/table-parity-plan.md)
   as the first Phase 6 shared work item; revisit the completed Phase 4
   prototypes and Phase 5 read-only list/detail surface before accepting any
   table-heavy feature slice.
4. Do not mark Phase 5 complete or begin Phase 6 until the phase-file exit
   gate and paired evidence are accepted.

For the all-phase dashboard, completed-phase evidence, and historical progress
log, see [progress-log.md](progress-log.md).

## Progress Update Rule

After meaningful work:

1. Update the active-phase status and handoff in this file.
2. Add concise dated evidence below `Current Phase Progress` with the tested
   revision, platform, and result.
3. Update `Current Focus` to the next incomplete gate.
4. Change a phase to **Complete** only when its phase-file exit gate passes;
   record the dashboard and historical entry in
   [progress-log.md](progress-log.md).
5. When the active phase changes, move the completed phase's detailed entries
   into [progress-log.md](progress-log.md) and start a fresh current-phase
   section here.
6. Keep detailed commands, measurements, screenshots, and failure analysis in
   test artifacts or evidence documents; link them here instead of copying
   long logs.

## Current Phase Progress

- **2026-09-08 - Phase 5 catalog fallback regression repaired and guarded.**
  Revision `27cf1d0` restores the WinUI-only `CampusInformationPage` context
  that was absent after the shared catalog refresh, preserving English fallback
  text and the Korean `Campus Information`/`Name` values. Revision `8d11eec`
  makes the generator fail clearly if either required default-catalog key is
  removed in a future refresh. Clean generation emits 2,048 entries per locale
  (10,240 total), all five `.ts` catalogs and generated `.resw` files parse,
  and the x64/x86 Debug/Release host builds plus complete staged verifiers pass
  against the repaired resources.

- **2026-09-08 - Phase 5 host verification and resource staging corrected.**
  Revisions `9fce845` and `4612eaa` correct the campus-resource destination
  layout and make the Phase 1 smoke hook independent of prior hook order. All
  four host-level x64/x86 Debug/Release WinUI targets build successfully; all
  four staged verifiers pass the complete Phase 1-5 sequence, and each stage
  contains the expected top-level campus resource files without a nested
  `resources/campuses/campuses` directory.

- **2026-09-08 - Phase 5 evidence tooling and interactive evidence recorded.**
  Revisions `9782f2b` and `32ced5b` make Qt-less paired-capture mode explicit,
  prevent fixture-mismatched Qt reuse, validate the known pair manifest, and
  keep captures in the foreground. The clean x64 Debug record contains five
  WinUI scenario PNG/metadata pairs; startup and no-database link matching Qt
  evidence, while empty/populated/error report missing Qt fixtures. The x64
  Debug and Release measurement reports each contain three successful clean
  iterations and pass the evaluated 200 MiB working-set target.

- **2026-09-08 - Phase 5 native output and Campus resource/parity hooks
  committed.** Revisions `11ae126` and `266ee11` add native Save, Save As,
  Close, current-page JSON export, and campus-resource folder export; package
  campus assets through the resource boundary; load optional Campus map images;
  localize the Campus Information surface; and add deterministic WinUI
  startup/no-database/empty/populated/error hooks plus the paired-capture
  runner. Focused engine tests passed 2/2, strict direct MSVC source checks
  passed, all five translation catalogs and generated `.resw` files parsed,
  and `git diff --check` passed. The local CMake WinUI build remains blocked by
  `FileTracker` access denial; real paired captures, measurements, and x86
  Debug/Release evidence remain required.

- **2026-09-08 - Phase 5 measurement and interim exit review recorded.** The
  Phase 5 measurement helper now records cold/warm launch timing, visible-window
  first-paint and scenario-ready proxies, resize latency/bounds, working-set
  and private memory, peak working set, and process handles without inventing
  unavailable budgets. The interim [Phase 5 exit review](../../docs/porting/windows-winui/phase5-exit-review.md)
  keeps the phase **In progress**. The focused Campus/ResourcePack engine tests
  pass 2/2 in x86 Debug and 2/2 in x86 Release; full x86 WinUI Debug and
  Release builds reach pinned resource generation but remain blocked by the
  host `FileTracker` access-denied failure. Runtime captures and measurement
  JSON remain pending a usable rebuilt stage and interactive desktop.

- **2026-09-07 - Phase 5 Campus Information read-only slice committed.**
  Revision `697f1fb` enables the Campus Directory > Information navigation
  item and renders a scrollable engine-backed campus list/details page without
  ad hoc SQL. No-database, empty, populated, and engine-error states are
  explicit; all campus record fields are shown read-only; and
  `--phase5-campus-test` seeds an in-memory Korean campus and checks the
  populated and reset states. The x64 Debug WinUI build, staged verifier
  (including all Phase 1-5 checks), and
  `ClassMngrEngineCampusRecordServiceTests` CTest passed. Image/resource
  loading, localized feature parity, native save/export/folder workflows,
  paired Qt/WinUI scenarios, and startup/runtime measurements remain.

- **2026-09-07 - Phase 5 native New/create flow committed.** Revision
  `ddeb818` enables File > New through an HWND-bound native `FileSavePicker`,
  restricts new profiles to `.tps`, closes/replaces the selected active
  database, creates through engine `OpenDatabase` with parent-directory
  creation, updates recents/status, and keeps Save/Close disabled. The x64
  Debug `ClassMngrWindowsWinUI` build and staged verifier passed; established
  SDK/PRI/lib-path warnings remain non-blocking. The next gate is save/export/
  folder flow and the first read-only feature slice.

- **2026-09-07 - Phase 5 native file-dialog policy recorded.** Revision
  `097e15d` records that WinUI file and folder workflows use native Windows
  picker surfaces rather than recreating the Qt custom `QFileDialog` UI. The
  existing Open implementation in `0295020` already uses an HWND-bound native
  `FileOpenPicker`; later create, save, import, export, and directory-selection
  slices must use the matching native picker APIs. The next gate is the
  create/save/export flow implementation.

- **2026-09-07 - Phase 5 engine-backed open/recent flow committed.** Revision
  `0295020` enables File > Open... through an HWND-bound native WinUI file
  picker for `.tps` and `.db`, opens selected files through engine
  `OpenDatabase`, retains up to ten existing recent paths in shell state, and
  honors explicit startup targets or most-recent startup restore while leaving
  create/save/close disabled. The x64 Debug `ClassMngrWindowsWinUI` build and
  staged verifier passed all existing Phase 1/3/4 checks. Existing SDK/PRI
  warnings remain non-blocking. The next gate is create/new database flow and
  broader file-dialog coverage.

- **2026-09-07 - Phase 5 menu/sidebar shell slice committed.** Revision `a1fab16`
  adds the Qt-ordered File, Edit, Classes, Teachers, Print / Export, Help,
  Admin, and Developer menu surface with representative tags and keyboard
  accelerators, plus the Qt-shaped sidebar hierarchy for workspace, classes,
  sub preparation, campus staff, useful links, and campus directory. The
  x64 Debug `ClassMngrWindowsWinUI` build passed. The first staged smoke run
  caught a runtime GridLength binding error in the menu row; the row was made
  explicitly 28 px and the corrected staged verifier passed all Phase 1/3/4
  manifest, smoke, input, theme, DPI, lifecycle, navigation, view-model,
  localization, dialog, threading, semantic, and Phase 4 semantic checks.
  Existing SDK/PRI warnings remain non-blocking. The next gate is startup,
  settings resolution, and database/file flows.

- **2026-09-07 — Phase 5 shell geometry slice committed.** Revision `7df035b`
  adds an explicit WinUI shell body grid with Qt-aligned 150–400 px sidebar
  bounds, a 600 px content minimum, Qt-equivalent body/sidebar margins, and
  the 800×600 minimum / 1270×1040 default window geometry. The x64 Debug
  `ClassMngrWindowsWinUI` build passed after pinned package restore, and the
  staged verifier passed all existing Phase 1/3/4 manifest, smoke, input,
  theme, DPI, lifecycle, navigation, view-model, localization, dialog,
  threading, semantic, and Phase 4 semantic checks. Existing SDK/PRI warnings
  remain non-blocking. The next gate is the startup/menu/sidebar navigation
  shell slice.

## Shared Completion Rules

- Every phase preserves `.tps` and supported legacy `.db` behavior.
- Every migrated slice uses the same engine rules and schema as the Qt apps.
- Korean text and IME, keyboard-only use, DPI, theme, failure paths, and
  unsaved-change behavior are acceptance work, not final polish.
- Screenshots supplement semantic and behavioral assertions; they do not prove
  focus, persistence, IME, or accessibility behavior.
- The WinUI target must not load or deploy Qt.
- Windows-owned source and approved dependencies must build for x64 and x86.
  An exception requires an explicit plan update with replacement or isolation
  work; silently dropping x86 is not permitted.
- The retained Qt products must remain releasable until Windows cutover.
