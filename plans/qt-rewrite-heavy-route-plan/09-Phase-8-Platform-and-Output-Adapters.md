# Phase 8 — Platform and Output Adapters

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Build and test verification

- Use `build/windows-x64-debug` as the standard local Windows x64 Debug
  build folder for slice work. Reuse this same configured folder across
  slices and phases; do not create slice-, task-, or reviewer-specific build
  folders. Build the affected targets and their dependencies, and let the
  build system recompile changed or out-of-date inputs.
- If a fresh build is needed, empty the applicable standard build folder
  before configuring and building in it. For Windows x64 Debug, keep using
  `build/windows-x64-debug`; do not create a new folder for the fresh build.
  Apply the same rule when changing the build system, toolchain, or
  dependency configuration; when stale artifacts could explain a result;
  or when a phase gate explicitly requires clean-checkout evidence.
- A gate requiring another platform or configuration must use that CMake
  preset's standard `build/<preset-name>` folder and reuse it for that
  preset, rather than creating a slice-specific folder.
- Keep any phase-specific full builds, test suites, platform matrices, and
  packaging checks required by an exit gate. Record whether verification
  used an incremental or fresh build, which targets were built, and which
  tests ran. Describe focused results as focused; do not report them as a
  full-suite pass.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phases 2, 3, and 7
- Blocks: Cross-platform parity and final release
- Owner: Unassigned
- Last updated: 2026-11-12
- Current note: Phase 8 remains In progress and its exit gate is open. The local macOS candidate at `e651b0164913478394945d8a1bc5936758bc852b` passed a clean universal Debug build, all 620 CTest targets on the normal-access rerun, required output captures, C53, and current-HEAD universal Release package verification. The macOS Debug leg was removed from GitHub Actions because it required the billed `macos-15-large` runner and took 34m55s locally; the clean macOS candidate now runs on the local Mac mini. The macOS Release workflow remains on a standard runner, which is free for this public repository. The Windows x64 Release package gap is resolved by official hosted run [43156278197](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/43156278197). The clean Windows x64 candidate passed at `bb37cf9aaa4562b4fdf9c9b592df2012928bf5c6`; Fedora remains accepted for Linux evidence at `e44f400c826250e7f4645e0e5a2c7f8df8163e54`. A later source-equivalent Windows/Linux run has two Windows target timeouts and one Linux Pretendard PDF assertion failure; those remain unresolved. The final cross-host review remains outstanding. See the [Phase 8 progress log](09-Phase-8-Progress-Log.md), [Phase 8a plan](09.5-Phase-8a-CI-Failure-and-QWARN-Triage.md), and [Phase 8a progress log](09.5-Phase-8a-Progress-Log.md).

### Slice batches

Use the shared [current slice-batch tracker](Current-Slice-Batches.md) for live work when Phase 8 is active. It contains only the active
batch and the next batch once discovery starts; completed batches are
not archived there.

## Progress log location

Write all historical Phase 8 progress—including implementation and verification results, decisions, blockers, acceptance findings, and exit-gate changes—to the [Phase 8 progress log](09-Phase-8-Progress-Log.md), not this plan. Keep current status, objective, requirements, and contracts here; keep live batches in the shared tracker.

## Objective


Move platform-specific behavior and large output workflows behind narrow, testable interfaces.

## Adapter interfaces

Create interfaces for:

- File dialogs.
- Clipboard.
- URL launching.
- Printing.
- PDF generation.
- PowerPoint automation.
- ZIP and package handling.
- Font loading.
- Window state persistence.
- Application update checking.
- Platform memory diagnostics.

Feature and domain code must depend on these interfaces, not directly on Windows, macOS, Linux, or external-application APIs.

## Work packages

### 8.1 File and shell integration

Preserve:

- Open and save dialogs.
- Recent files.
- Clipboard behavior.
- External URL launching.
- Application icon.
- Window geometry and splitter persistence.
- Platform-specific dialog behavior.

Keep the current user-visible behavior while moving implementation behind adapters.

### 8.2 Printing and PDF

Initially retain Qt PDF and printing to preserve output.

