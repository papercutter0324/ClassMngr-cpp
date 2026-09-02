# ClassMngr Windows WinUI 3 Port — Start Here

## Purpose

This directory is the plan of record for replacing the Windows Qt presentation
with WinUI 3 while keeping one portable C++23 engine and retaining the Qt
application on macOS and Linux.

Progress is tracked only in this file. Phase files define scope, implementation
order, validation, and exit gates; they must not maintain a second status log.

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
the active phase gate before beginning broad work from a later phase.

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

Status vocabulary:

- **Not started:** no phase deliverable has been accepted.
- **In progress:** implementation or required validation is underway.
- **Blocked:** the phase cannot advance without an explicit decision or an
  external prerequisite.
- **Complete:** every exit-gate item has recorded evidence.

## Progress Dashboard

Last updated: 2026-09-03 (Asia/Seoul)

| Phase | Status | Current evidence or next gate |
| --- | --- | --- |
| [Phase 0 — Baseline and contracts](phase-0-baseline-and-contracts.md) | **Complete** | Owner-accepted Qt captures, parity inventory, fixture corpus, and retained-platform validation exist. |
| [Phase 1 — Build split and WinUI bootstrap](phase-1-winui-bootstrap.md) | **Complete** | Phase 1 exit gate passed: local and hosted VS 2026/v145 x64/x86 Debug/Release builds, staged smoke tests, retained Qt validation, and owner-reviewed WinUI/Qt visual evidence are complete. The hosted x86 Release idle-memory report is uploaded; representative feature-workload peak evidence is intentionally deferred until a realistic feature slice exists, so no x86 release peak-budget claim is made. |
| [Phase 2 — Portable engine extraction](phase-2-portable-engine-extraction.md) | **In progress** | Typed errors, UTF-8 path rules, six-version schema/OpenDatabase behavior, Qt-free class CRUD, testing-class/testing-block persistence and retained adapter wiring, campus-record CRUD and retained campus adapter wiring, teacher validation/CRUD and retained teacher/class-adapter wiring, directory services and retained Native English/GS Team adapter wiring, class-time validation and retained validator adapter wiring, class-information persistence, schedule reads/conflicts, schedule-builder workflows, schedule-import workflows and retained schedule-import adapter wiring, class-transfer workflows and retained class-transfer adapter wiring, teacher-import workflows and retained teacher-import adapter wiring, academic calendar rules, calendar-event normalization/filtering, validation/recurrence rules, and calendar-event persistence with retained adapter wiring, intensive-slot-state persistence with retained adapter wiring, speaking-evaluation grade calculation, speaking-evaluation grid validation and persistence with retained adapter wiring, report batch ZIP archive writing, shared document-output result semantics, report metadata, output and filename policy, content assembly, AI prompt rules, template policy, batch-export policy, PowerPoint job content, schedule reports and print labels, schedule-time formatter adapter wiring, roster reports, Qt-free roster persistence and its retained Qt adapter wiring, roster template policy and validation, class/teacher naming, class-tab navigation and retained adapter wiring, evaluation-default selection policy and retained adapter wiring, upcoming-birthday scheduling, class analytics, sub-prep class-information, pagination, package-planning, document-model, document-catalog policies, application-settings persistence and retained settings adapter wiring, personal-details settings persistence and retained personal-details adapter wiring, file-backed retained Qt DatabaseSession and CalendarEventCache preflight through engine OpenDatabase, the eleven-case fixture corpus round-trip gate, P2-01 report/export adapter boundaries, P2-02 portable file/output contracts, and P2-03 retained database/application-service adapter cleanup are extracted; broader per-slice cross-platform fixture coverage remains. See the [Phase 2 local validation record](../../docs/porting/windows-winui/phase2-local-validation.md). |
| [Phase 3 — WinUI application foundation](phase-3-winui-application-foundation.md) | **Not started** | Begins after the Phase 2 engine gate. |
| [Phase 4 — Shared UX and high-risk controls](phase-4-shared-ux-and-high-risk-controls.md) | **Not started** | Begins after the application foundation is stable. |
| [Phase 5 — Shell and first feature slice](phase-5-shell-and-first-feature-slice.md) | **Not started** | No feature parity is claimed by the current WinUI bootstrap shell. |
| [Phase 6 — Data-entry feature migration](phase-6-data-entry-feature-migration.md) | **Not started** | Port vertical slices in the risk order defined by the phase file. |
| [Phase 7 — Media, output, and OS services](phase-7-media-output-and-os-services.md) | **Not started** | PDF, printing, exports, updates, and PowerPoint remain Qt-owned. |
| [Phase 8 — Hardening, packaging, and cutover](phase-8-hardening-packaging-and-cutover.md) | **Not started** | The Qt Windows release remains public until this phase passes. |

## Current Focus

Phase 1 is complete and Phase 2 is now in progress. The Qt-free engine already
contains the `SemanticVersion` seed slice and typed standard-library result and
error contracts. The database boundary now owns file-format rules, SQLite
connection/transaction behavior, six-version schema migration, `OpenDatabase`,
Qt-free class CRUD, validated teacher CRUD, the native-English/GS-team
directory services, validated class-information persistence, and schedule
reads/conflicts, schedule-builder workflows, schedule-import workflows,
class-transfer workflows, teacher-import workflows, the
speaking-evaluation overall-grade report rule, the schedule report model, and
the by-day/daily/per-class roster report model and roster template policy, the
sub-prep pagination policy,
academic calendar recurrence rules, calendar-event normalization, validation,
recurrence, and campus filtering, speaking-evaluation report metadata, output
policy, content assembly, AI prompt rules, template policy, batch-export policy,
PowerPoint
  job content, student filename policy, schedule-report print labels, and the
  sub-prep class-information report model, class/teacher naming,
  upcoming-birthday scheduling, class analytics, roster validation, Qt-free
  roster persistence, speaking-evaluation grid validation, report batch ZIP
  archive writing,
  shared document-output result semantics, and the sub-prep package-planning,
  document-model, and document-catalog policies,
with the retained Qt code still operational. The committed eleven-case
database-port corpus now passes the Qt-free engine read, migration, write,
reopen, and class-transfer import checks in all four WinUI lanes. The retained
Qt fixture verifier also proves temporary Qt-written → engine-read and
engine-written → Qt-read interoperability. The retained Qt roster repository
now routes its file-backed load, save, student-count, and atomic batch-save
operations through the engine service. The retained Qt class-information
repository now routes its six class-information and schedule operations through
the engine services, and the retained Qt class repository now routes its six
CRUD operations through the engine class service. The retained Qt Native
English teacher repository now routes its list and atomic directory-save
operations through the engine service, and the retained Qt GS Team repository
now routes its list and atomic directory-save operations through the engine
service as well. The retained Qt schedule-import repository now routes its
preview, read-only review validation, and apply workflows through the engine
service too, and the retained
Qt class-transfer repository now routes package build, preview, and import
through the engine service. The retained Qt calendar-event repository now
routes its eleven persistence operations through the Qt-free calendar-event
service, including UTF-8/date/time conversion and transactional batch writes.
The retained Qt intensive-slot-state repository now routes its file-backed
list and save operations through the Qt-free intensive-slot-state service as
well.
The retained Qt speaking-evaluation repository now routes its file-backed save,
load, and roster-score import operations through the Qt-free speaking-
evaluation persistence service as well. The retained Qt teacher-import
repository now routes its file-backed import workflow through the Qt-free
teacher-import service as well.
The retained Qt testing-class and testing-block repositories now route their
file-backed persistence, assignment, and cleanup operations through the
Qt-free testing-class and testing-block services as well.
The retained Qt campus-record repository now routes its file-backed CRUD
operations through the Qt-free campus-record service as well. The retained Qt
application-settings repository now routes its persistence and batch-rollback
operations through the Qt-free application-settings service, and the teacher-
and schedule-import services use that same engine settings boundary. The
retained Qt `DatabaseSession` now sends file-backed profile path preparation and
schema migration through engine `OpenDatabase`; exact `:memory:` sessions keep
the Qt schema compatibility path, while the retained Qt connection still
enables foreign-key enforcement. The Qt-free personal-details settings service
now owns personal-details defaults, legacy-key promotion, signature settings,
and transactional writes; the retained Qt personal-details repository routes
file-backed reads and writes through that boundary while keeping Qt image
preparation and presentation conversions at the edge. The retained Qt
`CalendarEventCache` now also preflights file-backed paths through engine
`OpenDatabase` before opening its worker `QSQLITE` connection, preserving the
Qt cache/model boundary while sharing path and schema preparation. The
database-port fixture verifier now also writes representative calendar-event,
roster, speaking-evaluation, and campus records through engine services and
checks them through retained Qt SQLite. The P2-03 retained-adapter audit now
confirms that all retained repositories delegate persistence operations through
engine services, `DatabaseSession` preflights file-backed profiles through
`OpenDatabase`, and production `ApplicationServices` wiring uses session-only
feature services. The complete retained lifecycle suite passes; broader
per-slice fixture evidence remains the active Phase 2 work.
The retained Qt teacher and class-information validators now route their
normalization and validation through the Qt-free engine validators as well.
The standalone retained Qt `ClassTimeValidator` now delegates to the same
engine contract while restoring its Qt row/column and diagnostic-argument
surface. The retained Qt `ClassInfoConfig` catalog now delegates its public
`QStringList` catalogs and lookup fallbacks to the Qt-free engine configuration
as well.
The retained Qt class-tab navigation model now delegates grouping, ordering,
schedule-label, duplicate-label, and day-filter rules to the Qt-free engine
service while retaining its Qt-facing model and localization boundary.
The retained Qt evaluation-default selection helpers now delegate term naming,
population detection, current/previous-term fallback, and grade classification
to the Qt-free engine policy while retaining calendar, settings, and localized
Qt orchestration at the boundary. The focused native test passed in x64/x86
Debug and Release WinUI lanes.
The Qt 6.12 formal deprecation gate, calendar-event import extraction,
schedule-import review validation, and schedule-time formatter adapter
extraction are now complete. The two retained Qt schedule-import apply
fixtures now release their verification SELECT cursors before a second
engine-backed apply; production apply semantics remain unchanged.
The retained Qt PowerPoint job-model adapter now consumes a typed
`Result<BatchJob>` from the extracted engine-backed job service, translating
engine validation failures before renderer work while retaining normalization,
measurement, resource mapping, and automation JSON at the Qt boundary. The
speaking-evaluation batch path now converts edited report data once to the
engine report-content model; both PDF and PowerPoint adapters consume that
typed handoff, with Qt retaining drawing, resource mapping, JSON, automation,
and filesystem commits. The focused Qt x64 Debug batch-report CTest passed
1/1, and the existing Windows x64 Release engine PowerPoint-job test passed.
The new `StandardFileSystem` contract now owns UTF-8 path normalization,
byte-file reads and writes, directory creation, temporary staging, atomic file
and directory replacement, copy, cleanup, and typed filesystem errors.
`DataService` save/export, speaking-report output commits, ZIP finalization,
and Sub Prep staging now use that boundary; Qt retains path composition,
localization, rendering, printing, resource mapping, and Office automation.
The focused filesystem, ZIP, DataService save/export, speaking-report, and
Sub Prep tests passed on Windows x64 Debug. The next active gates are the
retained-adapter audit and broader per-slice fixture evidence. The latest
fixture slice covers engine-written calendar-event, roster,
speaking-evaluation, and campus values when reopened through the retained Qt
connection.

