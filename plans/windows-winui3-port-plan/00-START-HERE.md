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
review on 2026-09-07. Phase 5 is complete; its shell geometry slice,
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
for startup/no-database; the owner-approved Phase 5 exception covers the
missing empty/populated/error Qt fixtures. Owner review of all five WinUI
scenarios and interactive picker/unsaved-change, keyboard/Korean IME, focus,
DPI, and accessibility review are approved. The Terra first-navigation
recommendation and measurement pass the provisional guardrails, and the
Qt-derived table-parity handoff was reviewed and accepted as Phase 6 input on
2026-09-08. Phase 6 is in progress: its shared Qt-derived table-parity
resource foundation and Campus Directory selector-plus-five-tabs
reconciliation are complete as the opening slice. The Personal Details
implementation is committed at `f1f9ba7`; its x64 Debug build, focused engine
tests, and staged lifecycle check pass. The Korean Teacher directory
implementation is committed at `1baea21`; its x64 Debug build, focused engine
tests, and staged lifecycle check pass. The Native English Teacher and GS Team
directory slices are also committed and pass their x64 Debug focused tests and
  staged checks. The class details, class information, and notes slice is
  committed at `951f272`; its x64 Debug build, focused ClassInfo/ClassRepository
  tests (2/2), and staged lifecycle check pass. The calendar viewing/editing
  and preferences slice is committed at `33f33e6`; its x64 Debug build,
  focused calendar/settings engine tests (5/5), and direct Phase 6 hooks pass.
  The roster, transfer, and template slice is committed at `3ed19f9` with
  evidence in `af4ff5d`; its focused roster engine tests and direct Phase 6
  hook pass. The schedule editor slice is committed at `a5f4593`, and the
  schedule import/testing-class slice is committed at `37ba09b`; the combined
  x64 Debug build has zero warnings/errors, the focused schedule/testing engine
  tests pass 5/5, the direct `--phase6-schedule-test` hook passes, and the
  retained `--phase4-semantic-test` hook still passes. The speaking-evaluation
  grid and private-notes slice is committed at `499e367`; it replaces the
  Classes speaking prototype with the engine-backed 25-row, 11-column grid,
  roster-name import, score-range paste, validation, save/discard, and
  per-evaluation switching while keeping the existing Classes Pivot and its
  sliding tab transition. The x64 Debug build has zero warnings/errors, all
  11 speaking-related engine tests pass, and the direct
  `--phase6-speaking-evaluation-test` plus retained
  `--phase4-semantic-test` hooks pass. The speaking analytics slice is
  committed at `dc8a790`; it adds evaluation-scope selection, engine-backed
  summary/criterion metrics, class shape and year-to-date results, and student
  ranking on the same Classes Pivot, preserving its sliding tab transition.
  Its x64 Debug build has zero warnings/errors, all 11 speaking-related engine
  tests pass, and the Phase 6 speaking plus retained Phase 4 hooks pass. The
  active speaking AI-comment slice is committed at `b690203`; it adds
  privacy-preserving single-student and batch prompt generation, clipboard and
  ChatGPT handoff, pasted-response parsing, placeholder replacement, comment
  length enforcement, and persistence through the same Classes Pivot. Its x64
  Debug build has zero warnings/errors, all 11 speaking-related engine tests
  pass, and the direct Phase 6 speaking plus retained Phase 4 hooks pass. The
  speaking batch-report operations slice is committed at `45403aa`; it adds an
  engine-backed renderer-neutral plan surface for Internal and PowerPoint
  renderers, Standard and Advanced templates, PDF/print selection, output
  folder requirements, and archive-versus-individual-PDF policy. Actual PDF,
  print, and Office execution remains a Phase 7 adapter responsibility. Its
  x64 Debug build has zero warnings/errors, all 11 speaking-related engine
  tests pass, and the direct Phase 6 speaking plus retained Phase 4 hooks pass.
  The substitute-preparation slice is committed at `2d52d9d`; it adds the
  WinUI Sub Prep route with Important Information, Schedule, and Class
  Information Pivot tabs, engine-backed document preview, editable settings,
  save/discard behavior, and a direct `--phase6-sub-prep-test` hook. The
  bundled-document planning slice is committed at `c8b8234`; it adds date and
  class selection, roster-template selection, deterministic package-path
  planning, and a renderer-neutral plan preview without performing output.
  Both step-7 slices preserve the sliding tab transition recorded in the Phase
  6 plan. The WinUI SDK macro correction is committed at `b99ed8f`; the x64
  and x86 Debug/Release WinUI targets now build with zero warnings/errors, the
  focused Phase 6 engine suites pass 23/23 in all four lanes, and the direct
  Sub Prep hook exits 0 in all four lanes. The x86 stage smoke gate passes in
  both configurations; x64 stage verification still stops at the existing
  Phase 3 semantic hook with access-violation code `-1073741819`. Resource
  manifest verification passes in x64 Debug and still reports the known
  `ClassMngr_en_AU.ts` size mismatch in x64 Release and both x86 lanes. Actual
  PDF, print, ZIP, and Office execution remains a Phase 7 adapter
  responsibility. Phase 6 migration-order implementation slices for steps 1-7
  are complete; paired-visual, table-parity, resource/semantic, and aggregate-
  verifier closure evidence remains open. Each implementation slice is accepted
  and committed independently; the remaining evidence is tracked as
  phase-level closure work.
  Touch,
