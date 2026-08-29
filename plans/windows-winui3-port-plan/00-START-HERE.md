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
    +-- ClassMngrWindowsWinUI
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
- Windows x64 is the release gate. ARM64 remains build-only and non-blocking
  until a separate support decision supplies runtime, performance, and package
  evidence.
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
| [Phase 1 — Build split and WinUI bootstrap](phase-1-winui-bootstrap.md) | **In progress** | Qt-free engine/build split, minimal Win32 shell, native staging, and local tests exist. Replace the provisional shell with a pinned WinUI 3 C++/WinRT app and prove self-contained deployment. |
| [Phase 2 — Portable engine extraction](phase-2-portable-engine-extraction.md) | **Not started** | `SemanticVersion` is the seed slice; database and use-case extraction remain. |
| [Phase 3 — WinUI application foundation](phase-3-winui-application-foundation.md) | **Not started** | Begins after the Phase 2 engine gate. |
| [Phase 4 — Shared UX and high-risk controls](phase-4-shared-ux-and-high-risk-controls.md) | **Not started** | Begins after the application foundation is stable. |
| [Phase 5 — Shell and first feature slice](phase-5-shell-and-first-feature-slice.md) | **Not started** | No feature parity is claimed by the current native shell. |
| [Phase 6 — Data-entry feature migration](phase-6-data-entry-feature-migration.md) | **Not started** | Port vertical slices in the risk order defined by the phase file. |
| [Phase 7 — Media, output, and OS services](phase-7-media-output-and-os-services.md) | **Not started** | PDF, printing, exports, updates, and PowerPoint remain Qt-owned. |
| [Phase 8 — Hardening, packaging, and cutover](phase-8-hardening-packaging-and-cutover.md) | **Not started** | The Qt Windows release remains public until this phase passes. |

## Current Focus

The active phase is Phase 1. The next accepted checkpoint requires all of the
following:

1. Pin a stable Windows App SDK and C++/WinRT toolchain.
2. Add a WinUI 3 XAML application that calls `ClassMngrEngine` and does not
   discover or load Qt.
3. Prove Debug and Release builds from a clean machine/runner.
4. Prove unpackaged, self-contained installation and launch through an
   isolated development stage.
5. Exercise light/dark theme, DPI, keyboard focus, and Korean IME in a small
   representative form.
6. Keep the retained Windows, macOS, and Linux Qt products green.

The existing `ClassMngrWindowsNative` Win32 shell and Direct2D SDK capability
test are provisional Phase 1 evidence. They are not the target presentation
architecture and may be removed after equivalent WinUI build, manifest,
deployment, and smoke-test coverage exists.

## Completed Evidence Carried Forward

- Phase 0 was accepted on 2026-08-29.
- The Windows Qt visual harness covers 80 registered capture rows, including
  owner-reviewed 100%, 150%, and 200% DPI evidence and representative editors
  and dialogs.
- The cross-platform fixture corpus contains eleven SHA-pinned database cases.
- Retained Windows, Linux, and macOS Qt validation passed at the Phase 0/1
  handoff recorded in the historical status file.
- A Qt-free `ClassMngrEngine` target and ordinary engine test executable exist.
- Native-only Debug and Release configurations, staging, manifest checks, and
  a separate native CI workflow exist locally.

Historical evidence remains under
[`docs/porting/windows-direct2d`](../../docs/porting/windows-direct2d/README.md).
The directory name records the original approach; it does not define the new
presentation architecture.

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
  progress because the provisional Win32 shell has not yet been replaced by a
  WinUI 3 C++/WinRT application or validated with Windows App SDK deployment.

## Shared Completion Rules

- Every phase preserves `.tps` and supported legacy `.db` behavior.
- Every migrated slice uses the same engine rules and schema as the Qt apps.
- Korean text and IME, keyboard-only use, DPI, theme, failure paths, and
  unsaved-change behavior are acceptance work, not final polish.
- Screenshots supplement semantic and behavioral assertions; they do not prove
  focus, persistence, IME, or accessibility behavior.
- The WinUI target must not load or deploy Qt.
- The retained Qt products must remain releasable until Windows cutover.