1. Extend the typed engine error and validation contracts where remaining
   domain and import slices need domain-specific diagnostics.
2. Extract the remaining domain models and validators, keeping rules in the
   Qt-free engine rather than in presentation adapters.
3. Migrate import, scheduling, calendar, and report-model use cases behind
   engine boundaries that both presentation stacks can consume.
4. Add cross-platform fixture round trips for each migrated persistence slice,
   including Windows-written and Qt-written databases.
5. Keep the retained Qt adapters and Windows, Linux, and macOS Qt suites
   green while each slice moves.

The former provisional Win32 shell and Direct2D SDK capability test were
removed after equivalent WinUI build, manifest, deployment, and smoke-test
coverage was established. Historical evidence remains under
[`docs/porting/windows-direct2d`](../../docs/porting/windows-direct2d/README.md).

## Completed Evidence Carried Forward

- Phase 0 was accepted on 2026-08-29.
- The Windows Qt visual harness covers 80 registered capture rows, including
  owner-reviewed 100%, 150%, and 200% DPI evidence and representative editors
  and dialogs.
- The cross-platform fixture corpus contains eleven SHA-pinned database cases.
- Retained Windows, Linux, and macOS Qt validation passed at the Phase 0/1
  handoff recorded in the historical status file.
- A Qt-free `ClassMngrEngine` target and ordinary engine test executable exist.
- The former provisional native-shell evidence is retained only as historical
  material under `docs/porting/windows-direct2d`; active Windows bootstrap
  validation is now owned by the WinUI lane.
- The Qt-free engine contains the `SemanticVersion` seed slice, typed result /
  error contracts, the UTF-8 database file-format contract, and the SQLite C
  API foundation. The entry-slice test result is recorded in
  [`phase2-local-validation.md`](../../docs/porting/windows-winui/phase2-local-validation.md).

Historical evidence remains under
[`docs/porting/windows-direct2d`](../../docs/porting/windows-direct2d/README.md).
The directory name records the original approach; it does not define the new
presentation architecture.

The current WinUI bootstrap evidence and pinned inputs are recorded under
[`docs/porting/windows-winui`](../../docs/porting/windows-winui/README.md).

## Progress Update Rule

After meaningful work:

1. Update the applicable dashboard row in this file.
2. Add concise dated evidence below `Progress Log` with the tested revision,
   platform, and result.
3. Update `Current Focus` to the next incomplete gate.
4. Change a phase to **Complete** only when its phase-file exit gate passes.
5. Keep detailed commands, measurements, screenshots, and failure analysis in
   test artifacts or evidence documents; link them here instead of copying
   long logs.

## Progress Log

- **2026-09-03 — P2-03 retained Qt database/application adapters completed.**
  The retained repository audit found all sixteen repository implementations
  delegating persistence through engine services with no direct Qt-SQL write
  paths. `DatabaseSession` now documents the engine-owned file-backed
  preflight/migration boundary and the explicit `:memory:` compatibility path;
  production `ApplicationServices` wiring passes session-only feature
  services, while legacy `DataService*` fallbacks remain documented for
  migration callers and tests. Lifecycle fixtures and the retained not-found
  error translation were corrected, and
  `ClassMngrDataServiceLifecycleTests` passed 15/15 through CTest on Windows
  x64 Debug. `git diff --check` passed. Broader per-slice fixture evidence
  remains open.
- **2026-09-03 — P2-02 portable file/output contracts completed.** The
  Qt-free `StandardFileSystem` now defines UTF-8 path normalization, byte
  reads/writes, directory creation, temporary directories, atomic file and
  directory replacement, copy, existence, and cleanup with typed error
  tokens. `DataService` save/export, speaking-report output commits, ZIP
  finalization, and Sub Prep staging now use the shared boundary. The
  filesystem, ZIP, DataService save/export, speaking-report, and Sub Prep
  focused tests built and passed on Windows x64 Debug; `git diff --check`
  passed. P2-03 retained-adapter cleanup and broader per-slice fixture
  evidence remain open.
- **2026-09-02 — P2-01 report/export adapter boundary completed.** The
  retained speaking-evaluation batch path now converts edited Qt report data
  once into the engine `SpeakingEvaluationReportContent` model. PDF and
  PowerPoint adapters consume that typed handoff while Qt retains rendering,
  resource mapping, JSON transport, Office automation, progress, and output
  commits. Existing engine-backed schedule, roster, and sub-prep report
  adapters complete the remaining P2-01 slices. The focused
  `ClassMngrSpeakingEvalBatchReportServiceTests` executable built and passed
  1/1 offscreen on Windows x64 Debug; the P2-01 diff passed `git diff --check`.
  P2-02 portable file/output contracts, P2-03 retained-adapter cleanup, and
  broader per-slice fixture evidence remain open.
- **2026-08-29 — Direction changed to WinUI 3.** The accepted Phase 0 evidence
  and completed generic build-boundary work are retained. Phase 1 remains in
  progress for hosted WinUI execution, owner interaction review, and memory
  evidence.
- **2026-08-29 — x86 build support added.** Phase 1 now requires x64 and x86
  Debug/Release builds, tests, and self-contained staging. x64 remains the
  release gate; publishing an x86 installer remains a separate decision.
- **2026-08-29 — WinUI bootstrap lane added.** A pinned Windows App SDK /
  C++/WinRT MSBuild project now contains the real XAML shell, calls the
  Qt-free engine, and has architecture-specific self-contained staging,
  smoke verification, and x64/x86 CI presets. Runner and owner-reviewed
  interaction evidence remain open.
- **2026-08-29 — Windows toolchain retargeted.** All active Windows CMake
  presets and workflows now target the `Visual Studio 18 2026` generator and
  `v145` toolset, using the dedicated `windows-2025-vs2026` CI image. Linux and
  macOS presets remain unchanged.
- **2026-08-29 — VS 2026 preflight hardened.** WinUI orchestration and stage
  verification now resolve only a VS 18 installation, require the selected
  x64/Win32 `v145` toolset, and reject mismatched MSBuild or `dumpbin` paths.
- **2026-08-29 — Runner toolchain preflight added.** The WinUI CI lane now
  verifies the VS 18 installation, x64/Win32 `v145` toolsets, MSBuild, and the
  pinned Windows SDK before configuring CMake.
- **2026-08-29 — Local VS 2026 matrix validated (eaeb62f plus working tree).**
  On the installed Visual Studio 2026 Community toolchain, x64 and x86
  Debug/Release WinUI stages built with `v145`; engine and staged WinUI CTest
  gates passed for all four configurations. The Win32 Release memory helper
  recorded 68.30 MiB startup, 71.00 MiB steady-state peak, and 71.00 MiB
  process peak against the 200 MiB steady-state target. GitHub runner execution,
  DPI/keyboard-focus/Korean-IME owner review, and representative peak-budget
  evidence remain open. Detailed results are in
  [`phase1-local-validation.md`](../../docs/porting/windows-winui/phase1-local-validation.md).