The QtPdf viewer is a separate on-demand path. Catalog initialization must not
load a PDF document or render pages. The adapter owns the loaded
`QPdfDocument` for the active viewer session and must close/release it when
the document is replaced, the viewer is closed, or the page is left or
released. Generated and print-output PDFs remain operation-scoped.

Isolate:

- Page layout.
- Resource lookup.
- Font embedding.
- Image loading.
- PDF page creation.
- QtPdf document load, status, page rendering, close, and release.
- Print preview.
- Printer selection.
- Output file handling.

Avoid retaining source documents, rendered pages, images, and output buffers simultaneously.

Sub Prep package generation and PDF/print output must consume the operation-
scoped projection defined by [Sub Prep Class Information and Output Memory
Plan](sub-prep-class-information-memory-plan.md). The adapter boundary must
not require the page to materialize class widgets or retain the page's rich UI
model while output is being generated.

The [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) extends this rule to speaking
reports and other large output workflows. Output adapters must consume
bounded application projections, render in chunks when necessary, and release
source records, HTML, rendered pages, and final buffers at explicit stage
boundaries.

### 8.3 PowerPoint automation

Preserve the current speaking-evaluation PowerPoint workflow.

Implement:

- Availability detection.
- External application connection.
- Workspace creation.
- Slide generation.
- Failure reporting.
- Cleanup.
- Cancellation.

PowerPoint objects and temporary files must be released even when an operation fails.

### 8.4 Packaging and application updates

The application updater remains a normal application feature.

The update package must contain:

- The executable.
- Required Qt deployment files.
- QML files if retained.
- Canonical resources.
- Required platform plugins.
- Licenses and metadata.

There must be one application update path, not an executable update path plus a resource update path.

### 8.5 Calendar presentation adapter

Keep the QML calendar implementation isolated behind a calendar view interface if it is required for appearance parity.

The adapter must:

- Load QML on calendar entry.
- Avoid creating QML objects during startup.
- Release the QML view when appropriate.
- Expose calendar data through a compact model.

### 8.6 Transfer and package memory

Class-transfer and package operations must follow the [Qt Rewrite Memory
Hotspot Remediation Plan](memory-hotspot-remediation-plan.md):

- write and read transfer data in bounded stages or through a streaming
  device where the format permits;
- avoid retaining raw JSON, parsed documents, domain packages, and preview
  copies simultaneously without a documented reason;
- preview compact summaries before loading full roster/evaluation details;
- transfer operation ownership explicitly into the dialog or job;
- release all temporary data on success, cancellation, validation failure, and
  output failure.

The existing transfer format and user-visible validation behavior remain the
parity contract.

## Deliverables

- Platform adapter interfaces.
- Windows, macOS, and Linux implementations.
- PDF and printing adapters.
- PowerPoint adapter.
- Application-update packaging integration.
- Calendar adapter.
- Adapter tests and failure handling.

## Exit gate

All external integrations work from v2 without platform-specific code leaking into domain, application, or feature logic.

Generated output matches the baseline on every supported platform.

### Cross-platform package verification evidence

Successful package-verification passes for macOS, Linux, and Windows may be
combined into one valid platform matrix even when they were run at different
HEADs, provided the changes between those HEADs do not alter source or packaged
inputs and the package workflow/verifier contract is unchanged. Record each
run's HEAD and verify source/package-input equivalence across the tested
revisions; documentation-only commits do not invalidate the matrix. If a
relevant source, resource, build, or packaging input changed, do not combine
the affected results as one source-equivalent matrix.

## Heavy-route requirements

- For every Phase 8 slice, use the heavy route: move the behavior behind its
  narrow adapter boundary, verify the full output or platform workflow, and
  remove temporary direct-to-legacy paths after acceptance.
- Do not duplicate business rules in platform implementations.
- Do not leave PowerPoint, QtPdf, or PDF output objects owned by widgets
  indefinitely; active viewer documents are session-scoped.
- Do not make resource updates a hidden part of application updates.
- Do not rewrite output formats and UI behavior simultaneously without parity fixtures.
