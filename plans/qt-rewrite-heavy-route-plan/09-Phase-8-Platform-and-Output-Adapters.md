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
- Last updated: 2026-11-03
- Current note: Keep feature code platform-neutral and isolate external integrations.

### Slice batches

Use the shared [current slice-batch tracker](Current-Slice-Batches.md) for live work when Phase 8 is active. It contains only the active
batch and the next batch once discovery starts; completed batches are
not archived there.

## Progress log

Use this section only when a decision, acceptance result, verification
finding, blocker, or exit-gate detail needs a durable record. Routine
slice chronology and closed batch history do not need to be archived;
keep current status in this plan and live batches in the shared tracker.

### 8.1.1 — External URL launch boundary (accepted)

Added the `ExternalUrlLaunchPort` application interface and its Qt
`QDesktopServices` adapter. Bootstrap now supplies it to shell navigation,
Speaking Evaluation, and Campus Directory. Those callers retain their current
URLs, HTTPS checks, localized failure behavior, and shell navigation behavior;
the platform adapter is the only direct `QDesktopServices` caller in the
assigned paths.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed for `ClassMngrNext`, `ClassMngrNextBootstrapTests`,
`ClassMngrNextSpeakingEvaluationPageTests`, and
`ClassMngrNextCampusDirectoryTests`. Focused tests passed for Campus Map URL
handoff/failure, AI URL handoff/failure, Bootstrap AI composition, and shell
navigation. Independent recheck confirmed Naver failure and Kakao success
handoffs; the Campus Map startup fixture has no URL, so those link assertions
use the injected action seam. No full suite was run.

### 8.1.2 — Speaking Evaluation clipboard adapter (accepted)

Added the write-only `ClipboardWritePort` and Qt clipboard adapter, injected
through Bootstrap into the Speaking Evaluation page and both AI dialogs. The
page and dialogs no longer write to the clipboard directly. Focused coverage
preserves copy-only dialog state, copy-before-open ordering, failure behavior,
batch copying of the displayed prompt, and Bootstrap adapter wiring.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build for `ClassMngrNextSpeakingEvaluationPageTests` and
`ClassMngrNextBootstrapTests` passed. Independent CTest verification passed
both targets (2/2); no full suite was run. `QClipboard::setText` has no result
value, so this port does not expose an operating-system clipboard-write
failure result.

### 8.1.3 — Schedule Import file-dialog adapter (accepted)

Bootstrap now supplies the existing `FileDialogPort` for Schedule Import
workbook selection, and the dialog no longer has a direct `QFileDialog`
fallback. The request retains the `ImportWorkbook` purpose, “Import Schedule”
title, `*.xlsx` filter, and remembered-directory behavior. Cancellation
preserves the current workbook selection.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed for `ClassMngrNextSchedulePageTests` and
`ClassMngrNextNormalScheduleImportIntegrationTests`. Independent focused runs
passed both filters (3/3 each), including the Bootstrap-selected workbook and
Gateway import, row-count checks, and unavailable-Gateway failure. The normal
headless verification uses Qt's offscreen platform. No full suite was run.

### 8.1.4 — Sub Prep output directory chooser (accepted)

Sub Prep now requests its output directory through the existing `FileDialogPort`
with `SubPrepPackage` purpose, “Select Folder” title, and the current field
value as the initial directory. A selection updates the field; cancellation
preserves it. Canceling the output dialog does not invoke package generation.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build and independent focused tests passed for `ClassMngrNextSubPrepPageTests`
and `ClassMngrNextBootstrapTests` (3/3 each). The page no longer calls
`QFileDialog`; the platform presenter was inspected but the real operating
system chooser was not opened. No full suite was run.

### 8.1.5 — Dialog geometry (accepted)

