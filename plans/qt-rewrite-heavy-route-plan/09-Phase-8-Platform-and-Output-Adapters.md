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
- Last updated: 2026-11-05
- Current note: Windows x64/ARM64 Release package verification now passes at `53aaeed2a4b1f1123a12a4f365696968c2b5cc80`. Phase 8 remains in progress: its exit gate still requires all v2 external integrations and generated-output baseline parity on every supported platform.

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

### 8.2.5 — Sub Prep output (accepted)

The page query result now lives only through mapping, and the mapped request
is retained through Sub Prep PDF rendering. Next supplies a neutral package
callback that Bootstrap maps to ordered `PdfPrintOperation` and
`ExternalUrlLaunchPort` operations. The shared service no longer references
Next Platform; Legacy direct output behavior remains.

The standard `build/macos-clang-debug` build passed for
`ClassMngrSubPrepPackageServiceTests`,
`ClassMngrNextSubPrepPageTests`, `ClassMngrStartupPerformanceTests`, and
`ClassMngrNextSubPrepSqlParityCapture` (exit 0; duplicate-library linker
warnings only). Focused CTest passed 3/3. Legacy
`capturesMatchedSqlSubPrepForC53` passed 3/3 in 11.573s against the SQL oracle
of 750 students and 30 headers. The Next matched-SQL capture passed for 12
Sub Prep and 5 roster pages (17 total), with matching dimensions, zero changed
pixels, and SQL text/font/hash gates passing. The strict Next audit covered 5
pages and 15 page-font records with zero unmapped or control codes; the Legacy
audit diagnostic showed expected control-code mappings. C53 artifacts were
written under dedicated `/private/tmp`; no PDF byte-identity claim is made.

No memory lifecycle verification was run. Separate Sub Prep memory acceptance
remains open under the memory plan.

### 8.2.6 — Speaking Evaluation output (accepted)

The shared print callback now accepts the parent widget, ordered staged-file
paths, and a singular/plural title. An empty callback preserves Legacy's
`PdfPrintService` path. Next installs a `PdfPrintOperation` callback, maps
Sent/Canceled/Failed results, and uses portrait, A4, and fit-to-page settings.
Staged files remain available through synchronous printing and are then
cleaned up. The bounded 100-report route is preserved.

In the standard macOS Debug configuration, builds passed for
`ClassMngrSpeakingEvalBatchReportServiceTests`,
`ClassMngrNextSpeakingEvaluationPageTests`, and
`ClassMngrNextBootstrapTests` (exit 0). Focused CTest, including
`NextStartupWorkspaceDependencyBoundary`, passed 4/4. QtTest results were 84
service cases passed with 5 gated PowerPoint skips, 33 Next page cases passed,
and 89 Bootstrap cases passed. Standard and advanced PDF-widget raster
comparisons and the maximum-row test passed. Duplicate-library, offscreen, and
font warnings were the only reported warnings. No native printer UI or
physical printing was exercised, and no Next/Legacy report-output parity
claim was measured.

The Save As chooser, output-folder chooser, and desktop folder-open paths
remain unchanged as separate dialog/URL integration work for later discovery.

### 8.3.1 — PowerPoint automation adapter (accepted)

The shared report engine now depends on a neutral PowerPoint automation port.
The Qt host adapter owns availability checks, process/workspace handling,
script execution, and cleanup; Legacy's wrapper and Next Bootstrap compose it,
and Next injects the port. macOS build repairs moved the workspace constructor
out of line for incomplete `QTemporaryDir` and made pure script generators
compile on all hosts without changing script contents.

The focused build passed for `ClassMngrSpeakingEvalBatchReportServiceTests`,
`ClassMngrNextSpeakingEvaluationPageTests`,
`ClassMngrNextBootstrapTests`, and `ClassMngrPowerPointProcessFixture` using
`-j4`. Focused CTest passed 5/5 in 38.69 seconds: service 109 passed, 0
failed, 5 skipped; mixed-template 3/0/0; Bootstrap 89/0/0; Next page 40/0/0;
dependency boundary 1/1. Four Office integration cases and one generated-assets
case were skipped because the integration gate was unset. The standalone
process fixture was up to date; runner behavior cases ran through the Legacy
service suite. Native PowerPoint automation was not exercised.

### 8.4.1 — Update operations ports (accepted)