- **2026-08-29 — All WinUI stages re-verified locally.** The staged x64 and
  Win32/x86 Debug and Release applications each passed the manifest,
  architecture, self-contained-runtime, Qt-absence, engine, input, theme, and
  DPI checks. Hosted runner execution and owner-reviewed interactive evidence
  remain open.
- **2026-08-29 — Provisional native lane retired.** The obsolete Win32 shell,
  Direct2D SDK capability gate, and native-only presets/workflow were removed
  after the WinUI lane established equivalent local build, staging, manifest,
  and smoke coverage. Its resource verifier was replaced by a generic
  resource-manifest CTest used by WinUI; the shared catalog manifest remains
  because WinUI consumes it.
- **2026-08-29 — Retained Qt regression rerun after cleanup.** The current
  Windows Qt Debug route passed all 68 non-visual tests after the provisional
  native lane was removed. The explicit Phase 0 visual lane passed 68 of 69;
  its only failure was the required 100/150/200% capture gate on this host's
  125% display scale. The startup fixture expectation was corrected from 12 to
  16 `app_settings` rows to match the committed fixture, and startup
  performance then passed.
- **2026-08-29 — Hosted memory capture wired into the WinUI lane.** The
  Windows runner now records and uploads an x86 Release idle-memory report
  after stage verification. A representative import/report/PDF/large-data
  peak workload remains intentionally open because the bootstrap has no such
  feature slice yet.
- **2026-08-29 — Hosted Windows bootstrap gate passed.** GitHub Actions run #2
  on `windows-2025-vs2026` passed the VS 2026/v145 preflight, configure, build,
  Qt-free cache, CTest, self-contained-stage, and upload steps for x64 and x86
  Debug and Release. The x86 Release idle-memory report was uploaded as a
  separate artifact. The retained Linux x64 Release and macOS universal runs
  also passed their build, package, and upload checks.
- **2026-08-29 — WinUI owner review passed.** The owner reported all required
  light/dark, 100%/150%/200% DPI, keyboard-focus, and Korean-IME checks passed
  against the x64 Release stage. The x64 stage verifier also passed for
  `dist/ClassMngr-windows-winui-x64/Release`.
- **2026-08-29 — x86 idle-memory capture refreshed.** The Win32 Release stage
  recorded 67.52 MiB startup working set and 68.14 MiB steady-state/process
  peak, within the shared 200 MiB steady-state target. This remains an idle
  baseline; representative feature-workload peak evidence is still open.
- **2026-08-29 — Retained Windows Qt validation refreshed.** The startup
  controller still reapplies the loaded font-size setting when its actions are
  connected, and the retained Windows Qt Release product previously rebuilt
  successfully with its deployed runtime. The current fresh Debug/visual
  result is recorded above: 68/68 non-visual tests pass, with only the
  125%-scale visual-capture gate open. Detailed matrix and environment notes
  are in
  [`phase1-local-validation.md`](../../docs/porting/windows-winui/phase1-local-validation.md).
- **2026-08-30 — Retained Windows Qt visual capture rerun at 100%.** After the
  host display scale was manually set to 100%, the VS 2026/v145 Debug visual
  target passed its full CTest run in 64.55 seconds. It produced 56 metadata
  sidecars under
  `artifacts/phase0/windows-qt-visual/20260829T150959409Z-25956/`, and the
  repository validator accepted all 56. Owner review of the PNGs is good;
  representative x86 peak-memory evidence remains open.
- **2026-08-30 — Retained Windows Qt 150% capture attempted.** The full run
  and one retry ended with native `qWaitForWindowExposed` failures (57/2 and
  55/4 passed/failed), not scale assertion failures. The latest partial set
  has 52 valid 150% sidecars under
  `artifacts/phase0/windows-qt-visual/20260829T151739696Z-4248/`; it is
  diagnostic for automation and is not counted as a green automated test
  gate; the owner visual-gate acceptance is recorded below.
- **2026-08-30 — Retained Windows Qt visual capture completed at 200%.** The
  full VS 2026/v145 Debug visual target passed in 75.27 seconds and produced
  56 valid 200% sidecars under
  `artifacts/phase0/windows-qt-visual/20260829T152306133Z-24800/`. Owner
  review reports the complete 100% and 200% PNG sets and the available 150%
  PNGs appear good. The owner accepts the 150% visual gate despite its
  documented native-exposure automation exception. Representative x86
  peak-memory evidence is intentionally deferred until a realistic feature
  slice exists and before any x86 release-support decision.
- **2026-08-30 — Representative peak-memory evidence deferred.** The bootstrap
  has no realistic import, reporting, PDF, or large-data workload, so the
  representative x86 peak measurement and budget decision are deferred to the
  first applicable feature slice. The existing idle baseline is retained, and
  Phase 1 is complete without making an x86 release peak-budget claim.
- **2026-08-30 — Phase 2 started.** The database file-format rules were
  extracted into the Qt-free engine as a UTF-8 standard-library contract. The
  retained Qt implementation is now an adapter, and dedicated x64 Debug and
  x86 Release headless engine tests pass. Portable SQLite/session/schema and
  use-case extraction remain; Phase 2 is not complete. Detailed evidence is
  in [`phase2-local-validation.md`](../../docs/porting/windows-winui/phase2-local-validation.md).
- **2026-08-30 — Portable database foundation added.** `ClassMngrEngine` now
  exposes typed errors, an opaque SQLite database boundary, prepared UTF-8
  parameter binding, typed row mapping, a five-second busy timeout, foreign
  key setup, schema-version primitives, and RAII transactions. Windows uses
  the architecture-correct Windows SDK `winsqlite3` library; non-Windows
  configuration uses CMake's SQLite3 target. All four local Windows engine
  lanes pass the new SQLite test. Schema migration and `OpenDatabase` remain
  open, so Phase 2 is not complete.
- **2026-08-30 — Phase 2 foundation integrated validation.** The x64 Debug
  CTest sweep passed all three headless engine suites and both WinUI staging /
  manifest checks. The x64 Debug and x86 Release staged WinUI targets rebuilt
  with the architecture-matched SQLite dependency, and the narrowed engine
  source audit found no Qt, WinUI, WinRT, Direct2D/DirectWrite, or Win32 UI
  dependency.
- **2026-08-30 — Schema, OpenDatabase, and first CRUD slice added.** The
  engine now owns the existing six-version schema/migration sequence, including
  preflight rejection, atomic rollback, and file-backed migration backup. The
  `OpenDatabase` use case opens and migrates a UTF-8 path, and the first
  Qt-free `Classroom`/`ClassRepository` slice covers representative CRUD. All
  four local Windows engine lanes pass the schema and class-repository tests;
  retained Qt schema-manager and updater tests remain green. Phase 2 remains
  open for broader model, validator, use-case, import, and cross-platform
  fixture extraction.
- **2026-08-30 — Teacher model and validated use case added.** The engine now
  owns the standard-library `Teacher` model, Qt-free validation result,
  English/Korean name normalization, phone/birthday/enum rules, and a
  parameterized `TeacherService` CRUD boundary with transactional assignment
  cleanup. All four Windows lanes passed the six-suite headless engine sweep
  plus WinUI staging and manifest checks; retained Qt schema-manager and
  updater tests also passed. Phase 2 remains open for broader models, imports,
  reports, adapters, and cross-platform fixture round trips.
- **2026-08-30 — Teacher directory services added.** The engine now owns
  native-English-teacher and GS-team directory models and atomic save/list
  services, including the retained position ordering and independent
  case-insensitive name uniqueness rules. All four Windows lanes passed the
  eight-suite headless engine sweep plus WinUI staging and manifest checks;
  the retained Qt teacher-import regression also passed. Phase 2 remains open
  for broader models, imports, reports, adapters, and cross-platform fixture
  round trips.
- **2026-08-30 — Class information service added.** The engine now owns the
  Qt-free `ClassInfo`/`ClassTime` models, canonical class configuration,
  schedule/class-information validation, teacher-join reads, normalized
  class-information saves, note saves, and transactional regular/intensive
  time replacement. All four Windows lanes passed the nine-suite headless
  engine sweep plus WinUI staging and manifest checks; the retained Qt
  class-information lifecycle regression also passed. Phase 2 remains open
  for schedule conflict aggregates, imports, reports, adapters, and
  cross-platform fixture round trips.
- **2026-08-30 — Class schedule service added.** The engine now owns regular
  class teacher assignments, renderer-neutral schedule snapshots, testing
  class filtering, and candidate/stored conflict detection for regular and
  intensive intervals. All four Windows lanes passed the ten-suite headless
  engine sweep plus WinUI staging and manifest checks; the retained Qt
  assignment and lifecycle regressions also passed. Phase 2 remains open for
  schedule imports, reports, adapters, and cross-platform fixture round trips.
