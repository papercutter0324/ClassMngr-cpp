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

Status vocabulary:

- **Not started:** no phase deliverable has been accepted.
- **In progress:** implementation or required validation is underway.
- **Blocked:** the phase cannot advance without an explicit decision or an
  external prerequisite.
- **Complete:** every exit-gate item has recorded evidence.

## Progress Dashboard

Last updated: 2026-08-29 (Asia/Seoul)

| Phase | Status | Current evidence or next gate |
| --- | --- | --- |
| [Phase 0 — Baseline and contracts](phase-0-baseline-and-contracts.md) | **Complete** | Owner-accepted Qt captures, parity inventory, fixture corpus, and retained-platform validation exist. |
| [Phase 1 — Build split and WinUI bootstrap](phase-1-winui-bootstrap.md) | **In progress** | Local VS 2026/v145 x64/x86 Debug/Release builds and staged smoke tests pass. The retained Qt route passes 68/68 non-visual tests; the explicit 69-test visual lane is 68/69 because this host is at 125% DPI. The hosted lane now captures an x86 Release idle-memory report; runner execution, owner-reviewed interaction evidence, and representative x86 peak-budget evidence remain. |
| [Phase 2 — Portable engine extraction](phase-2-portable-engine-extraction.md) | **Not started** | `SemanticVersion` is the seed slice; database and use-case extraction remain. |
| [Phase 3 — WinUI application foundation](phase-3-winui-application-foundation.md) | **Not started** | Begins after the Phase 2 engine gate. |
| [Phase 4 — Shared UX and high-risk controls](phase-4-shared-ux-and-high-risk-controls.md) | **Not started** | Begins after the application foundation is stable. |
| [Phase 5 — Shell and first feature slice](phase-5-shell-and-first-feature-slice.md) | **Not started** | No feature parity is claimed by the current WinUI bootstrap shell. |
| [Phase 6 — Data-entry feature migration](phase-6-data-entry-feature-migration.md) | **Not started** | Port vertical slices in the risk order defined by the phase file. |
| [Phase 7 — Media, output, and OS services](phase-7-media-output-and-os-services.md) | **Not started** | PDF, printing, exports, updates, and PowerPoint remain Qt-owned. |
| [Phase 8 — Hardening, packaging, and cutover](phase-8-hardening-packaging-and-cutover.md) | **Not started** | The Qt Windows release remains public until this phase passes. |

## Current Focus

The active phase is Phase 1. The next accepted checkpoint is a clean Windows
runner execution of the Visual Studio 18 2026/v145 bootstrap lane, followed by
owner-reviewed interaction evidence and representative x86 peak-memory
evidence. It requires all of the following:

1. Pin a stable Windows App SDK and C++/WinRT toolchain.
2. Add a WinUI 3 XAML application that calls `ClassMngrEngine` and does not
   discover or load Qt.
3. Prove x64 and x86 Debug and Release builds from a clean machine/runner.
4. Prove architecture-correct, unpackaged, self-contained staging and launch
   for x64 and x86.
5. Exercise light/dark theme, DPI, keyboard focus, and Korean IME in a small
   representative form.
6. Measure x86 memory behavior against the shared 200 MiB steady-state target
   and record worst-case peak usage before considering release support.
7. Keep the retained Windows, macOS, and Linux Qt products green.

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
- **2026-08-29 — Retained Windows Qt validation refreshed.** The startup
  controller still reapplies the loaded font-size setting when its actions are
  connected, and the retained Windows Qt Release product previously rebuilt
  successfully with its deployed runtime. The current fresh Debug/visual
  result is recorded above: 68/68 non-visual tests pass, with only the
  125%-scale visual-capture gate open. Detailed matrix and environment notes
  are in
  [`phase1-local-validation.md`](../../docs/porting/windows-winui/phase1-local-validation.md).

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