Added Qt-free Next Application contracts for download events and cleanup,
package actions, and the application artifact value. Qt adapters wrap the
existing downloader and installer, preserving hash/size validation,
resume/reuse, cleanup, and process actions. The shared dialog no longer
references `UpdateDownloader`, `UpdateInstaller`, or `Q_OS_LINUX`; Next
`UpdateCoordinator` and Legacy `UpdateController` inject the ports, including
cleanup policy and current artifact. `ClassMngrUpdateOperations` owns
downloader/installer sources, `ClassMngrNextUpdateAdapters` links it directly,
and `ClassMngrUpdatePresentation` owns only dialog sources.

The final build passed for `ClassMngrUpdateOperations`,
`ClassMngrNextUpdateAdapters`, `ClassMngrUpdatePresentation`, `ClassMngr`,
`ClassMngrNext`, `ClassMngrNextBootstrapTests`,
`ClassMngrNextUpdateCoordinatorTests`, `ClassMngrUpdaterTests`, and
`ClassMngrUpdateControllerAutomaticStartupTests`. Focused CTest passed 4/4 in
24.01s: Bootstrap, startup/workspace dependency boundary, Next coordinator,
and shared update presentation. The boundary report confirms the direct
Update Operations dependency and unique source ownership. The download-failure
fake-port recheck passed 3 QtTest cases and coordinator CTest 1/1. Offline
selected updater cases passed 15/15; selected Legacy controller port/data
cases passed 11/11; unconfigured-startup cases passed 3/3.

The full updater and Legacy controller suites were not run because their HTTP
fixtures fail at `QTcpServer::listen()` before reaching tested behavior. Real
installer/folder actions were not invoked, and the Linux branch was not
runtime-tested on macOS.

### 8.4.2 — Next application packaging (accepted)

The Next app package replaces contents under the existing app/update asset
identities; it does not add a separate resource update asset. Independent
package verifier tests passed 11/11, covering manifest tree/hash integrity,
resource catalog and direct hashes, Qt plugin/QML runtime, licenses/SBOMs, and
absence of Legacy `.rcc` payloads. The macOS staged package verifier covered
885 paths, 466 SHA-256 checks, 187 direct resource hashes, and 21 license/SBOM
files. Its universal executable contains arm64+x86_64 with minimum macOS 14.4;
ad-hoc `codesign --verify --deep --strict` passed. The
`ClassMngr-0.19.0-macos-universal.dmg` in
`/private/tmp/classmngr-phase842-build/dmg-retry/` was created and verified
with `hdiutil` and its checksum sidecar (SHA-256
`a5e34bfc3c8d6d7f36706fb9e3cf2254f7b5608a098210246118f78f6396ee8c`). The
package executor mounted it read-only, verified the mounted tree, and detached.
An independent mount was not repeated: `hdiutil` attach failed with “Device
not configured”; the tester confirmed counts from the executor's prior report.

Windows release wiring stages the matching VS 2022 v143 redist outside the app
package, validates Authenticode and PE architecture, passes it to Inno, and
checks the embedded hash. The Inno helper checks x64 and ARM64 payloads, x64
installed Next startup stages, artifacts, and checksums. Eight synthetic CMake
ON/OFF x64/ARM64 cases passed; actual Windows PowerShell, Inno, and runtime
execution remain CI-only. Linux workflow extracts and verifies the final
tarball; YAML and extracted shell syntax passed, while package execution remains
CI-only.

Publisher tag/version, asset filenames, and the eight-entry/eight-checksum
contract are unchanged. The macOS signature is ad hoc; no Developer ID signing
or notarization was performed.

### 8.5.1 — Calendar view adapter (accepted)

Extracted an injectable `CalendarView` interface and QML factory. `CalendarPage`
still creates its model and providers lazily on entry, releases the view before
its borrowed providers, and discards partial state after a failed load so a
later activation can retry. The QML component and compact `CalendarEventModel`
contract are unchanged.

Incremental macOS builds passed for `ClassMngr`,
`ClassMngrNextCalendarPageTests`, and `ClassMngrNextMyWorkspacePageTests`.
Focused CTest execution passed both affected suites:
`ctest --test-dir build/macos-clang-debug --output-on-failure -R 'ClassMngrNext(CalendarPage|MyWorkspacePage)Tests'`.
English and Korean calendar captures in light and dark palettes were generated
and visually reviewed. No full test suite was run.

### 8.6.1 — Transfer export codec (accepted)

The mapped/staged reader was audited and left intact. Both `saveFile` overloads
now share bounded incremental JSON output with an 8 KiB buffer and Qt-escaped
1,024 UTF-16-unit chunks. A count-only preflight enforces the inclusive 16 MiB
cap before writing, preserving validation error order and the single timestamp.