- **2026-08-30 — Class transfer service added.** The engine now owns the
  versioned class-package models, export reads, match previews, teacher/class
  create-replace-skip planning, schedule preflight, roster and
  speaking-evaluation child-table writes, and transactional import behavior.
  All four Windows lanes passed the eleven-suite headless engine sweep plus
  WinUI staging and manifest checks; the retained Qt class-transfer regression
  also passed. The Qt JSON/file codec remains an adapter, and Phase 2 remains
  open for schedule imports, report models, adapter wiring, and
  cross-platform fixture round trips.
- **2026-08-30 — Transfer regression matrix refreshed.** The four WinUI
  Debug/Release x64/x86 lanes each passed all 13 CTest entries, including the
  new class-transfer service and stage checks. The retained Windows Qt
  non-visual suite passed all 78 registered tests, including the existing
  class-transfer path. Phase 2 remains open for schedule-import/report
  models, adapter wiring, and cross-platform fixture round trips.
- **2026-08-30 — Schedule-import service added.** The engine now owns the
  workbook-neutral schedule-import models, teacher/class match ranking,
  meeting-pattern rules, plan and stale-target validation, normal snapshot
  replacement, intensive preserve/replace behavior, slot-state snapshots,
  profile-name policy, and transactional schedule writes. The retained
  workbook parser remains the Qt adapter. The new headless service test and
  the complete 14-test engine/WinUI staging sweep passed in all four WinUI
  Debug/Release x64/x86 lanes; the retained Qt schedule-import regression
  also passed. Phase 2 remains open for report models, adapter wiring, and
  cross-platform fixture round trips.
- **2026-08-30 — Speaking-evaluation report grade rule extracted.** The
  Qt-free `SpeakingEvaluationReportService` now owns the six-metric overall
  grade calculation, including the retained fractional rounding threshold and
  invalid-score `N/A` behavior. The retained Qt report assembler, report
  widget, and roster score-import path delegate to the engine. The new engine
  test passed in all four x64/x86 Debug/Release lanes; the integrated WinUI
  sweeps passed 15/15, and the retained report-widget test passed. Remaining
  Phase 2 work is report content/pagination models, adapter wiring, and
  cross-platform fixture round trips.
- **2026-08-30 — Schedule report model extracted.** The Qt-free
  `ScheduleReportService` now owns the renderer-neutral grid model, slot-state
  defaults and overrides, intensive trimming, testing assignments and
  suppression, summary counts, teacher-room labels, and 12/24-hour range
  formatting. The retained Qt schedule view model delegates through a
  conversion adapter; schedule/PDF and sub-prep report regressions passed.
  The integrated WinUI sweep passed all fourteen engine suites plus staging
  and manifest checks (16/16) in x64/x86 Debug and Release.
- **2026-08-30 — Roster report model extracted.** The Qt-free
  `RosterReportService` now owns the by-day, daily, and per-class cell-value
  models, time-slot mapping, duplicate-slot validation, daily overflow page
  keys, teacher/room and Zoom fallback labels, student limits, and
  per-class extra-column filtering/caps. The retained Qt roster service now
  converts into the portable model while keeping PDF geometry and drawing as
  adapters. The engine test passed in all four x64/x86 Debug/Release lanes,
  the retained Qt roster PDF regression passed, and the integrated WinUI
  sweep passed all fifteen engine suites plus staging and manifest checks
  (17/17).
- **2026-08-30 — Sub-prep pagination policy extracted.** The Qt-free
  `SubPrepPaginationService` now owns teacher-section page-span detection,
  the "Sub Notes" new-page threshold, and fallback last-page placement. The
  retained Qt renderer delegates those decisions while keeping text
  measurement and PDF drawing in the adapter. The engine test passed in all
  four x64/x86 Debug/Release lanes, the retained Qt sub-prep PDF regression
  passed, and the integrated WinUI sweep passed all sixteen engine suites plus
  staging and manifest checks (18/18).
- **2026-08-30 — Academic calendar rules extracted.** The Qt-free
  `AcademicCalendarSchedule` now owns standard-library date arithmetic,
  default term schedules, custom-year rollover, previous-fall continuity,
  term/week lookup, and schedule validation. The retained Qt schedule class
  delegates those rules while keeping settings JSON and locale formatting in
  the adapter. The engine test passed in all four x64/x86 Debug/Release lanes,
  the retained Qt calendar and evaluation-selection regressions passed, and
  the integrated WinUI sweep passed all seventeen engine suites plus staging
  and manifest checks (19/19).
- **2026-08-30 — Calendar-event rules extracted.** The Qt-free
  `CalendarEventRules` now owns exact event-type/time-status normalization,
  start-of-term recognition, and literal campus-code token matching. The
  retained Qt event model/filter delegate through UTF-8 adapters. The engine
  test passed in all four x64/x86 Debug/Release lanes, the retained calendar-
  import regression passed, and the integrated WinUI sweep passed all eighteen
  engine suites plus staging and manifest checks (20/20).
- **2026-08-30 — Speaking-evaluation report metadata extracted.** The Qt-free
  report model now owns elementary-grade parsing, class-label construction,
  advanced-template selection, and deterministic report-date formatting. The
  retained report dialog and AI grade helper delegate through the model. Its
  headless test passed in all four x64/x86 Debug/Release lanes, the retained
  Qt batch-report regression passed, and the integrated WinUI sweep passed all
  nineteen engine suites plus staging and manifest checks (21/21).
- **2026-08-30 — Speaking-evaluation output policy extracted.** The Qt-free
  output policy now owns schedule-aware destination-folder and batch-archive
  naming, including day/time labels and safe folder components. The retained
  Qt adapter supplies standard-path lookup, localized fallbacks, and Unicode-
  safe student filenames. Its headless test passed in all four x64/x86
  Debug/Release lanes, the retained Qt batch-report regression passed, and the
  integrated WinUI sweep passed all twenty engine suites plus staging and
  manifest checks (22/22).
- **2026-08-30 — Speaking-evaluation report content extracted.** The Qt-free
  content service now owns blank-student filtering, display-name normalization,
  source-row identity, student/class/teacher fields, score/comment transfer,
  and metadata composition. The retained Qt dialog only converts row values,
  supplies the current date/signature bytes, and creates widget types. Its
  headless test passed in all four x64/x86 Debug/Release lanes, the retained
  Qt batch-report regression passed, and the integrated WinUI sweep passed all
  twenty-one engine suites plus staging and manifest checks (23/23).
- **2026-08-30 — Speaking-evaluation AI prompt rules extracted.** The Qt-free
  AI prompt service now owns observation-line normalization, student/classmate
  name redaction, prompt eligibility and composition, and batch-response marker
  parsing. The retained Qt callers only convert values and expose engine
  results to the dialogs. Its headless test passed in all four x64/x86
  Debug/Release lanes, the retained Qt batch-report regression passed, and the
  integrated WinUI sweep passed all twenty-two engine suites plus staging and
  manifest checks (24/24).
- **2026-08-30 — Speaking-evaluation template policy extracted.** The Qt-free
  template policy now owns the Standard/Advanced enum, page and signature
  geometry, PowerPoint resource identifiers, score-table placement and shape,
  and neutral fill colors. The retained Qt template header and PowerPoint job
  model are conversion adapters for those values; assets, text measurement,
  JSON transport, and drawing remain presentation-owned. Its headless test
  passed in all four x64/x86 Debug/Release lanes, the retained offscreen
  batch-report regression passed, and the integrated WinUI sweep passed all
  twenty-three engine suites plus staging and manifest checks (25/25).
- **2026-08-30 — Speaking-evaluation batch-export policy extracted.** The
  Qt-free policy now owns report-count, output-mode, PDF-destination, and
  exact-file validation, PowerPoint template homogeneity, and archive versus
  individual-PDF decisions. The retained Qt batch service maps typed policy
  failures to localized messages while retaining filesystem, rendering,
  printing, and Office work. Its headless test and retained offscreen
  batch-report regression passed; all four integrated WinUI sweeps passed
  twenty-four engine suites plus staging and manifest checks (26/26).
- **2026-08-30 — Speaking-evaluation PowerPoint job content extracted.** The
  Qt-free job service now owns renderer-neutral field mapping, UTF-8 paths and
  signature bytes, overall-grade calculation, path-count validation, and
  homogeneous template validation. The retained Qt job model converts that
  content and keeps NFC normalization, comment measurement, resource paths,
  JSON, and Office arguments at the adapter edge. Its headless test and
  retained offscreen batch-report regression passed; all four integrated
  WinUI sweeps passed twenty-five engine suites plus staging and manifest
  checks (27/27).
- **2026-08-30 — Speaking-evaluation student filename policy extracted.** The
  portable output policy now owns student PDF name composition, reserved-name
  protection, unsafe-character replacement, suffix normalization, fallback
  naming, and the UTF-8 length limit. Qt keeps native-text normalization,
  directory creation, collision checks, and atomic commits. The existing
  output-policy test and retained offscreen batch-report regression passed;
  all four WinUI sweeps remained green at 27/27.