high-contrast, and accessibility automation remain out of scope for Phase 4.

## Phase 5 Completion Handoff

Phase 5 is complete through the recorded implementation/evidence commits and
the owner-approved table-parity handoff. Host-level x64/x86 Debug/Release
builds and staged verification are complete. Phase 6 begins by reconciling the
Qt-derived table-parity contract with the Phase 5 Campus surface.

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
  marks missing Qt evidence instead of fabricating a pair; and
- the Terra-recommended x64 Release first-navigation measurement: 20/20
  independent samples passed, with nearest-rank p95 `52.881 ms` and maximum
  `63.1 ms`.

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
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\scripts\porting\windows\measure_phase5_winui.ps1 -StageDirectory .\dist\ClassMngr-windows-winui-x64\Release -Platform x64 -FirstNavigation -Iterations 20 -ReportPath .\artifacts\phase5\measurements-20260908-x64\phase5-first-navigation-x64-release.json
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
The dedicated x64 Release first-navigation report contains 20/20 valid
samples, nearest-rank p95 `52.881 ms`, and maximum `63.1 ms`; its raw samples
are retained beside the report.

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

### Git index access note

In the restricted sandbox, Git staging and commit operations can fail with
`Unable to create '.git/index.lock': Permission denied` because the repository
index is not writable there, even though the working tree is writable. When a
commit is explicitly required, run the Git operation from a host-level shell
with write access to `.git`; on this machine, use the full Windows PowerShell
executable at `C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe`
instead of the WindowsApps `pwsh.exe` alias.

Next work, in order:

1. Close the remaining Phase 6 x64 semantic/resource, x86 resource,
   paired-visual, table-parity, and aggregate-verifier evidence, then complete
   the Phase 6 exit review.
2. Commit every independently accepted closure slice or evidence step with a
   `Phase 6 - ...` subject; keep the tabbed-page motion note as the reference
   for any future in-page tab work.

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

- **2026-09-11 - Speaking report/editor workflow review repaired.** The AI
  comment voice selector now appears on the Create Prompt step, making the
  existing Third Person input reachable for batch and report-editor prompts.
  Editing a parsed batch comment updates that row's character count, displayed
  validity, enabled state, and apply selection immediately. `git diff --check`
  passes. The host-level x64 Debug WinUI build now compiles and stages the
  target successfully, and the staged `--phase6-speaking-evaluation-test` hook
  exits 0. The complete x64 Debug staged verifier passes all 23 checks;
  interactive review remains pending.

- **2026-09-08 - Phase 6 opening data-entry migration slice implemented.** The
  shared `TableStyles.xaml` resource dictionary now carries the Qt-derived
  table tokens, metrics, state styles, and compact schedule variants. The
  Campus Directory surface now uses a selector plus Information, Directions,
  Address, Housing, and Maps tabs while preserving the engine-backed records,
  localized content, async image guards, and existing Phase 5 checks. The
  x64 Debug WinUI target, focused Campus/ResourcePack engine tests (2/2), and
  complete staged verifier pass. The step-1 and step-2 feature slices have
  x64 implementation baselines; paired visual evidence, persistence checks,
  and x86 Debug/Release evidence remain pending for their closure gates.

- **2026-09-09 - Phase 6 Personal Details slice implemented.** Revision
  `f1f9ba7` adds the WinUI Personal Details page to the My Information route,
  backed by `PersonalDetailsService`, with load/save/discard, dirty-state,
  validation, no-database/error states, Korean text entry, Zoom N/A handling,
  typed-signature controls, and retained-image messaging. The x64 Debug WinUI
  target, focused ApplicationSettings/PersonalDetails engine tests (2/2), and
  complete staged verifier pass, including `--phase6-personal-details-test`.
  Image selection, x86 evidence, and paired Qt/WinUI visual evidence remain
  for the closing gate.