`DialogShell` now accepts a UI-owned, binary-safe geometry callback bundle.
Next composition supplies callbacks backed by the existing Application port
and SettingsManager platform adapter for Update, DocumentViewer printing,
Roster output, and both custom and native print dialogs. The shared shell no
longer includes Next Application or Platform types. Empty callbacks retain the
existing Legacy settings path; Next's RosterPrintDialog requires a complete
read/write pair and rejects missing or partial pairs, preventing accidental
fallback to Legacy storage. Stable dialog keys, first-show restore, close and
destruction persistence, and screen clamping are preserved.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental affected-
target Debug build passed in `build/macos-clang-debug`. Independent CTest
verification passed all five focused suites: DialogShell, Next DocumentViewer,
Next Roster Output, Next Update Coordinator, and the Next SettingsManager
geometry adapter (5/5). Coverage includes binary-safe callback round-trip,
Legacy geometry persistence, Next callback composition, and rejection of
missing or one-sided Roster callbacks. The native macOS print dialog was not
opened interactively; its callback path compiled and was inspected. No full
suite was run.

### 8.1.6 — Next main-window geometry (accepted)

Added a dedicated `WindowGeometryPort` and SettingsManager adapter using the
existing `ui/windowGeometry` key. Bootstrap supplies the port to
`ApplicationShell`, which restores the binary-safe Qt geometry after applying
its default and minimum size and before the first show. It saves only after
the close handler leaves the close event accepted; rejected closes do not
write. Missing or invalid geometry preserves the default bounds. The shell
contains no SettingsManager or Platform dependency.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed in `build/macos-clang-debug` for the affected production,
navigation, bootstrap, and geometry-adapter targets. Independent CTest
verification passed `ClassMngrNextNavigationTests`,
`ClassMngrNextBootstrapTests`, and
`ClassMngrNextPlatformSettingsManagerWindowGeometryPortTests` (3/3). Tests
cover pre-show restore, accepted-close persistence, rejected-close no-write,
empty/invalid payload defaults, and exact-key binary round-trip. The bootstrap
suite also ran its embedded visual-capture slots; dedicated visual-capture
targets compiled but were not run or visually reviewed. No full suite was run.

### 8.1.7 — Next splitter state (accepted)

Added a dedicated `SplitterStatePort` and SettingsManager adapter with four
isolated `ui/next/splitters/...` keys for the retained Classes Navigator,
Testing Classes Manager, Speaking Evaluation, and Class Transfer page
splitters. The transient Schedule Import review splitter remains unchanged.
Each page requires the port, restores its configured splitter state once, and
saves on suspension. Page reactivation retains the live splitter without
reapplying stored state. Missing or invalid payloads preserve the current
defaults; the adapter syncs writes so the final active-page save during
PageHost destruction is flushed.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed in `build/macos-clang-debug` for `ClassMngrNext`, the four focused
page targets, the settings-adapter target, and affected visual-capture and
Schedule targets. Independent CTest verification passed the four page suites
and `ClassMngrNextPlatformSettingsManagerSplitterStatePortTests` (5/5).
Coverage includes isolated keys, binary round-trip, absent/invalid defaults,
one-time restoration, suspension persistence, and retained-page reactivation.
The full suite was not run; visual captures were compiled but not visually
reviewed.

### 8.1.8 — Next application icon (accepted)

Next startup now loads the existing 256×256 PNG through `ResourceLoader` and
sets both the QApplication default icon and the main shell icon. The Next
macOS target builds as a bundle with the existing ICNS and matching
`MACOSX_BUNDLE_ICON_FILE` metadata. The Windows target receives a minimal RC
icon resource using the existing ICO, without Legacy version metadata or a
subsystem change. Installer and update-package production remain in 8.4.2.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed for `ClassMngrNext` and `ClassMngrNextBootstrapTests`; focused
CTest passed `ClassMngrNextApplicationIconTests` (1/1). The test compares
application and shell icons against the packaged PNG. The built app bundle's
Info.plist names `app_icon.icns`, and the bundled ICNS matches the source asset
byte-for-byte. Windows RC target wiring was inspected but not compiled on this
macOS host. No installer/package build or full suite was run.

The Bootstrap fixture's exact startup-resource expectation now includes the
required icon PNG. Its existing equality assertion remains unchanged; the
focused `generatedCatalogStartsWithOnlySelectedStartupPayloads` test passed.

### 8.1.9 — Navigation and Teacher Profile font-family lookup (accepted)