- **2026-08-30 — Database fixture round-trip gate added.** The new Qt-free
  `ClassMngrEngineDatabaseFixtureRoundTripTests` opens all eleven committed
  current, legacy, and failure fixtures, verifies bilingual class-transfer
  payloads plus calendar/campus data, checks migration backups and rollback,
  and performs engine write/reopen and class-transfer import round trips. The
  target passed in x64/x86 Debug/Release, and the integrated WinUI sweeps
  passed all twenty-six engine suites plus staging and manifest checks (28/28).
  Remaining Phase 2 work is the rest of the report/export adapter migration
  and explicit two-direction cross-platform fixture writes.
- **2026-08-31 — Schedule-report print labels extracted.** The Qt-free
  `ScheduleReportService` now owns class-line formatting, bilingual Excel day
  labels, and Excel time-range compaction. The retained Qt adapters delegate
  those deterministic labels while keeping fonts, colors, page geometry, and
  drawing. The headless schedule-report test passed in all four x64/x86
  Debug/Release lanes, the retained schedule model and PDF regressions passed,
  and the integrated WinUI sweeps remained green at 28/28. Remaining Phase 2
  work is the rest of the report/export adapter migration and broader
  per-slice cross-platform fixture coverage.
- **2026-08-31 — Two-direction database fixture evidence added.** The
  retained Qt fixture verifier now creates a temporary Qt-written profile and
  opens it through `OpenDatabase`, then creates a temporary engine-written
  profile and opens it through Qt `QSQLITE` queries. The checks cover schema v6
  and bilingual teacher/class values without modifying the committed corpus;
  the interoperability regression passed. Remaining Phase 2 work is the rest
  of the report/export adapter migration and broader per-slice fixture
  coverage.
- **2026-08-31 — Sub-prep class-information report model extracted.** The
  Qt-free `SubPrepClassInformationService` now owns visible-class filtering,
  meeting-time compaction, teacher grouping, grade/level/time ordering, and
  renderer-neutral class details. The retained Qt model is a conversion
  adapter, and the new headless service test passed in all four x64/x86
  Debug/Release WinUI lanes; the integrated WinUI sweeps remained green at
  29/29. The next Phase 2 gate remains the remaining report/export
  adapter/model migration and broader per-slice fixture coverage.
- **2026-08-31 — Schedule-builder rules extracted.** The Qt-free
  `ScheduleBuilderService` now owns schedule-time parsing, visible-day
  filtering, regular/intensive range selection, `:05`/`:55` offset handling,
  and renderer-neutral entry assembly. The retained Qt builder is now a data
  access and conversion adapter. Its native test passed in all four x64/x86
  Debug/Release lanes, the retained Qt schedule-builder regression passed,
  and the integrated WinUI sweeps passed all twenty-eight engine suites plus
  staging and manifest checks (30/30). Phase 2 remains open for the remaining
  report/export adapter/model migration and broader per-slice fixture coverage.
- **2026-08-31 — Class naming and Sub Prep package planning extracted.** The
  Qt-free `ClassNamingService` now owns stable class/teacher labels and
  ordering, and `SubPrepPackageService` owns date/day selection, regular versus
  intensive class filtering, deterministic folder/document planning, and safe
  unique class-folder names. The retained Qt naming and package files are
  conversion/rendering/filesystem adapters. Both new headless tests passed in
  all four x64/x86 Debug/Release lanes; retained package, class-page, and
  sub-prep regressions passed, and the integrated WinUI sweeps passed all
  thirty engine suites plus staging and manifest checks (32/32). Phase 2
  remains open for the remaining report/export adapter/model migration and
  broader per-slice fixture coverage.
- **2026-08-31 — Upcoming-birthday scheduling extracted.** The Qt-free
  `UpcomingBirthdaySchedule` now owns birthday parsing, today/weekly windows,
  year rollover, non-leap-year February 29 handling, staff grouping, fallback
  display names, and deterministic ordering. The retained Qt schedule is now
  a `QDate`/UTF-8 conversion adapter while the dialog remains presentation-
  owned. Its native test passed in all four x64/x86 Debug/Release lanes, the
  retained Qt birthday regression passed, and the integrated WinUI sweeps
  passed all thirty-one engine suites plus staging and manifest checks (33/33).
  Phase 2 remains open for the remaining report/export adapter/model migration
  and broader per-slice fixture coverage.
- **2026-08-31 — Class analytics extracted.** The Qt-free
  `SpeakingAnalyticsService` now owns evaluation-name discovery, grade
  conversion and rounding, consolidated criterion distributions, class-shape
  summaries, roster filtering, student rankings, and year-to-date points. The
  portable student-name service provides the shared English/Korean matching
  rules, while the retained Qt analytics service and utility are conversion
  adapters. Its native test passed in all four x64/x86 Debug/Release lanes;
  the retained Qt adapter and student-name utility passed compile-only checks
  against Qt 6.11/MSVC, and the complete integrated WinUI sweeps passed all
  thirty-two engine suites plus staging and manifest checks (34/34). Phase 2
  remains open for the remaining report/export adapter/model migration and
  broader per-slice fixture coverage.
- **2026-08-31 — Roster validation extracted.** The Qt-free `Roster` and
  `RosterValidator` boundaries now own canonical column normalization,
  UTF-8 cell cleanup, row/column limits, required student names,
  English/Korean validation, duplicate-pair detection, and typed row/column
  diagnostics. The retained Qt validator is a conversion adapter; table
  editing and presentation remain Qt-owned. Its headless test passed in all
  four x64/x86 Debug/Release lanes, the retained Qt adapter passed a Qt
  6.11/MSVC compile-only check, and the integrated WinUI sweeps passed all
  thirty-three engine suites plus staging and manifest checks (35/35). Phase
  2 remains open for the remaining report/export adapter/model migration and
  broader per-slice fixture coverage.
- **2026-08-31 — Roster report template policy extracted.** The Qt-free
  `RosterReportTemplateService` now owns stable template ordering, report
  orientation mapping, and all/current/selected class-id resolution. The
  retained Qt roster-print service delegates those decisions while keeping
  localized titles, page-size selection, PDF rendering, and drawing in the
  adapter. Its headless test passed in all four x64/x86 Debug/Release lanes,
  the retained x64 Qt roster-print regression passed, and the integrated
  WinUI sweeps passed all thirty-four engine suites plus staging and manifest
  checks (36/36). Phase 2 remains open for the remaining report/export
  adapter/model migration and broader per-slice fixture coverage.
- **2026-08-31 — Sub-prep document model extracted.** The Qt-free
  `SubPrepDocumentService` now owns the renderer-neutral document aggregate
  for campus/Zoom information, instructional text, schedule reports, and
  nested teacher/class details. The retained Qt document model converts the
  complete value graph through UTF-8 while keeping PDF rendering and drawing
  presentation-owned. Its headless test passed in all four x64/x86
  Debug/Release lanes, the retained Qt Sub Prep PDF regression passed, and
  the integrated WinUI sweeps passed all thirty-five engine suites plus
  staging and manifest checks (37/37). Phase 2 remains open for the remaining
  report/export adapter/model migration and broader per-slice fixture
  coverage.
- **2026-08-31 — Document-catalog policy extracted.** The Qt-free
  `DocumentCatalogService` now owns locale fallback, UTF-8-safe identifier and
  relative-path/file-name/order validation, parent-path derivation,
  duplicate/reachability filtering, and renderer-neutral catalog model
  construction. The retained Qt parser keeps JSON shape/type checks,
  filesystem and resource-root existence checks, absolute-path reconstruction,
  active/embedded-root fallback, and localized diagnostics at the adapter
  edge. Its headless test passed in all four x64/x86 Debug/Release lanes, the
  retained Qt document-catalog regression passed, and the integrated WinUI
  sweeps passed all thirty-six engine suites plus staging and manifest checks
  (38/38). Phase 2 remains open for the remaining report/export adapter/model
  migration and broader per-slice fixture coverage.

- **2026-08-31 — Speaking-evaluation grid validation extracted.** The
  Qt-free `SpeakingEvaluationValidator` now owns score aliases,
  renderer-neutral row normalization, structural limits, student-name,
  score, comment/note, and duplicate-pair diagnostics, including the
  questionable Korean-length warning policy. The retained Qt validator is
  now a UTF-8 adapter with its public API unchanged. Its focused native test
  passed in all four x64/x86 Debug/Release lanes; the retained Qt
  `ClassMngrSharedPolicyTests` regression passed through the adapter; and the
  integrated WinUI sweeps passed all thirty-seven engine suites plus staging
  and manifest checks (39/39). Phase 2 remains open for the remaining
  report/export adapter/model migration and broader per-slice fixture
  coverage.
- **2026-08-31 — Report batch ZIP writer extracted.** The Qt-free
  `ZipArchiveWriter` now owns stored standard-ZIP construction, UTF-8 entry
  names, CRC-32, DOS timestamps, size/count/name validation, and atomic
  temporary-output replacement. The retained Qt helper keeps its public API
  and localized diagnostics while delegating archive construction to the
  engine. Its focused native test passed in all four x64/x86 Debug/Release
  lanes; the retained Qt facade compiled under Qt 6.11/MSVC; and the
  integrated WinUI sweeps passed all
  thirty-eight engine suites plus staging and manifest checks (40/40). Phase
  2 remains open for the remaining report/export adapter/model migration and
  broader per-slice fixture coverage.