Incremental macOS builds passed for `ClassMngrClassTransferSupport` and
`ClassMngrClassTransferExportCodecParityTests`; focused CTest passed 1/1:
`ctest --test-dir build/macos-clang-debug --output-on-failure -R '^ClassMngrClassTransferExportCodecParityTests$'`.
Coverage includes exact Qt-indented byte parity for Legacy and portable output
with escaped, control, and Unicode strings; exact-cap acceptance; over-cap
destination preservation; and mapped-reader behavior. No full suite or memory
measurement was run; Phase 9 retains the memory gate.

### 8.6.2 — Transfer apply workflow ownership (accepted)

The page and `ClassTransferApplyCommand` now share one immutable Portable
package handle, removing the deep command copy; Legacy conversion takes a const
reference. Retryable apply errors retain the same handle. Success, cancellation,
invalid replacement, and workspace change release it, and cleanup frees review
row-vector capacity.

Incremental macOS production and test builds passed in the standard folder:
`cmake --build build/macos-clang-debug --target ClassMngr ClassMngrNext --parallel 4`
and `cmake --build build/macos-clang-debug --target ClassMngrNextClassTransferWorkflowTests ClassMngrNextApplicationClassTransferApplyUseCaseTests ClassMngrClassTransferApplyPackageAdapterTests ClassMngrNextPlatformApplicationServicesClassTransferApplyPortTests --parallel 4`.
Focused CTest passed 4/4 in 6.47 seconds, including weak-handle identity,
retry/success/cancel release, and null-handle validation coverage. The initial
platform suite exposed a stale assertion expecting 25 rows; commit `4b5f2de9`
updated storage to `MaximumRowCount=100`, and the assertion now checks 100 rows
with bounds 0–99 while retaining wide-ID and retained-marker checks. No full
suite or memory measurement was run; Phase 9 owns the memory gate.

### 8.7.1 — Windows redistributable staging

Package run `41794662707` failed on both x64 and ARM64 at Visual Studio 2022
discovery, before package configure or build. The same-SHA rerun (attempt 2)
failed at the same point. The repair records stdout, stderr, and exit status,
and documents a narrow VS 2022 Enterprise fallback requiring the v143 tree
and requested architecture payload. Existing Authenticode, signer, PE
architecture, and staged-package checks are preserved.

Static review accepted the candidate for commit; this does not establish a
runtime pass. Ruby YAML parsing and `git diff --check` passed; PowerShell and
actionlint were unavailable. The full Windows package workflow remains pending
after the batch push. This entry does not establish the Phase 8 exit gate or
Phase 9 readiness.

### 8.7.2 — macOS/Linux package triggers

The macOS and Linux release workflows now include `Qt-Rewrite` alongside
`main` under `push.branches`. Path filters, `workflow_dispatch`,
`workflow_call`, runners, package steps, and permissions are unchanged.

Ruby Psych parsed both YAML files; removing the added branch line from each
makes it byte-for-byte identical to `HEAD`. `git diff --check` passed;
`actionlint` was unavailable. Package CI results remain pending after the
batch push, and Phase 9 remains gated.

### 8.7.3 — Windows redistributable architecture selection

At pushed SHA `c6fcae8e`, Windows run `41799924542` failed on x64 and ARM64
after `vswhere` succeeded; its logs omitted candidate path and machine. The
workflow now logs each matching candidate's path, version, and PE machine,
then selects the highest-version candidate only after successful `dumpbin`
inspection confirms the target machine. No-match diagnostics are included.
Authenticode validity and Microsoft-signer checks, staged signature/hash
checks, and installer/package checks remain in place.

Static Tester accepted the change. Ruby YAML parsing and `git diff --check`
passed; PowerShell was unavailable, and Windows runtime CI is pending. The
Phase 8 package gate remains blocked by missing Qt license notices on Linux
and macOS (runs `41799924350` and `41799924549`); Phase 9 remains gated.

### 8.7.4 — Qt license staging

CMake now stages three checked-in, verbatim Qt 6.12.0 license texts from
official `qt/qtbase` tag `v6.12.0`: `LICENSES/LGPL-3.0-only.txt` (blob
`65c5ca88a67c30becee01c5a8816d964b03862f9`),
`LICENSES/GPL-3.0-only.txt` (blob
`94a9ed024d3859793618152ea559a168bbcbb5e2`), and
`LICENSES/Qt-GPL-exception-1.0.txt` (blob
`d0322bf0e4e1d484d35cc6a1d96fa2b65b9d579b`). Provenance is recorded in
`licenses/qt/UPSTREAM.md`; the package inventory lists LGPL, GPL, and the Qt
exception separately. Installed Qt SBOM staging and validation remain.