Added a Qt-free Application port for the ordered available font-family list
and a Qt Platform adapter that queries `QFontDatabase` on demand. Bootstrap
supplies the required port to Navigation and Teacher Profile. Matching policy
remains in the UI: Navigation uses the first exact case-insensitive Inter match
and preserves inherited fallback ordering; Teacher Profile uses the first
case-insensitive substring match for Inter/Pretendard and retains inherited
families when no match exists. Font sizes, styles, and widget assignments are
preserved. ResourceLoader's font registration is unchanged.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed for `ClassMngrNext`, Navigation tests, Bootstrap tests, and the
Teacher Profile visual-capture target. Independent CTest verification passed
Navigation, Document Catalog Navigation, Document Viewer, Shared Staff
Directory, Application Icon, and the full `ClassMngrNextBootstrapTests`
target (6/6 targets; 89 Bootstrap cases passed). Deterministic coverage checks
exact versus substring matching, list order, deduplication, missing-family
fallback, Unicode conversion, all four Navigation sizes/styles, and the
intended Teacher Profile widgets. Offscreen English/light and Korean/dark
captures passed runtime assertions; two Next captures were visually reviewed.
The normal capture launch required offscreen mode because no display was
available. No repository-wide suite was run.

### 8.2.1 — Selected-document QtPdf session (accepted)

`QtPdfViewerSession` now owns the active `QPdfDocument`, `QPdfView`, and
borrowed source resource. `DocumentViewerPage` retains application session
state and controls. Replacement, close, suspension, page release, malformed
PDF handling, stale status suppression, close-timeout teardown, and reopen
are covered. Generated and print-output PDFs remain outside this viewer
session and continue through their operation paths.

On Darwin 27 arm64 in the Qt offscreen Debug configuration, the incremental
build in `build/macos-clang-debug` passed for five targets:
`ClassMngrNextDocumentCatalogNavigationTests`,
`ClassMngrNextDocumentViewerTests`,
`ClassMngrNextDocumentViewerVisualCapture`,
`ClassMngrNextPageHostTests`, and
`ClassMngrNextPlatformQtPdfSessionReleaseTests`. Independent CTest
verification passed all six focused checks: Document Viewer, QtPdf session
owner, visual capture, startup/workspace dependency boundary, PageHost, and
catalog navigation. The requested `ClassMngrNextDependencyBoundaryTests`
name is not registered; `NextStartupWorkspaceDependencyBoundary` was verified
with `ctest -N -V` and used. Ninja reported `premature end of file; recovering`
and completed with exit code 0 without compile or link failures. Visual
captures were generated and English/light Ready and Korean/dark Closed states
were reviewed; no pixel-parity claim against the older baseline was made.

The synchronous print borrow rejects reentrant replacement or release; its
owner/page must remain alive until the callback returns. The production path
uses a modal print dialog, so destruction from inside that callback is outside
the supported contract. The native macOS print dialog was not opened, and no
repository-wide suite was run.

### 8.2.2 — PDF/print operation adapter (accepted)

Added the Qt Platform `PdfPrintOperation` with separate borrowed-ready-document
and ordered local-batch operations. `DocumentViewerPage` invokes it inside the
viewer session's synchronous borrow; the viewer remains sole owner of its active
document and stream. The adapter delegates to shared Qt printing mechanics.
Shared batch printing validates and releases each document before the dialog,
stores page counts, determines orientation from the selected range, then
reopens and renders documents serially into one print job. Schedule, Roster,
Sub Prep, and Speaking Evaluation migrations remain in 8.2.3–8.2.6.

Focused verification passed the incremental build
`cmake --build build/macos-clang-debug --target ClassMngrNext ClassMngrQtPdfPrint ClassMngrNextDocumentViewerTests -j 8`, despite Ninja reporting
`premature end of file; recovering` (exit 0, no compile or link failures).
`ctest --test-dir build/macos-clang-debug -R '^(ClassMngrNextDocumentViewerTests|ClassMngrNextPlatformQtPdfSessionReleaseTests|NextStartupWorkspaceDependencyBoundary)$' --output-on-failure`
passed 3/3; `QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1
./build/macos-clang-debug/ClassMngrNextDocumentViewerTests -v1` passed 17/17.
`git diff --check` passed. Coverage verifies source order, global page ranges,
orientation, release before dialog, at most one ready document, PDF output
reload, pre-dialog missing/corrupt errors, and cancellation cleanup. No full
suite was run.