- **2026-08-31 — Shared document-output result contract extracted.** The
  Qt-free `classmngr::engine::DocumentOutputResult` now owns output-status
  semantics and UTF-8 message, PDF-path, and archive-path values. The retained
  Qt result model preserves its public shape and converts at the boundary. The
  focused engine test passed in all four x64/x86 Debug/Release WinUI lanes;
  the Qt 6.11/MSVC compatibility header passed a compile-only round-trip
  check. Phase 2 remains open for the remaining report/export adapter/model
  migration and broader per-slice fixture coverage.
- **2026-08-31 — Calendar-event validation extracted.** The Qt-free
  `CalendarEvent` and `CalendarEventValidator` now own string normalization,
  UTF-8 length checks, date/time consistency, recurrence bounds, month-end
  recurrence stepping, and repeat-series caps. The retained Qt validator is a
  UTF-8/date/time conversion adapter with its public API unchanged. The
  focused engine test passed in all four x64/x86 Debug/Release WinUI lanes;
  the retained adapter passed a Qt 6.11/MSVC compile-only check. Phase 2
  remains open for the remaining report/export adapter/model migration and
  broader per-slice fixture coverage.
- **2026-08-31 — Roster persistence extracted.** The Qt-free `RosterService`
  now owns class-scoped roster-column and sparse-cell loading and saving with
  UTF-8 preservation, width fallback, replacement semantics, malformed-cell
  filtering, and transactional rollback. The retained Qt repository now
  converts through UTF-8 and delegates its file-backed load, save, student-
  count, and batch-save operations to the engine; the engine batch operation
  preserves all-or-nothing writes. The focused engine and retained Qt
  lifecycle/class-transfer regressions passed, and all four x64/x86
  Debug/Release engine CTest sweeps passed. Phase 2 remains open for the
  remaining report/export adapter/model migration and broader per-slice
  fixture coverage.
- **2026-09-01 — Class-information adapter connected.** The retained Qt
  `ClassInfoRepository` now converts through UTF-8 and delegates its six
  class-information, schedule-read, and conflict operations to the Qt-free
  `ClassInfoService` and `ClassScheduleService`; engine save and notes failures
  retain operation and class-id context for the existing rollback diagnostics.
  A rebuilt focused retained-Qt assignment test passed 8/8 cases, and direct
  VS 2026/Qt 6.11 compile checks passed for the adapter and engine changes.
  The regular Qt CMake rebuild remains blocked by the existing regeneration /
  MSBuild FileTracker issue. Phase 2 remains open for the remaining
  report/export adapter/model migration and broader per-slice fixture coverage.
- **2026-09-01 — Teacher adapter connected.** The retained Qt
  `TeacherRepository` now converts through UTF-8 and delegates all six teacher
  CRUD operations to the Qt-free `TeacherService`, including engine-owned
  validation, canonical phone formatting, and transactional class-assignment
  cleanup on delete. The lifecycle fixture now uses valid representative
  teacher values and verifies the engine's canonical phone result. The adapter
  passed a direct VS 2026/Qt 6.11 compile-only check, and the complete Qt-free
  engine/WinUI CTest sweep passed 43/43. The regular Qt CMake rebuild remains
  blocked by the existing regeneration / MSBuild FileTracker issue. Phase 2
  remains open for the remaining report/export adapter/model migration and
  broader per-slice fixture coverage.
- **2026-09-01 — Class adapter connected.** The retained Qt
  `ClassRepository` now converts through UTF-8 and delegates its six CRUD
  operations to the Qt-free `ClassRepository`; class deletion retains the
  existing child-cleanup order, contextual diagnostics, and transaction
  rollback. The class-assignment fixture now uses a temporary file so Qt and
  engine connections exercise the same profile. The adapter, Data project,
  and focused test source passed direct VS 2026/Qt 6.11 compile checks, and
  the focused engine class-repository test passed in all four x64/x86
  Debug/Release WinUI lanes after rebuilding.
  The regular Qt CMake rebuild remains blocked by the existing regeneration /
  MSBuild FileTracker issue. Phase 2 remains open for the remaining retained
  adapters, report/export adapter/model migration, and broader per-slice
  fixture coverage.

- **2026-09-01 — Native English teacher adapter connected.** The retained Qt
  `NativeEnglishTeacherRepository` now converts through UTF-8 and delegates
  list and atomic directory-save operations to the Qt-free
  `NativeEnglishTeacherService`, including engine-owned ordering,
  normalization, uniqueness, and transaction behavior. The adapter compiled
  as part of the real VS 2026/Qt 6.11 Data project. The regular Qt CMake
  rebuild remains blocked by the existing regeneration / MSBuild FileTracker
  issue. Phase 2 remains open for the remaining retained adapters,
  report/export adapter/model migration, and broader per-slice fixture
  coverage.
- **2026-09-01 — GS Team adapter connected.** The retained Qt
  `GsTeamRepository` now converts through UTF-8 and delegates list and atomic
  directory-save operations to the Qt-free `GsTeamService`, including
  engine-owned ordering, normalization, uniqueness, and transaction behavior.
  The four existing x64/x86 Debug/Release GS Team engine tests passed, and
  static adapter checks passed. The regular Qt CMake rebuild remains blocked
  by the existing regeneration / MSBuild FileTracker issue, so no current-
  binary retained-Qt directory regression is claimed for this slice. Phase 2
  remains open for the remaining retained adapters, report/export
  adapter/model migration, and broader per-slice fixture coverage.
- **2026-09-01 — Schedule-import adapter connected.** The retained Qt
  `ScheduleImportRepository` now converts the complete nested import model
  graph through UTF-8 and delegates preview and transactional apply workflows
  to the Qt-free `ScheduleImportService`, including engine-owned matching,
  plan validation, intensive-mode handling, and rollback. The four existing
  x64/x86 Debug/Release schedule-import engine tests passed, and static
  adapter checks passed. The regular Qt CMake rebuild remains blocked by the
  existing regeneration / MSBuild FileTracker issue, so no current-binary
  retained-Qt schedule-import regression is claimed for this slice. Phase 2
  remains open for the remaining retained adapters, report/export
  adapter/model migration, and broader per-slice fixture coverage.

- **2026-09-01 — Class-transfer adapter connected.** The retained Qt
  `ClassTransferRepository` now converts the complete nested package and plan
  model graph through UTF-8 and delegates package build, preview, and
  transactional import to the Qt-free `ClassTransferService`; matching,
  schedule preflight, and SQL writes are no longer duplicated in the adapter.
  A direct Qt 6.11/MSVC compile-only check and a link-level Qt/engine smoke
  harness passed, including UTF-8 values and an unset export timestamp. The
  x64/x86 Debug/Release class-transfer engine tests passed. The normal Qt CMake
  regeneration remains blocked by the existing MSBuild FileTracker access and
  regeneration stall, so no current-binary retained-Qt regression is claimed
  for this slice. Phase 2 remains open for the remaining retained adapters,
  report/export adapter/model migration, and broader per-slice fixture
  coverage.

- **2026-09-01 — Calendar-event persistence adapter connected.** The
  Qt-free `CalendarEventService` now owns calendar-event CRUD, date/range/
  upcoming/repeat-series queries, ISO/HH:mm row mapping, validation, and
  transactional batch writes. The retained Qt `CalendarEventRepository`
  delegates all eleven operations through explicit UTF-8/date/time adapters;
  file-backed fixtures keep the Qt and engine SQLite connections on the same
  profile. The x64 Release engine calendar-event selection passed 3/3, the
  retained Qt repository passed 11/11, and the calendar cache regression
  passed. The full lifecycle target still reports two unrelated existing
  fixture assertions, so it is not counted as a clean full-suite gate.
  Phase 2 remains open for the remaining retained adapters, report/export
  adapter/model migration, and broader per-slice fixture coverage.
- **2026-09-01 — Intensive-slot-state persistence adapter connected.** The
  Qt-free `IntensiveSlotStateService` now owns ordered UTF-8 state reads,
  default-state deletion, and upsert persistence with typed SQLite errors. The
  retained Qt `IntensiveSlotStateRepository` converts through UTF-8 and
  delegates through a cached file-backed engine connection; its updated
  regression uses a temporary profile because Qt and engine connections cannot
  share `:memory:` storage. The focused engine test passed in all four x64/x86
  Debug/Release WinUI lanes. The current retained Qt objects also passed a
  manual link-level offscreen run with 5/5 cases after direct Qt 6.11/MSVC
  compilation. The normal Qt CMake regeneration remains blocked by the
  existing stale/missing generated project and FileTracker stall, so no
  CMake-generated current-binary Qt gate is claimed. Phase 2 remains open for
  the remaining retained adapters, report/export adapter/model migration, and
  broader per-slice fixture coverage.