- **2026-09-09 - Phase 6 Korean Teacher directory slice implemented.** Revision
  `1baea21` adds the engine-backed Korean Teachers route with the retained Qt
  teacher fields, Korean IME-capable name entry, preferred-name choices,
  connectivity and notes controls, new/edit/delete/discard behavior, dirty
  selection protection, and engine validation/error presentation. The x64
  Debug WinUI target, focused Teacher/TeacherImport engine tests (2/2), and
  complete staged verifier pass, including
  `--phase6-korean-teacher-test`, which covers empty/create/invalid/update and
  no-database states. x86 and paired Qt/WinUI visual evidence remain open.

- **2026-09-09 - Phase 6 Native English Teacher directory slice implemented.**
  Revision `856ccf1` adds the engine-backed Native English Teachers route with
  the retained six-column directory fields, position choices, Korean IME-capable
  name entry, new/edit/delete/discard behavior, dirty selection protection, and
  MM-dd birthday plus engine validation/error presentation. The x64 Debug WinUI
  target, focused NativeEnglishTeacher engine test (1/1), and complete staged
  verifier pass, including `--phase6-native-english-teacher-test`, covering
  empty/create/invalid/update and no-database states. x86 and paired Qt/WinUI
  visual evidence remain open.

- **2026-09-09 - Phase 6 GS Team directory slice implemented.** Revision
  `3a733a6` adds the engine-backed GS Team route with bilingual names,
  position/contact/birthday fields, Korean IME-capable name entry,
  new/edit/delete/discard behavior, dirty selection protection, and MM-dd
  birthday plus engine validation/error presentation. The x64 Debug WinUI
  target, focused GsTeam engine test (1/1), and complete staged verifier pass,
  including `--phase6-gs-team-test`, covering empty/create/invalid/update and
  no-database states. x86 and paired Qt/WinUI visual evidence remain open.

- **2026-09-09 - Phase 6 class information slice implemented.** Revision
  `951f272` adds the engine-backed Classes Details and Notes tabs with class
  directory selection, class information create/edit/delete/discard, shared
  `ClassInfoConfig` options and validation, teacher display, notes and
  time-filler persistence, dirty-selection protection, and explicit
  no-database/empty/error states. The existing Classes Pivot remains the owner
  of Details, Roster, Speaking Evaluations, Analytics, and Notes, preserving
  the tabbed-page motion contract recorded in the Phase 6 plan. The x64 Debug
  WinUI target, focused ClassInfo/ClassRepository engine tests (2/2), and
  complete staged verifier passed with
  `--phase6-class-information-test`. Step-1 and step-2 x86, paired Qt/WinUI
  visual, and table-parity evidence remain open.

- **2026-09-09 - Phase 6 calendar slice implemented.** Revision `33f33e6`
  adds the engine-backed Calendar and Preferences tabs with month navigation,
  selected-day event viewing, add/edit/delete and recurrence validation,
  academic schedule and display-preference persistence, reset behavior, and
  explicit no-database/empty/populated states. The nested Calendar Pivot keeps
  the Phase 6 tabbed-page motion contract, including the sliding transition
  reference for future tabbed slices. The x64 Debug WinUI target built with
  zero warnings/errors, the focused calendar/settings engine tests passed 5/5,
  and all direct Phase 6 hooks passed, including
  `--phase6-calendar-test`. The aggregate staged verifier was attempted twice
  but stopped at the existing Phase 3 semantic sequence with crash code
  `-1073741819`; its isolated semantic hook and all Phase 6 hooks pass. Step-1
  and step-2 x86, paired Qt/WinUI visual, and table-parity evidence remain open;
  step 4 is now active.

- **2026-09-09 - Phase 6 speaking-evaluation grid slice implemented.** Revision
  `499e367` replaces the Classes speaking prototype with an engine-backed
  25-row, 11-column evaluation grid covering roster names, six score columns,
  comments, and private Notes. It adds evaluation-term switching, roster-name
  import, score-range paste, engine normalization/validation, save/discard,
  dirty-selection protection, and no-database/error states. The implementation
  stays in the existing Classes Pivot so the current sliding tab transition
  remains the reference for this and future tabbed slices. The x64 Debug WinUI
  build completed with zero warnings/errors, all 11 speaking-related engine
  tests passed, and both direct `--phase6-speaking-evaluation-test` and the
  retained `--phase4-semantic-test` hooks exited 0. Analytics, AI comments,
  and batch behavior remain for the rest of step 6; x86, paired Qt/WinUI
  visual, table-parity, and aggregate-verifier evidence remain open.