The production success path of the new Qt adapter was source-reviewed but not
runtime-driven because it opens printer UI. No native macOS print dialog,
physical print, or Linux/Windows runtime was exercised. Page-image lifetime was
code-inspected, not instrumented. Batch inputs are reopened after the modal
dialog, so changed contents with the same page count could print, and deleted
or inaccessible inputs can fail after acceptance; current callers use
operation-scoped temporary outputs.

### 8.2.3 — Schedule output (accepted)

Extracted the existing schedule PDF layout and writer into
`SchedulePdfRenderer`. Legacy `SchedulePrintService` remains the compatibility
wrapper with its existing print flow; Next Schedule output calls the renderer
directly and uses 8.2.2 `PdfPrintOperation` for printing. The Next target no
longer links `ClassMngrSchedulePrint`, and the dependency-boundary CTest passed.
Request, projection, dialogs, style/theme/font/orientation/path semantics are
preserved. Save As does not invoke a printer. Print creates a temporary PDF
and synchronously borrows a stack-owned ready document to the print operation,
releasing it when the operation returns.

The independent Tester incrementally built `ClassMngrSchedulePdfRenderer`,
`ClassMngrSchedulePrintPdfTests`, `ClassMngrNext`,
`ClassMngrNextScheduleOutputAdapterTests`,
`ClassMngrNextNormalScheduleImportIntegrationTests`, and
`ClassMngrNextSchedulePageTests` in `build/macos-clang-debug` with `-j4`
(exit 0). Focused CTest passed 5/5 for those four test suites and
`NextStartupWorkspaceDependencyBoundary`. QtTest counts were Legacy PDF 10,
Schedule page 13, adapter 5, and integration 18. `git diff --check` passed;
no full suite was run.

On the controlled macOS/Qt 6.12 fallback-font host, production Next Save As
matched the frozen `legacy-page-1.png` raster exactly at 1754×1240: zero
changed pixels and zero mean absolute channel difference (SHA-256
`eff64821b32270a3d9803f79ef2db869f6a63d3729f080ae2fd6406d6a53c175`). Fresh
PDFs each loaded as one A4 landscape page (842×595 points) and contained Alice
and E6-Helios/Poseidon/Gaia/Hera; their bytes differ from the archived PDF.
This establishes fixture raster parity, not PDF-byte identity or all-platform
parity. `Sans Serif` was unavailable and the host fallback font was used. No
native print dialog, physical print, Windows/Linux runtime, or full repository
suite was exercised.

### 8.2.4 — Roster output (accepted)

Roster Save As now sends a `GeneratedPdf` request through the existing
`FileDialogPort`-shaped boundary and writes directly through the atomic
`savePdf` adapter. Print uses the injected `PdfPrintOperation` with a nonempty
Ready borrowed PDF, preserving the title, options, and orientation. The
temporary PDF and document session are local to Print and retained only through
Preview; Preview Save and Print reuse that same PDF. Direct `QFileDialog` and
`PdfPrintService` fallbacks are absent. The existing truncation policy and all
four Next/Legacy template parity cases remain.

Verification used a fresh configure in the standard
`build/macos-clang-debug` folder on macOS with AppleClang 21, Ninja, Qt 6.12,
Debug, arm64+x86_64, deployment target 14.4, and `BUILD_TESTING=ON`.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextRosterOutputTests --parallel 4
```

The build exited 0. Focused CTest passed 2/2:

```sh
ctest --test-dir build/macos-clang-debug -R 'ClassMngrNextRosterOutputTests|NextStartupWorkspaceDependencyBoundary' --output-on-failure
```

The verbose Roster suite passed 18 tests with none failed or skipped, and all
four template parity rows passed. Offscreen Qt emitted the expected size-hint
and `Sans Serif` fallback warnings. Native printer UI and physical printing
were not exercised.

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

## Heavy-route requirements

- For every Phase 8 slice, use the heavy route: move the behavior behind its
  narrow adapter boundary, verify the full output or platform workflow, and
  remove temporary direct-to-legacy paths after acceptance.
- Do not duplicate business rules in platform implementations.
- Do not leave PowerPoint, QtPdf, or PDF output objects owned by widgets
  indefinitely; active viewer documents are session-scoped.
- Do not make resource updates a hidden part of application updates.
- Do not rewrite output formats and UI behavior simultaneously without parity fixtures.