- **2026-09-01 — Speaking-evaluation persistence adapter connected.** The
  Qt-free `SpeakingEvaluationPersistenceService` now owns evaluation lookup and
  creation, the fixed 25x11 grid persistence, dirty-cell/full-save semantics,
  typed schema and transaction failures, and roster-score import assembly. The
  retained Qt `SpeakingEvalRepository` now converts through UTF-8 and delegates
  its save, load, and import operations through a cached file-backed engine
  connection. The focused persistence test passed in all four x64/x86
  Debug/Release WinUI lanes, and a temporary Qt 6.11/MSVC link-level adapter
  smoke harness passed file-backed save/load, UTF-8 values, dirty-cell update,
  and roster-score import. The regular Qt CMake regeneration remains blocked by
  the existing QML generation/FileTracker stall, so no CMake-generated current-
  binary Qt lifecycle gate is claimed. Phase 2 remains open for the remaining
  retained adapters, report/export adapter/model migration, and broader
  per-slice fixture coverage.

- **2026-09-01 — Teacher-import service and adapter connected.** The
  Qt-free `TeacherImportService` now owns ISO source-date validation,
  Hangul-only Korean matching, case-insensitive Native English/GS Team
  matching, blank-field preservation, duplicate/ambiguous diagnostics,
  transactional writes, and monotonic latest-source-date persistence. The
  retained Qt `TeacherImportRepository` converts its existing plan and result
  models through a cached file-backed engine connection. The focused engine
  test passed in x64/x86 Debug/Release, and the retained Qt teacher-import
  regression passed with file-backed fixtures. Phase 2 remains open for the
  remaining retained adapters, report/export adapter/model migration, and
  broader per-slice fixture coverage.
- **2026-09-01 — Testing-class service and adapter connected.** The Qt-free
  `TestingClassService` now owns required-field validation, mixed-level choice
  catalogs, class-info/class-row persistence, assignment creation, ordering,
  membership, and transactional cleanup. The retained Qt
  `TestingClassRepository` now converts through UTF-8 and delegates its CRUD,
  list, membership, and assignment operations through the engine. Its focused
  native test passed in x64/x86 Debug/Release, and the retained Qt repository
  regression passed against a temporary file-backed profile.
- **2026-09-01 — Testing-block service and adapter connected.** The Qt-free
  `TestingBlockService` now owns canonical weekday/HH:mm keys, plain-versus-
  special assignment mapping, explicit replacement conflicts, testing-class
  validation, UTF-8 rooms, typed row errors, and assignment rollback. The
  retained Qt `TestingBlockRepository` now delegates through the engine with
  explicit Qt/UTF-8 conversions. Its focused native test passed in x64/x86
  Debug/Release, and the retained Qt repository regression passed with
  file-backed fixtures, including explicit confirmation when replacing a
  class assignment with a plain block or vice versa. Phase 2 remains open for
  the remaining retained adapters, report/export adapter/model migration, and
  broader per-slice fixture coverage.
- **2026-09-01 — Campus-record service and adapter connected.** The Qt-free
  `CampusRecordService` now owns all fourteen campus text fields, CRUD/save
  semantics, name ordering, typed invalid-id/not-found/schema errors, and
  UTF-8 SQLite row mapping. The retained Qt `CampusRecordRepository` now
  converts through UTF-8 and delegates its file-backed operations through a
  cached engine `OpenDatabase` connection. The focused service test compiled,
  linked, and passed directly in VS 2026/v145 x64 and x86 Debug/Release lanes; the
  retained adapter compiled against Qt 6.11/MSVC and passed a temporary
  file-backed UTF-8 save/load/update/list/delete smoke test. CMake
  reconfiguration/regeneration passed for both architectures, but the normal
  target build remains blocked by the existing MSBuild FileTracker
  `UnauthorizedAccessException` in `ZERO_CHECK`/compile tracking. Phase 2
  remains open for the remaining retained adapters, report/export migration,
  and broader per-slice fixture coverage.
- **2026-09-01 — Application-settings service and adapter connected.** The
  Qt-free `ApplicationSettingsService` now owns prepared app-settings upserts,
  typed SQLite value binding, reads, and transactional batch rollback. The
  retained Qt `SettingsRepository` converts QVariant values through a cached
  file-backed engine connection while preserving its public API and localized
  diagnostics. Teacher-import and schedule-import app-settings reads/writes
  now use the same engine service. The focused engine test compiled and passed
  in direct VS 2026/v145 x64 and x86 lanes; a Qt 6.11/MSVC link-level smoke
  passed UTF-8, QVariant conversion, and cross-connection batch rollback. The
  changed adapter and both import call sites also passed compile-only checks.
  The normal Qt CMake target build remains blocked by the existing MSBuild
  FileTracker `UnauthorizedAccessException`; Phase 2 remains open for the
  remaining retained adapters, report/export migration, and broader per-slice
  fixture coverage.
- **2026-09-01 — File-backed DatabaseSession boundary connected.** The retained
  Qt `DatabaseSession` now preflights ordinary profile paths through engine
  `OpenDatabase`, so UTF-8 path preparation, parent-directory creation, and
  schema migration cross the portable boundary before the Qt connection is
  created. Exact `:memory:` sessions retain Qt schema initialization, and Qt
  connections retain foreign-key setup. The focused lifecycle source compiled
  and `git diff --check` passed; the current CMake target rebuild remains
  blocked by the existing MSBuild FileTracker `UnauthorizedAccessException`,
  so no current-binary lifecycle result is claimed for this slice.

- **2026-09-02 — Personal-details settings service and adapter connected.** The
  Qt-free `PersonalDetailsService` now owns personal-name/campus and Zoom
  settings, `N/A` defaults, legacy `subPrep/...` fallback and promotion,
  signature mode/font/text, opaque base64 storage, campus-only updates, and
  transactional nine-setting saves. The retained Qt
  `PersonalDetailsRepository` converts through UTF-8 and delegates file-backed
  personal-details and campus persistence through the engine while keeping
  image preparation and localized/default API behavior at the Qt edge. The
  focused engine harness passed directly in VS 2026/v145 x64 and x86 Debug;
  the retained Qt adapter and lifecycle selection passed an offscreen
  link-level Qt 6.11/MSVC smoke. CMake reconfiguration passed for x64 Qt and
  x64/x86 WinUI, and direct compile checks passed for the engine, adapter,
  feature-service, and lifecycle sources. The normal CMake target rebuild
  remains blocked by the existing MSBuild FileTracker
  `UnauthorizedAccessException`, so no current-binary CMake lifecycle result
  is claimed. Phase 2 remains open for the other retained adapters,
  report/export adapter/model migration, and broader per-slice fixture
  coverage.

- **2026-09-02 — Teacher and class-information validation adapters connected.**
  The retained Qt `TeacherValidator` and `ClassInfoValidator` now convert
  models and nested schedule values through UTF-8/std-library engine contracts.
  Normalization, phone formatting, validation, and notes policy are engine-owned
  while Qt diagnostics retain field/severity/row/column mapping and length
  message metadata. Direct VS 2026/v145 Qt compile checks passed; the focused
  `ClassMngrSharedPolicyTests` selection passed 1/1; and the current engine CTest
  lane passed all 43 available executables, with nine generated targets absent
  and therefore not run. The normal Qt CMake target build remains blocked by
  the existing MSBuild FileTracker `UnauthorizedAccessException`. Phase 2
  remains open for remaining report/export adapters and broader fixture
  evidence.
- **2026-09-02 — Standalone class-time validation adapter connected.** The
  retained Qt `ClassTimeValidator` now converts class-time values through UTF-8
  and delegates normalization and validation to the Qt-free engine contract.
  The engine now reports class-time row/column metadata, while the adapter
  restores the legacy Qt invalid-value, ordering, and duplicate-row arguments.
  Direct VS 2026/v145 Qt and engine compile checks passed. A manually linked
  Qt 6.11/MSVC `ClassMngrSharedPolicyTests` binary passed with explicit
  normalization, row/column, and diagnostic-argument assertions. The normal
  Qt CMake target build remains blocked by the existing MSBuild FileTracker
  `UnauthorizedAccessException`. Phase 2 remains open for the remaining
  report/export adapters, other retained adapters, and broader fixture
  evidence.
- **2026-09-02 — Class-information catalog adapter connected.** The retained
  Qt `ClassInfoConfig` now converts the engine's UTF-8 grade, level, and book
  catalogs into its existing `QStringList` API; Qt consumers no longer carry a
  duplicate catalog map. Focused parity coverage compares all public catalog
  lists, canonical grade/level lookups, GraVoca and fallback branches, and
  case-sensitive invalid inputs. Direct VS 2026/v145 Qt compile and MOC checks
  passed; CMake configure/generate succeeded, but the focused target rebuild
  remains blocked by the existing MSBuild FileTracker
  `UnauthorizedAccessException`, so no current-binary Qt pass is claimed.
  Phase 2 remains open for remaining report/export adapters and broader
  fixture evidence.
- **2026-09-02 — Class-tab navigation service and adapter connected.** The
  Qt-free `ClassTabNavigationService` now owns adaptive/forced grade grouping,
  catalog-ordered tabs, schedule/day-label formatting, duplicate-label
  disambiguation, and regular/intensive day filtering. The retained Qt model
  converts through UTF-8 while preserving its existing Qt-facing model shape,
  `QSet` filter API, and localized fallback labels. The focused native CTest
  passed in x64 and x86 Debug WinUI lanes, and the retained Qt
  `ClassMngrClassTabNavigationModelTests` CMake target passed 1/1 in the x64
  Debug Qt lane. Phase 2 remains open for the remaining report/export
  adapters, other retained adapters, and broader per-slice fixture coverage.