- **2026-09-09 - Phase 6 speaking analytics slice implemented.** Revision
  `dc8a790` adds evaluation-scope selection, engine-backed summary and
  criterion metrics, class-shape and year-to-date results, and a read-only
  student ranking to the existing Classes Analytics tab. Refreshes follow
  class/evaluation changes and saved speaking data, with explicit
  no-database, empty, and engine-error states. The slice remains inside the
  existing Classes Pivot so its sliding tab transition remains the reference
  for future tabbed slices. The x64 Debug WinUI build completed with zero
  warnings/errors, all 11 speaking-related engine tests passed, and both
  direct `--phase6-speaking-evaluation-test` and retained
  `--phase4-semantic-test` hooks exited 0. AI comments and batch behavior
  remain for the rest of step 6; x86, paired Qt/WinUI visual, table-parity,
  and aggregate-verifier evidence remain open.

- **2026-09-09 - Phase 6 speaking AI-comment slice implemented.** Revision
  `b690203` adds a privacy-preserving AI-comments card to the existing Classes
  Speaking Evaluations Pivot. It supports direct-to-student and third-person
  voices, formatted private observations, single-student and batch prompts,
  `STD_NAME`/stable batch-ID placeholders, clipboard plus ChatGPT handoff,
  pasted-response parsing, review-before-apply behavior, 450-character
  enforcement, and persistence through the engine-backed evaluation grid. The
  x64 Debug WinUI build completed with zero warnings/errors, all 11
  speaking-related engine tests passed, and both direct
  `--phase6-speaking-evaluation-test` and retained `--phase4-semantic-test`
  hooks exited 0. Batch report operations remain for step 6; x86, paired
  Qt/WinUI visual, table-parity, and aggregate-verifier evidence remain open.

- **2026-09-09 - Phase 6 substitute-preparation slice implemented.** Revision
  `2d52d9d` adds the engine-backed WinUI Sub Prep route with Important
  Information, Schedule, and Class Information Pivot tabs, document-preview
  data, editable substitute settings, save/discard behavior, and a direct
  `--phase6-sub-prep-test` hook. The three tabs preserve the sliding transition
  recorded in the Phase 6 plan as the reference for future tabbed slices. The
  x64 Debug WinUI target built with zero warnings/errors; the focused Sub Prep,
  Schedule, ClassInfo, and Roster engine tests passed 12/12; and the Sub Prep
  plus retained Phase 4 hooks exited 0. Bundled output remains deferred to the
  following planning slice and, for actual PDF/print/Office execution, Phase 7.

- **2026-09-09 - Phase 6 bundled substitute-document planning committed.**
  Revision `c8b8234` adds date and class selection, roster-template choice,
  deterministic relative package-path planning, and a renderer-neutral planned
  file list to the Sub Prep Schedule tab. It does not create PDFs, ZIP files,
  printed output, or Office documents; those remain Phase 7 adapter work. The
  x64 Debug WinUI target built with zero warnings/errors, the focused Sub Prep
  engine tests passed 4/4, and both the direct Sub Prep and retained Phase 4
  hooks exited 0. Migration-order implementation steps 1-7 are now complete;
  x86, paired Qt/WinUI visual, table-parity, and aggregate-verifier closure
  evidence remains open.

- **2026-09-09 - Phase 6 closure audit and WinUI SDK warning correction.**
  Revision `b99ed8f` undefines the Windows SDK `GetCurrentTime` macro after
  `windows.h` so it cannot collide with the C++/WinRT ABI method in generated
  headers. The x64/x86 Debug/Release WinUI targets build with zero
  warnings/errors; the focused Phase 6 engine suites pass 23/23 in every lane;
  and `--phase6-sub-prep-test` exits 0 in every lane. The x86 stage smoke gate
  passes in Debug and Release. x64 stage verification remains blocked by the
  existing Phase 3 semantic-test access violation (`-1073741819`), while the
  resource-manifest check passes in x64 Debug and still reports the known
  `ClassMngr_en_AU.ts` size mismatch in x64 Release and both x86 lanes.
  Paired Qt/WinUI visual, table-parity, and aggregate-verifier closure work
  remains open.

- **2026-09-08 - Phase 6 migration ledger activated.** The progress dashboard
  now reports Phase 6 as **In progress**. The active gate is migration-order
  step 1, beginning with Personal Details and then the three teacher-directory
  families. Each feature slice and migration step will have its own reviewed
  implementation commit and evidence entry; no later step is accepted merely
  because an earlier prototype exists.

- **2026-09-08 - Phase 5 Campus database hydration regression fixed.** The
  cached Campus Information page now re-queries `CampusRecordService` when
  navigation returns after a database is opened, so a prior no-database or
  empty page cannot mask records from the active `.tps` file. The no-database
  scenario hook also no longer creates an unintended empty in-memory database.
  The x64 Debug WinUI target rebuilt successfully; `--phase5-campus-test`
  exited 0; fresh five-state captures validate; and an end-to-end launch with
  `tests/fixtures/database-port/typical.tps` displayed its `Fixture Campus`
  record on the Campus page.

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