The focused `python3 scripts/test_verify_next_package.py` verifier passed
11/11, and the static Tester accepted the contract. The upstream exception
text retains its one blank line at EOF to preserve the source hash;
`git diff --check` reports that line when checking the untracked text file,
while tracked code changes are clean. Full Windows, macOS, and Linux package
CI remains pending, and Phase 9 remains gated.

### 8.7.5 — Phase 8 Batch 4 package matrix

At SHA `7de5aa471c75bd274e1102464b8f2f02c4f23e5b`, Windows run `41806105651`
failed on x64 and ARM64 because the selected architecture-named
`vc_redist.*.exe` files have x86 outer PE images; installer and package checks
did not run. Linux run `41806105416` failed during CMake configure/generate
with a strong dependency cycle involving
`RESCAN:{ClassMngrRuntime,ClassMngrWorkspaceStore}`, `ClassMngrSubPrepShared`,
and `ClassMngrNextCalendarUi`; no build or package verifier ran.

macOS run `41806105824` passed image verification and the mounted package
verifier (853 paths, 451 hashes, 187 direct-resource hashes, 22 license/SBOM
files, and 82 universal binaries). The combined disk-image step then exited 16
before focused verifier tests and artifact upload. `hdiutil detach` is the
likely failure point: it is the script command after the last successful
verifier output, but the logs do not prove this.

The Phase 8 package gate did not pass, and no artifact was uploaded for any
workflow. Phase 9 remains gated.

### 8.7.6 — Windows redistributable package validation

Windows run `41806105651` reported PE machine `x86` for both architecture-named
v143 redistributable installers (`14.44.35211.0`). This identifies the outer
installer stub and does not establish the runtime payload architecture. Source
review confirmed Microsoft's architecture-specific package naming, the
`ClassMngr.iss` mapping, and the verifier's exact embedded filename/hash. The
workflow now accepts only the matrix-derived filename under the VS2022 v143
tree and selects the newest valid version. Authenticode, Microsoft signer, and
staged-hash checks remain, as does the separate `ClassMngr.exe` PE check.

Static Tester accepted the change. Ruby YAML parsing and `git diff --check`
passed; PowerShell was unavailable, and Windows package CI is pending. The
Linux CMake cycle and macOS disk-image exit remain open, so Phase 9 stays gated.

### 8.7.7 — Linux CMake link-group cycle

The Linux dependency cycle is addressed by keeping the Runtime/WorkspaceStore
`RESCAN` group and moving WorkspaceStore, Sub Prep, and Calendar UI edges out of
`ClassMngrRuntime`'s public interface into the legacy runtime closure. The
no-RESCAN fallback and Apple test runtime retain their required dependencies.
Static review and `git diff --check` passed. This Darwin host has no Linux
release preset or `QT_LINUX_PREFIX`, so local Linux configure was unavailable;
Linux package configure/build CI remains pending. Phase 9 remains gated.

### 8.7.8 — macOS disk-image cleanup

After mounted-package verification succeeds, the release script retries
`hdiutil detach` status 16 up to three times and reports failures. If detach or
temporary-directory removal still fails, it warns and continues to checksum
generation; the EXIT trap retains one final best-effort detach attempt.
Verification failures remain fatal and preserve their exit status.

`bash -n` and `git diff --check` passed. A temporary harness covered retry
success, persistent detach failures, other detach errors, verification failure,
and directory-removal failure; checksum generation continued after cleanup
failures, and verification failure retained its original status. It did not
exercise a real macOS mount. The prior package run's exit 16 is still inferred
to be a busy detach, and macOS package CI remains pending. Phase 9 remains
gated.

### 8.7.9 — Linux package domain-model link closure

At pushed SHA `3808d5da9d4fa4c76a7df5e9d14d31a8234b45ed`, Linux run
`41813392564` configured successfully but failed linking `ClassMngrNext`:
`ClassMngrNextWorkspaceDomain` was scanned before Sub Prep and WorkspaceStore,
leaving `Classroom` and `Roster::BaseColumns` undefined. The correction puts the
existing Domain objects in one `ClassMngrDomainArchive`, shared privately by
WorkspaceStore, Runtime, and Apple TestRuntime; NextWorkspaceDomain now uses
that archive, and its exact dependency assertion is updated. Static review
accepted the target graph and source ownership. No local Linux build was run
on this Darwin host; Linux package CI must confirm link order and symbols. The
prior Windows and macOS runs were still in progress at the last check. Phase 9
remains gated.