- **2026-09-02 — Qt formal deprecation gate completed.** The three formal Qt
  deprecation groups were migrated across the nine audited files: UTC calls now
  use `QTimeZone::UTC`, mouse handlers use `position().toPoint()`, and fixed-
  argument `invokeMethod` calls use typed variadic arguments. The Qt 6.12.0
  Debug audit build with `QT_DISABLE_DEPRECATED_UP_TO=0x060C00` compiled the
  production targets, five affected Qt test targets, and the Windows visual-
  capture target; all five focused CTest selections passed 1/1. The retained
  class-transfer executable requires the matching Qt 6.12 runtime; the global
  Qt 6.11 PATH causes the entry-point error, and the matched-runtime run still
  exposes seven database-dependent setup failures. Phase 2 remains open for
  report/export adapters, other retained adapters, and broader fixture
  evidence.
- **2026-09-02 — Evaluation-default selection policy and adapter connected.**
  The Qt-free `EvaluationDefaultSelection` contract now owns term-name
  mapping, current/previous-term fallback, populated-row detection, and
  M1/M2/M3 grade classification. The retained Qt helpers convert terms and
  rows through UTF-8 while keeping calendar lookup, settings policy, service
  availability, and presentation at the boundary. The focused
  `ClassMngrEngineEvaluationDefaultSelectionTests` target built and passed
  1/1 in x64/x86 Debug and Release WinUI lanes. The retained Qt target could
  not regenerate on this host because the project requires Qt 6.12.0 and only
  Qt 6.11.1 is installed; no current Qt parity result is claimed. Phase 2
  remains open for report/export adapters, other retained adapters, and
  broader fixture evidence.
- **2026-09-02 — Calendar-event import extraction completed.** The Qt-free
  `CalendarEventImportService` now owns month-grid date mapping, legend
  classification, note ranges/cancellation, campus-note title suffixes,
  normalization, and duplicate signatures. The retained Qt parser is now a
  workbook/event conversion adapter while ZIP/XML decoding and
  network/database orchestration remain Qt-owned. The focused engine target
  built and passed 1/1 in the Windows x64 WinUI lane, and the retained Qt
  `ClassMngrCalendarImportTests` target built and passed 1/1 in the Qt 6.12
  audit lane using `/p:TrackFileAccess=false` for the existing host FileTracker
  permission issue. Phase 2 remains open for report/export adapters, other
  retained adapters, and broader fixture evidence.
- **2026-09-02 — Schedule-import review validation connected.** The Qt-free
  `ScheduleImportService` now exposes read-only plan/current-state validation,
  including projected schedule conflicts, through the retained Qt repository,
  compatibility facade, and feature service. The review dialog uses that
  authoritative preflight after its localized Qt checks while retaining
  presentation-owned conflict messaging. The focused engine test passed in all
  four x64/x86 Debug/Release WinUI lanes, and the retained Qt dialog test passed
  1/1 in the Qt 6.12 audit lane with `/p:TrackFileAccess=false`. The separate
  existing `ClassMngrScheduleImportTests` selection still has the two known
  apply-case failures `intensiveModesPreserveOrReplaceAbsentHours` and
  `skippedExactMatchPreservesItsSchedule`; they are recorded as a review-later
  baseline item because this slice did not change import/apply behavior. Phase
  2 remains open for report/export adapters, other retained adapters, and
  broader fixture evidence.
- **2026-09-02 — Schedule-import matching and pattern rules extracted.** The
  Qt-free `ScheduleImportRules` contract now owns weekday grouping and
  compatibility, schedule-kind fallback/target selection, class-option
  eligibility, supported meeting patterns, and meeting-pattern validation.
  `ScheduleImportService` no longer carries a duplicate implementation, and
  the retained Qt helper now only converts values and restores localized
  presentation. The focused native target passed 1/1 in all four x64/x86
  Debug/Release WinUI lanes, and the retained Qt review-dialog target passed
  1/1 in the Qt 6.12 audit lane. The two existing apply-case failures in
  `ClassMngrScheduleImportTests` remain recorded as a separate review-later
  baseline item. Phase 2 remains open for report/export adapters, other
  retained adapters, and broader fixture evidence.

- **2026-09-02 — Schedule-time formatter adapter connected.** The retained Qt
  `ScheduleTimeFormatter` now delegates display-time and range-label formatting
  to the Qt-free `ScheduleReportService` through explicit UTF-8 conversion,
  preserving invalid-input handling, 12/24-hour output, 50/55-minute endings,
  and cross-period labels. Focused parity coverage was added for the adapter
  and direct engine results. At tested revision `75fe1cc` on Windows x64 with
  VS 2026/v145 and the Qt 6.12 CMake tree, the existing engine schedule-report
  test executable passed directly and the committed diff passed `git show
  --check`. The Qt 6.12 CMake target was generated, but its build remains
  blocked by the host MSBuild FileTracker `UnauthorizedAccessException`, so the
  new Qt CTest executable was not available to run. Phase 2 remains open for
  the remaining report/export adapters and models, other retained adapters, and
  broader fixture evidence.

- **2026-09-02 — Schedule-import apply fixture baseline corrected.** The
  retained Qt `intensiveModesPreserveOrReplaceAbsentHours` and
  `skippedExactMatchPreservesItsSchedule` fixtures now finish their
  verification `QSqlQuery` cursors before the second engine-backed apply,
  releasing the SQLite read locks held across the separate Qt/engine
  connections. The production import/apply implementation is unchanged. Both
  focused QtTest cases passed in implementation validation, and
  `git diff --check` passed. A fresh Qt 6.12 CMake target rebuild on this host
  remains blocked by the MSBuild FileTracker `UnauthorizedAccessException`.
  Phase 2 remains open for the remaining report/export adapters and models,
  other retained adapters, and broader fixture evidence.

- **2026-09-02 — PowerPoint job-model result boundary connected.** The
  retained Qt PowerPoint job-model adapter now returns a typed
  `Result<BatchJob>`, translates engine validation errors, and stops the
  caller before renderer setup when job construction fails. Successful
  renderer-neutral mapping and JSON output remain unchanged; NFC text
  normalization and comment-fit sizing remain explicit Qt presentation
  responsibilities with focused parity coverage. The Qt x64 Debug target
  built successfully and `ClassMngrSpeakingEvalBatchReportServiceTests`
  passed 1/1 in 47.76 seconds; the existing Windows x64 Release engine
  PowerPoint-job regression also passed. A current-tree x64 Debug rebuild was
  attempted but hit the host MSVC generated-object `Permission denied`/
  `D8040` process issue, so no clean current-tree full build is claimed.
  Phase 2 remains open for the remaining report/export adapters and models,
  other retained adapters, and broader fixture evidence.

- **2026-09-02 — MSVC generated-object permission issue stabilized.** CMake
  now disables MSVC `/MP` compilation by default and sets Visual Studio's
  `TrackFileAccess=false` for generated targets. This avoids the host's
  parallel FileTracker/object contention while retaining an explicit
  `CLASSMNGR_ENABLE_MSVC_PARALLEL_COMPILE=ON` opt-in for stable hosts. The
  current Windows x64 Debug engine target was clean-rebuilt successfully from
  fresh objects with no `C1083`, `D8040`, or FileTracker failure. Phase 2
  remains open for the remaining report/export adapters and models, other
  retained adapters, and broader fixture evidence.
- **2026-09-02 — Calendar-event cache adapter connected.** The retained Qt
  `CalendarEventCache` now preflights file-backed profiles through engine
  `OpenDatabase`, preserving UTF-8 path normalization and parent-directory /
  schema preparation before its background Qt read connection is opened. The
  focused regression exercises a nested Korean profile path and confirms the
  preflight-created database reaches the loaded empty-range state. CMake
  configure/generate, compile-only MSVC validation for the focused sources and
  owning feature target, and `git diff --check` passed. The ordinary
  dependency build first hit the existing MSBuild FileTracker
  `UnauthorizedAccessException` and generated-PDB contention; after sequential
  dependency rebuilds with project references disabled, the focused test
  linked and both its full executable and new focused case exited 0 offscreen.
  Phase 2 remains open for the remaining report/export adapters and models,
  other retained adapters, and broader fixture evidence.

- **2026-09-02 — Database-port cross-slice fixture evidence expanded.** The
  engine-written → retained-Qt round trip now writes and verifies representative
  calendar-event, roster, speaking-evaluation, and campus-record values through
  the Qt-free services and retained Qt SQLite. The fixture generator's temporary
  Qt-written profile also uses the canonical teacher preferred-name choice under
  the current validator. The shared speaking-evaluation constants are now
  declared once and reused by class-transfer headers. The Qt 6.12.0 x64
  `ClassMngrDatabasePortFixtureGenerator` target built with serialized MSVC
  compilation and its fixture verification executable exited 0; `git diff
  --check` passed. Phase 2 remains open for report/export adapters, other
  retained adapters, and the full per-slice fixture matrix.

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