### 8.7.10 — Linux package color-validation dependency

At SHA `76bba91f0bc65be0f4f09392ff8ec52c650178c5`, Linux rerun `41816907048`
resolved the Classroom/Roster symbols but exposed an undefined
`ColorUtils::canonicalHexColor` reference, whose implementation was owned by
the earlier ClassMngrClassTransferSupport archive. The function was extracted
to a separately owned `ClassMngrColorValidation` leaf target, privately linked
by ClassMngrDomainArchive and ClassMngrClassTransferSupport. Source ownership
and exact dependency-boundary assertions were updated; static review passed.
No local build or test was run. Package CI is pending, so Phase 9 remains
gated.

### 8.7.11 — Qt Concurrent package runtime artifact

At SHA `5cad12853814d8cd348b6f22ad3c9a788d5c3a2a`, Linux run `41820638403`
built and linked, but strict package verification failed because the package
did not contain Qt Concurrent. Windows run `41820638202` failed the same
verifier check for x64 and ARM64. The exact reason Qt's automatic deployment
omitted the library is unproven.

`cmake/next_resources.cmake` now installs the imported runtime artifact
`Qt6::Concurrent` in the Runtime component before the generated Qt deployment
script: Windows DLLs go to `${CMAKE_INSTALL_BINDIR}`, Linux libraries to
`${CMAKE_INSTALL_LIBDIR}`, and macOS dylibs/frameworks to
`ClassMngr.app/Contents/Frameworks`. Static review accepted the rule. No local
build or test was run. At the same SHA, macOS run `41820638665` passed the
disk-image build and package verification. Its checksum reported VALID; the
focused verifier checked 853 paths, 451 hashes, 187 direct-resource hashes,
and 22 license/SBOM files, then uploaded `ClassMngr-macos-universal` (DMG,
checksum, and JSON report). The package step passed and no cleanup failure
surfaced, though the log does not expose the explicit detach command or result.

At that revision, Linux and Windows still failed strict package verification
for missing Qt Concurrent. Fresh package CI for the explicit staging rule was
pending; the macOS run did not verify that rule. The Phase 8 package gate and
Phase 9 remained gated at that revision.

### 8.7.12 — Windows Release startup verifier and package gate

At `53aaeed2a4b1f1123a12a4f365696968c2b5cc80`, Windows Release run
`42111769908` passed final installer verification (step 22) for x64 and
ARM64. The x64 log confirms all ten installed startup stages in order:
`process-created`, `core-services-created`, `settings-loaded`,
`language-loaded`, `theme-loaded`, `required-fonts-loaded`,
`workspace-opened`, `shell-created`, `initial-page-ready`, and
`startup-ready`. Both architecture-specific
`WINDOWS_PACKAGE_VERIFICATION_SUCCESS` sentinels appeared. Final verification
logs (step 23) and release artifacts (step 25) uploaded for both architectures.
The verifier fix reads startup output asynchronously and makes the workflow
fail on verifier errors or a missing success sentinel. Windows Release package
verification is therefore accepted on both architectures.

Linux Release run `42104507218` passed its package-verification step (step 9)
and uploaded its artifact at ancestor SHA
`44e89448a1b463a1428de1d89f43a7cd445fdb27`; the workflow later failed at step
11 in a supplemental Phase 0 route-contract test because
`CLASSMNGR_STARTUP_SUB_PREP_OUTPUT_REFERENCE_DATE='2026-09-14'` was treated as
a confined path. macOS Release run `42104507137` passed at that same ancestor
SHA. These are not Linux/macOS reruns at the Windows verifier fix SHA. Separate
Phase 4 resource-install-layout run `42111769886` passed on Windows x64, Linux
x64, and macOS.

The local verifier invocation previously shown stopped at
`verify_next_package.ps1:32` because
`dist/checksums-windows-x64.txt` was absent; it did not reach extraction. The
CI results verify the uploaded Windows packages, but do not establish the
Phase 8 exit gate: all v2 external integrations must work without platform
code leaking into feature logic, and generated output must match the baseline
on every supported platform. Phase 7.5 remains In progress and does not block
Phase 8. The Linux supplemental route-contract failure is a separate follow-up.

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
